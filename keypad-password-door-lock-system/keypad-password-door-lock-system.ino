/*
 Keypad Password Door Lock System
 
 
 Designed for Abdullah Daniel's Final Project
 
 Created 23 February 2018
 @Gorontalo, Indonesia
 by ZulNs
 */

#include <EEPROM.h>
#include <MultitapKeypad.h>
#include <LiquidCrystal_I2C.h>

#define DOOR_STATE_LOCKED       0
#define DOOR_STATE_UNLOCKED     1
#define DOOR_STATE_OPENED       2
#define UNLOCKED_PERIOD         5000
#define SLEEP_PERIOD            10000

#define DOOR_CLOSE_SWITCH_PIN   2     // Sense by INT0
#define DOOR_UNLOCK_SWITCH_PIN  3     // Sense by INT1
#define DOOR_LOCKER_PIN         4
#define BUZZER_PIN              5

#define ROW4  A0
#define ROW3  A1
#define ROW2  A2
#define ROW1  A3
#define COL4  9
#define COL3  10
#define COL2  11
#define COL1  12

#define CHR_BOUND        3
#define BACKSPACE        8
#define CLEARSCREEN      12
#define CARRIAGE_RETURN  13
#define CAPSLOCK_ON      17
#define CAPSLOCK_OFF     18
#define NUMLOCK_ON       19
#define NUMLOCK_OFF      20

// Multitap Symbols from '1' key
const char SYMBOL_1[] PROGMEM =
{
  '(', ')', '[', ']', '{',
  '}', '@', '$', CHR_BOUND
};

// Multitap Symbols from '*' key
const char SYMBOL_A[] PROGMEM =
{
  '/', '+', '-', '=', '%',
  '^', '_', CHR_BOUND
};

// Multitap Symbols from '#' key
const char SYMBOL_NS[] PROGMEM =
{
  ',', '.',  ';', ':', '!',
  '?', '\'', '"', CHR_BOUND
};

const char ALPHABET[] PROGMEM =
{
  'A', 'B', 'C', CHR_BOUND, CHR_BOUND,
  'D', 'E', 'F', CHR_BOUND, CHR_BOUND,
  'G', 'H', 'I', CHR_BOUND, CHR_BOUND,
  'J', 'K', 'L', CHR_BOUND, CHR_BOUND,
  'M', 'N', 'O', CHR_BOUND, CHR_BOUND,
  'P', 'Q', 'R', 'S',       CHR_BOUND,
  'T', 'U', 'V', CHR_BOUND, CHR_BOUND,
  'W', 'X', 'Y', 'Z',       CHR_BOUND
};

const int EEPROM_ADMIN_PASSWORD = EEPROM.length() - 48;
const int EEPROM_USER_PASSWORD  = EEPROM.length() - 32;
const int EEPROM_PHONE_NUMBER   = EEPROM.length() - 16;

// creates lcd as LiquidCrystal object
LiquidCrystal_I2C lcd(0x3f, 16, 2);
// creates kpd as MultitapKeypad object
// for matrix 4 x 3 keypad
// MultitapKeypad kpd(ROW0, ROW1, ROW2, ROW3, COL0, COL1, COL2);
// for matrix 4 x 4 keypad
MultitapKeypad kpd(ROW1, ROW2, ROW3, ROW4, COL1, COL2, COL3, COL4);
// creates key as Key object
Key key;

boolean isAlphaMode = true;
boolean isUpperCaseMode = true;
boolean isEndOfDisplay = false;
boolean isWaitingForSleep = false;
byte startCursorPos;
byte endCursorPos;
byte cursorPos;
byte chrCtr;
char strAdminPassword[16] = "admin";
char strUserPassword[16] = "user";
char strPhoneNumber[16] = "085395262767";
char strBuffer[16];

volatile boolean isInt0 = false;
volatile boolean isInt1 = false;
byte doorState;
long unlockedTimeout;
long sleepTimeout;

void setup() {
  if (EEPROM[EEPROM_ADMIN_PASSWORD] == 0xFF)
  {
    EEPROM.put(EEPROM_ADMIN_PASSWORD, strAdminPassword);
  }
  else
  {
    EEPROM.get(EEPROM_ADMIN_PASSWORD, strAdminPassword);
  }
  
  if (EEPROM[EEPROM_USER_PASSWORD] == 0xFF)
  {
    EEPROM.put(EEPROM_USER_PASSWORD, strUserPassword);
  }
  else
  {
    EEPROM.get(EEPROM_USER_PASSWORD, strUserPassword);
  }
  
  if (EEPROM[EEPROM_PHONE_NUMBER] == 0xFF)
  {
    EEPROM.put(EEPROM_PHONE_NUMBER, strPhoneNumber);
  }
  else
  {
    EEPROM.get(EEPROM_PHONE_NUMBER, strPhoneNumber);
  }
  
  kpd.attachFunction(pollingCheck);
  lcd.init();
  lcd.backlight();

  PORTD |= bit(DOOR_CLOSE_SWITCH_PIN) | bit(DOOR_UNLOCK_SWITCH_PIN); // sets PD2 & PD3 as input pull-up
  bitSet(DDRD, DOOR_LOCKER_PIN); // sets PD4 as output
  EICRA |= bit(ISC00) | bit(ISC10); // INT0 & INT1 set on change state
  
  lcd.print(F("Door "));
  if (bitRead(PIND, DOOR_CLOSE_SWITCH_PIN))
  {
    lcd.print(F("opened..."));
    doorState = DOOR_STATE_OPENED;
    enableInt0();
    doorOpenedTone();
    enableSleepLCD();
  }
  else
  {
    lcd.print(F("locked..."));
    doorState = DOOR_STATE_LOCKED;
    enableInt0();
    enableInt1();
    doorLockedTone();
  }
}

void loop()
{
  if (isWaitingForSleep && millis() >= sleepTimeout)
  {
    sleepLCD();
  }

  if (doorState == DOOR_STATE_LOCKED)
  {
    if (getPassword())
    {
      unlockDoor();
    }
    else
    {
      if (!kpd.isCanceled)
      {
        lcd.clear();
        lcd.print(F("Wrong password!!"));
        byte ctr = 20;
        while (ctr > 0 && !isInt0 && !isInt1)
        {
          doorBreakingTone();
          ctr--;
        }
      }
      
      if (kpd.isCanceled && !isInt0 && !isInt1)
      {
        sleepLCD();
      }
      
      if (isInt0)
      {
        disableInt0();
        if (!ensureSwitchState(DOOR_CLOSE_SWITCH_PIN))
        {
          enableInt0();
          return;
        }
        disableInt1();
        lcd.clear();
        lcd.print(F("Door breaking"));
        lcd.setCursor(0, 1);
        lcd.print(F("detected!!!"));
        while (true)
        {
          doorBreakingTone();
        }
      }
      
      if (isInt1)
      {
        disableInt1();
        if (ensureSwitchState(DOOR_UNLOCK_SWITCH_PIN))
        {
          enableInt1();
          return;
        }
        unlockDoor();
      }
    }
  }
  
  else if (doorState == DOOR_STATE_UNLOCKED)
  {
    if (millis() >= unlockedTimeout)
    {
      bitClear(PORTD, DOOR_LOCKER_PIN);
      doorState = DOOR_STATE_LOCKED;
      enableInt1();
      lcd.clear();
      lcd.print(F("Door locked..."));
      doorLockedTone();
      enableSleepLCD();
      return;
    }
    
    if (isInt0)
    {
      disableInt0();
      if (!ensureSwitchState(DOOR_CLOSE_SWITCH_PIN))
      {
        enableInt0();
        return;
      }
      bitClear(PORTD, DOOR_LOCKER_PIN);
      doorState = DOOR_STATE_OPENED;
      enableInt0();
      lcd.clear();
      lcd.print(F("Door opened..."));
      doorOpenedTone();
      enableSleepLCD();
    }
  }

  else if (doorState == DOOR_STATE_OPENED)
  {
    if (isInt0)
    {
      disableInt0();
      if (ensureSwitchState(DOOR_CLOSE_SWITCH_PIN))
      {
        enableInt0();
        return;
      }
      doorState = DOOR_STATE_LOCKED;
      enableInt0();
      enableInt1();
      lcd.clear();
      lcd.print(F("Door locked..."));
      doorLockedTone();
      enableSleepLCD();
    }
  }
}

void unlockDoor()
{
  bitSet(PORTD, DOOR_LOCKER_PIN);
  doorState = DOOR_STATE_UNLOCKED;
  unlockedTimeout = millis() + UNLOCKED_PERIOD;
  lcd.clear();
  lcd.print(F("Door unlocked..."));
  doorUnlockedTone();
  isWaitingForSleep = false;
}

void pollingCheck()
{
  if (isInt0 || isInt1)
  {
    kpd.isCanceled = true;
  }
  if (isWaitingForSleep && millis() >= sleepTimeout)
  {
    kpd.isCanceled = true;
  }
}

ISR(INT0_vect)
{
  isInt0 = true;
}

ISR(INT1_vect)
{
  isInt1 = true;
}

void enableInt0()
{
  cli();
  bitSet(EIFR, INTF0); // clears any outstanding INT0 interrupt
  bitSet(EIMSK, INT0); // enables INT0 interrupt
  sei();
}

void disableInt0()
{
  cli();
  bitClear(EIMSK, INT0); // disables INT0 interrupt
  sei();
  isInt0 = false;
}

void enableInt1()
{
  cli();
  bitSet(EIFR, INTF1); // clears any outstanding INT1 interrupt
  bitSet(EIMSK, INT1); // enables INT1 interrupt
  sei();
}

void disableInt1()
{
  cli();
  bitClear(EIMSK, INT1); // disables INT1 interrupt
  sei();
  isInt1 = false;
}

void enableSleepLCD()
{
  sleepTimeout = millis() + SLEEP_PERIOD;
  isWaitingForSleep = true;
}

void sleepLCD()
{
  lcd.noBacklight();
  lcd.clear();
  lcd.noCursor();
  lcd.noBlink();
  isWaitingForSleep = false;
  while (true)
  {
    key = kpd.getKey();
    if (key.state == KEY_UP || key.state == CANCELED)
    {
      break;
    }
  }
  lcd.backlight();
  enableSleepLCD();
}

boolean ensureSwitchState(byte switchPin)
{
  boolean state0 = bitRead(PIND, switchPin);
  boolean state1;
  while (true)
  {
    delay(50);
    state1 = bitRead(PIND, switchPin);
    if (state0 == state1);
    {
      return state0;
    }
    state0 = state1;
  }
}

void doorBreakingTone()
{
  float sinVal;
  int toneVal;
  for (byte i = 0; i < 180; i++)
  {
    sinVal = sin(i * 3.14159 / 180);
    toneVal = 2000 + int(sinVal * 2000);
    tone(BUZZER_PIN, toneVal);
    delay(2);
    if (i == 90)
    {
      lcd.noBacklight();
    }
  }
  noTone(BUZZER_PIN);
  lcd.backlight();
}

void doorOpenedTone()
{
  for (int i = 1700; i < 1944; i *= 1.01)
  {
    tone(BUZZER_PIN, i);
    delay(30);
  }
  noTone(BUZZER_PIN);
  delay(100);
  for (int i = 1944; i > 1808; i *= 0.99)
  {
    tone(BUZZER_PIN, i);
    delay(30);
  }
  noTone(BUZZER_PIN);
}

void doorLockedTone()
{
  for (int i = 1000; i < 2000; i *= 1.02)
  {
    tone(BUZZER_PIN, i);
    delay(10);
  }
  for (int i = 2000; i > 1000; i *= 0.98)
  {
    tone(BUZZER_PIN, i);
    delay(10);
  }
  noTone(BUZZER_PIN);
}

void doorUnlockedTone()
{
  tone(BUZZER_PIN, 1568);
  delay(200);
  tone(BUZZER_PIN, 1318);
  delay(200);
  tone(BUZZER_PIN, 1046, 200);
}

void wrongPasswordTone()
{
  tone(BUZZER_PIN, 1046);
  delay(200);
  tone(BUZZER_PIN, 1318);
  delay(200);
  tone(BUZZER_PIN, 1568, 200);
}

boolean getPassword()
{
  enableSleepLCD();
  byte ctr = 3;
  while (ctr > 0)
  {
    lcd.clear();
    lcd.print(F("Password? #"));
    lcd.print(ctr);
    lcd.setCursor(0, 1);
    if (!getString() && kpd.isCanceled)
    {
      return false;
    }
    if (key.character == 'C')
    {
      continue;
    }
    if (strcmp(strUserPassword, strBuffer) == 0)
    {
      return true;
    }
    if (strcmp(strAdminPassword, strBuffer) == 0)
    {
      if (adminAccess())
      {
        return true;
      }
      if (kpd.isCanceled)
      {
        return false;
      }
      ctr = 3;
      continue;
    }
    ctr--;
    lcd.clear();
    lcd.print(F("Wrong password!"));
    if (ctr > 0 && !getAKey())
    {
      return false;
    }
  }
  return false;
}

boolean adminAccess()
{
sub_menu_1:
  lcd.clear();
  lcd.print("1.Unlock");
  lcd.setCursor(0, 1);
  lcd.print("2.Setting");
  while (true)
  {
    if (!getAKey())
    {
      return false;
    }
    if (key.character == '1' || key.character == '2' || key.character == 'B' || key.character == 'C')
    {
      tone(BUZZER_PIN, 5000, 20);
      break;
    }
    else
    {
      tone(BUZZER_PIN, 4000, 100);
    }
  }
  if (key.character == 'B' || key.character == 'C')
  {
    return false;
  }
  else if (key.character == '1')
  {
    return true;
  }
  
sub_menu_2:
  lcd.clear();
  lcd.print("1.Change number");
  lcd.setCursor(0, 1);
  lcd.print("2.Change pswd");
  while (true)
  {
    if (!getAKey())
    {
      return false;
    }
    if (key.character == '1' || key.character == '2' || key.character == 'B' || key.character == 'C')
    {
      tone(BUZZER_PIN, 5000, 20);
      break;
    }
    else
    {
      tone(BUZZER_PIN, 4000, 100);
    }
  }
  if (key.character == 'B')
  {
    goto sub_menu_1;
  }
  else if (key.character == 'C')
  {
    return false;
  }
  else if (key.character == '1')
  {
    if(!changeString(strPhoneNumber) && key.character == 'B')
    {
      goto sub_menu_2;
    }
    return false;
  }
  
sub_menu_3:
  lcd.clear();
  lcd.print("1.Admin pswd");
  lcd.setCursor(0, 1);
  lcd.print("2.User pswd");
  while (true)
  {
    if (!getAKey())
    {
      return false;
    }
    if (key.character == '1' || key.character == '2' || key.character == 'B' || key.character == 'C')
    {
      tone(BUZZER_PIN, 5000, 20);
      break;
    }
    else
    {
      tone(BUZZER_PIN, 4000, 100);
    }
  }
  if (key.character == 'B')
  {
    goto sub_menu_2;
  }
  else if (key.character == 'C')
  {
    return false;
  }
  else if (key.character == '1')
  {
    if (!changeString(strAdminPassword) && key.character == 'B')
    {
      goto sub_menu_3;
    }
    return false;
  }
  if (!changeString(strUserPassword) && key.character == 'B')
  {
    goto sub_menu_3;
  }
  return false;
}

boolean changeString(char *strDestination)
{
  boolean oldIsAlphaMode = isAlphaMode;
  boolean oldIsUpperCaseMode = isUpperCaseMode;
  lcd.clear();
  if (strDestination == strPhoneNumber)
  {
    lcd.print(F("Current number:"));
  }
  else if (strDestination == strAdminPassword)
  {
    lcd.print(F("Cur admin pswd:"));
  }
  else if (strDestination == strUserPassword)
  {
    lcd.print(F("Cur user pswd:"));
  }
  lcd.setCursor(0, 1);
  lcd.print(strDestination);
  if (!getAKey())
  {
    return false;
  }
  if (key.character == 'B' || key.character == 'C')
  {
    tone(BUZZER_PIN, 5000, 20);
    return false;
  }
  lcd.clear();
  if (strDestination == strPhoneNumber)
  {
    lcd.print(F("New number?"));
    isAlphaMode = false;
  }
  else if (strDestination == strAdminPassword)
  {
    lcd.print(F("New adm pwd?"));
    isAlphaMode = true;
  }
  else if (strDestination == strUserPassword)
  {
    lcd.print(F("New usr pwd?"));
    isAlphaMode = true;
  }
  boolean gs = getString();
  isAlphaMode = oldIsAlphaMode;
  isUpperCaseMode = oldIsUpperCaseMode;
  if (!gs || key.character == 'C')
  {
    return false;
  }
  strcpy(strDestination, strBuffer);
  lcd.clear();
  if (strDestination == strPhoneNumber)
  {
    if (!saveToEEPROM(EEPROM_PHONE_NUMBER, strDestination))
    {
      return false;
    }
    lcd.print(F("Phone number"));
  }
  else if (strDestination == strAdminPassword)
  {
    if (!saveToEEPROM(EEPROM_ADMIN_PASSWORD, strDestination))
    {
      return false;
    }
    lcd.print(F("Admin password"));
  }
  else if (strDestination == strUserPassword)
  {
    if (!saveToEEPROM(EEPROM_USER_PASSWORD, strDestination))
    {
      return false;
    }
    lcd.print(F("User password"));
  }
  lcd.setCursor(0, 1);
  lcd.print(F("changed..."));
  getAKey();
  return true;
}

boolean saveToEEPROM(int address, char *str)
{
  //EEPROM.put(address, str);
  
  // ZulNs: I don't know why above
  // code didn't working, instead
  // I use following code which
  // write one by one byte to EEPROM.
  for (byte i = 0; i < 16; i++)
  {
    EEPROM.update(address + i, str[i]);
  }
  
  EEPROM.get(address, strBuffer);
  if (strcmp(str, strBuffer) != 0)
  {
    lcd.print(F("Failed to save"));
    lcd.setCursor(0, 1);
    lcd.print(F("to EEPROM!!!"));
    byte ctr = 30;
    while (ctr > 0 && !isInt0 && !isInt1)
    {
      doorBreakingTone();
      ctr--;
    }
    lcd.clear();
    enableSleepLCD();
    return false;
  }
  return true;
}

void getKeyObject()
{
  key = kpd.getKey();
  if (key.state == KEY_UP)
  {
    enableSleepLCD();
  }
  else
  {
    isWaitingForSleep = false;
  }
}

boolean getAKey()
{
  while(true)
  {
    getKeyObject();
    if (kpd.isCanceled)
    {
      return false;
    }
    if (key.state == KEY_UP && key.character != 0)
    {
      return true;
    }
  }
}

boolean getString()
{
  lcd.cursor();
  lcd.blink();
  boolean result = getStringPD(16, 30);
  lcd.noCursor();
  lcd.noBlink();
  return result;
}

boolean getStringPD(byte startPos, byte endPos)
{
  char chr;
  byte strSize = endPos - startPos + 1;
  startCursorPos = startPos;
  endCursorPos = endPos;
  cursorPos = startPos;
  setCursorPos();
  chrCtr = 0;
  displayInputMode();
  while (true)
  {
    getKeyObject();
    chr = key.character;
    switch (key.state)
    {
      case CANCELED:
        return false;
      case KEY_DOWN:
      case MULTI_TAP:
        tone(BUZZER_PIN, 5000, 20);
        digitalWrite(LED_BUILTIN, HIGH);
        switch (key.code)
        {
          case KEY_1:
            if (isAlphaMode)
            {
              chr = getSymbol(key.tapCounter, SYMBOL_1);
            }
            break;
          case KEY_0:
            if (isAlphaMode)
            {
              chr = ' ';
            }
            break;
          case KEY_ASTERISK:
            if (isAlphaMode)
            {
              chr = getSymbol(key.tapCounter, SYMBOL_A);
            }
            break;
          case KEY_NUMBER_SIGN:
            if (isAlphaMode)
            {
              chr = getSymbol(key.tapCounter, SYMBOL_NS);
            }
            break;
          case KEY_A:
            if (isAlphaMode)
            {
              isUpperCaseMode = !isUpperCaseMode;
              chr = isUpperCaseMode ? CAPSLOCK_ON : CAPSLOCK_OFF;
            }
            else
            {
              chr = 0;
            }
            break;
          case KEY_B:
            chr = BACKSPACE;
            break;
          case KEY_C:
            chrCtr = 0;
            strBuffer[0] = 0;
            chr = CLEARSCREEN;
            break;
          case KEY_D:
            strBuffer[chrCtr] = 0;
            chr = CARRIAGE_RETURN;
            break;
          default:
            if (isAlphaMode)
            {
              chr = getAlphabet(key.character, key.tapCounter);
              if (!isUpperCaseMode)
              {
                chr += 32;  // makes lower case
              }
            }
        }
        if (key.state == MULTI_TAP && isAlphaMode && key.character < 'A' && key.character != '0')
        {
          printToLcd(BACKSPACE);
          chrCtr--;
        }
        if (chr == BACKSPACE)
        {
          chrCtr--;
        }
        if (chr >= ' ')
        {
          strBuffer[chrCtr] = chr;
          if (chrCtr < strSize)
          {
            chrCtr++;
          }
        }
        if (chr != CARRIAGE_RETURN && chr !=0)
        {
          printToLcd(chr);
        }
        break;
      case LONG_TAP:
        switch (key.code)
        {
          case KEY_A:
            if (isAlphaMode)
            {
              isUpperCaseMode = !isUpperCaseMode;
              isAlphaMode = false;
              chr = NUMLOCK_ON;
            }
            else
            {
              isAlphaMode = true;
              chr = NUMLOCK_OFF;
            }
            break;
          case KEY_B:
            chr = CLEARSCREEN;
            chrCtr = 0;
            break;
          case KEY_C:
          case KEY_D:
            chr = 0;
            break;
          default:
            if (!isAlphaMode)
            {
              chr = 0;
            }
        }
        if (chr > 0)
        {
          tone(BUZZER_PIN, 5000, 20);
          if (' ' < chr && chr < 'A')
          {
            printToLcd(BACKSPACE);
            chrCtr--;
            strBuffer[chrCtr] = chr;
            if (chrCtr < strSize)
            {
              chrCtr++;
            }
          }
          printToLcd(chr);
        }
        break;
      case MULTI_KEY_DOWN:
        tone(BUZZER_PIN, 4000, 100);
        break;
      case KEY_UP:
        if (key.character == 'D' && chrCtr > 0)
        {
          return true;
        }
        if (key.character == 'C')
        {
          return false;
        }
    }
  }
}

void printToLcd(char chr)
{
  switch (chr)
  {
    case BACKSPACE:
      if (cursorPos > startCursorPos)
      {
        if (!isEndOfDisplay)
        {
          decCursorPos();
        }
        setCursorPos();
        lcd.print(F(" "));
        setCursorPos();
        isEndOfDisplay = false;
      }
      break;
    case CLEARSCREEN:
      while (cursorPos > startCursorPos)
      {
        decCursorPos();
        setCursorPos();
        lcd.print(F(" "));
        setCursorPos();
      }
      break;
    case CAPSLOCK_ON:
    case CAPSLOCK_OFF:
    case NUMLOCK_ON:
    case NUMLOCK_OFF:
      displayInputMode();
      break;
    default:
      if (cursorPos == endCursorPos)
      {
        isEndOfDisplay = true;
      }
      lcd.print(chr);
      incCursorPos();
      setCursorPos();
  }
}

void displayInputMode()
{
  lcd.setCursor(13, 0);
  if (isAlphaMode)
  {
    if (isUpperCaseMode)
    {
      lcd.print(F("ABC"));
    }
    else
    {
      lcd.print(F("abc"));
    }
  }
  else
  {
    lcd.print(F("123"));
  }
  setCursorPos();
}

void incCursorPos()
{
  if (cursorPos < endCursorPos)
  {
    cursorPos++;
  }
}

void decCursorPos()
{
  if (cursorPos > startCursorPos)
  {
    cursorPos--;
  }
}

void setCursorPos()
{
  lcd.setCursor(cursorPos % 16, cursorPos / 16);
}

byte getSymbol(byte ctr, char * pointer)
{
  byte chr = pgm_read_byte_near(pointer + ctr);
  if (chr == CHR_BOUND)
  {
    chr = pgm_read_byte_near(pointer);
    kpd.resetTapCounter();
  }
  return chr;
}

byte getAlphabet(byte chr, byte ctr)
{
  chr = (chr - '2') * 5;
  byte alpha = pgm_read_byte_near(ALPHABET + chr + ctr);
  if (alpha == CHR_BOUND)
  {
    alpha = pgm_read_byte_near(ALPHABET + chr);
    kpd.resetTapCounter();
  } 
  return alpha;
}

