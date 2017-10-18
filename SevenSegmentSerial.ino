#include <SoftTimer.h>
#include <DelayRun.h>

/**
 * Reads 4 numbers or characters on serial (9600) to be display.
 * Decimal point can be used as an extra for turn on the decimal point on the prevoius segment.
 * List of available characters are seen below.
 * e.g.: 1.6E4
 * e.g.: StOP.
 * 
 * Special commands are:
 * *1 *2 - Blink rate. *0 must be called to disable blinking.
 * ! - Will clear the screen. If combined with text, will clear the screen after the last character.
 * $ - Align right. After this sign is set a blank screen will appear for a shor period.
 *     Characters provided before the $ sign may appear for a shor amount.
 *      cannot be combined.
 * 
 * Command can be combined (but not $ with !):
 * e.g.: *2E1! (! will clean the rest of the screen)
 * e.g.: Err8*7
 * e.g.: $Hi.*0
 * or even: A$b*2d.
 * 
 * You can write direct digits by providing the 8bit representation of the digits after a # sign.
 * e.g.: 2#4!
 */

// -- Pins
byte digitPins[] = {2, 3, 4, 5};
byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13};

// -- Global
byte digitSegments[4];
boolean screenOn = true;
boolean blinking = false;

// -- Function pointer definitions
void serialCheck(Task* me);
boolean serialTimeout(Task* me);
void draw(Task* me);
boolean turnOff(Task* task);
boolean turnOn(Task* task);

// -- Tasks
Task serialCheckTask(0, serialCheck);
DelayRun serialWatchdogTask(10, &serialTimeout);
Task drawTask(1, draw);
// -- This task will turn off the LED after 1 second.
DelayRun offTask(500, turnOff);
// -- This task will turn on the LED after 2 seconds.
// -- After the onTask we loop the offTask.
DelayRun onTask(500, turnOn, &offTask);

// -- Setup
void setup() {
  Serial.begin(9600);
  for(int i=0; i< sizeof(digitPins); i++) {
    pinMode(digitPins[i], OUTPUT);
  }
  for(int i=0; i< sizeof(segmentPins); i++) {
    pinMode(segmentPins[i], OUTPUT);
  }
  SoftTimer.add(&serialCheckTask);
  SoftTimer.add(&drawTask);
  offTask.followedBy = &onTask;
}

// -- Worker functions
byte pos = 0;
boolean blinkCommand = false;
boolean alignRightLater = false;
boolean digitFollows = false;
void serialCheck(Task* me) {
  while (Serial.available()) {
    // read the most recent byte (which will be from 0 to 255):
    char c = Serial.read();
    serialWatchdogTask.startDelayed();
    if (digitFollows && (pos < 4))
    {
      // -- Direct digit
      digitSegments[pos++] = c;
      digitFollows = false;
      continue;
    }
    if ((c < 32) && (c > 0)) {
      continue;
    }
    if (c == '#') {
      digitFollows = true;
      // -- Next character will be a direct digit
      continue;
    }
    if (c == '!') {
      // -- Clear the screen.
      for(;pos < 4; pos++)
      {
        digitSegments[pos] = 0;
      }
      break;
    }
    if (c == '$') {
      // -- Align right.
      alignRightLater = true;
      break;
    }
    if (c == '*') {
      blinkCommand = true;
      // -- Next character will be a number with a blink rate.
      continue;
    }
    if (blinkCommand) {
      // -- Previous character was a *
      if ((c >= '0') && (c <= '9')) {
        setBlinkRate(c - '0');
      }
      blinkCommand = false;
      continue;
    }
    if ((c == '.') && (pos > 0)) {
      digitSegments[pos-1] |= 0b10000000;
      continue;
    }
    if (pos < 4) {
      byte segment = charToSegment(c);
      Serial.println(segment, BIN);
      digitSegments[pos++] = segment;
    }
  }
}

boolean serialTimeout(Task* me) {
  if (alignRightLater) {
    alignRight();
    alignRightLater = false;
  }
  pos = 0;
  blinkCommand = false;
  digitFollows = false;
  return false;
}

byte currentDigit=0;
void draw(Task* me) {
  for(int i=0; i< sizeof(segmentPins); i++) {
      digitalWrite(segmentPins[i], HIGH);
  }
  if ((!screenOn) || alignRightLater) {
    return;
  }

  currentDigit += 1;
  if(currentDigit >= sizeof(digitPins)) {
    currentDigit = 0;
  }
  for(byte d=0; d < sizeof(digitPins); d++) {
    if(d == currentDigit) {
      digitalWrite(digitPins[d], 0);
    } else {
      digitalWrite(digitPins[d], 1);
    }
  }

  byte segment = digitSegments[currentDigit];
  for(int i=0; i< sizeof(segmentPins); i++) {
      byte pos = 1 << i;
      digitalWrite(segmentPins[i], !(segment & pos));
  }
}

boolean turnOff(Task* task) {
  screenOn = false;
  return true; // -- Return true to enable the "followedBy" task.
}
boolean turnOn(Task* task) {
  screenOn = true;
  return blinking; // -- Return true to enable the "followedBy" task.
}


// -- Helper methods
void setBlinkRate(byte value) {
  if (value == 0) {
    blinking = false;
    screenOn = true;
    return;
  }
  
  unsigned long ms = 1000L / value;
  offTask.delayMs = ms;
  onTask.delayMs = ms / 2; // -- Next task will be delayed by this amount.
  blinking = true;
  offTask.startDelayed();
}

void alignRight() {
  byte i = 1;
  while((pos - i) >= 0) {
    digitSegments[4-i] = digitSegments[pos - i];
Serial.print(pos-i);
Serial.print(" -> ");
Serial.println(4-i);
    i++;
  }
  while(i <= 4) {
    digitSegments[4-i] = 0;
Serial.print("x -> ");
Serial.println(4-i);
    i++;
  }
}

byte charToSegment(char c) {
  /**
   *   a
   *  f b
   *   g
   *  e c
   *   d
   */
  switch(c)
  {
    case '1': return 0x06;
    case '2': return 0x5b;
    case '3': return 0x4f;
    case '4': return 0x66;
    case '5': return 0x6d;
    case '6': return 0x7d;
    case '7': return 0x07;
    case '8': return 0x7f;
    case '9': return 0x6f;
    case '0': return 0x3f;
    //   a
    //  f b
    //   g
    //  e c
    //   d
    //                 gfedcba
    case '-': return 0b1000000;
    case '_': return 0b0001000;
    case ' ': return 0b0000000;
    case 'a':
    case 'A': return 0b1110111;
    case 'B':
    case 'b': return 0b1111100;
    case 'c': return 0b1011000;
    case 'C': return 0b0111001;
    case 'D':
    case 'd': return 0b1011110;
    case 'e':
    case 'E': return 0b1111001;
    case 'f':
    case 'F': return 0b1110001;
    case 'h': return 0b1110100;
    case 'H': return 0b1110110;
    case 'i': return 0b0010000;
    case 'I': return 0b0110000;
    case 'j':
    case 'J': return 0b0001110;
    case 'L': return 0b0111000;
    case 'l': return 0b0000110;
    case 'O': return 0b0111111;
    case 'o': return 0b1011100;
    case 'p':
    case 'P': return 0b1110011;
    case 'Q':
    case 'q': return 0b1100111;
    case 'r': return 0b1010000;
    case 's':
    case 'S': return 0b1101101;
    case 'T': return 0b0110001;
    case 't': return 0b1111000;
    case 'U': return 0b0111110;
    case 'u': return 0b0011100;
    case 'y':
    case 'Y': return 0b1110010;
    //   a
    //  f b
    //   g
    //  e c
    //   d
    //                 gfedcba
    case '=': return 0b1001000;
    case ',': return 0b0000100;
    case '[': return 0b0111001;
    case ']': return 0b0001111;
    case '}': return 0b1110000;
    case '{': return 0b1000110;
    case '"': return 0b0100010;
    case '\'': return 0b0000010;
    case 248: return 0b1100011;
    case 240: return 0b1001001;
    default: return 0;
  }
}

