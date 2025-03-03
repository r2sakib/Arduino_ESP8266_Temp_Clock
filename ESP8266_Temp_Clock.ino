#include <SoftwareSerial.h>
#include <TimeLib.h>
#include <LiquidCrystal.h>

#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"

#define RX 11 // TX of ESP8266
#define TX 10 // RX of ESP8266
#define THERMISTORPIN 6

#define I2C_ADDRESS 0x3C
#define RST_PIN -1

#define LCD_BACKLIGHT_PIN 3
#define MODE_BTN 7
#define STOPWATCH_BTN 13

#define TIMER_INCREASE_BTN A0
#define TIMER_DECREASE_BTN A1
#define TIMER_START_BTN A2
#define BUZZER 12


SSD1306AsciiAvrI2c oled;
SoftwareSerial esp8266(RX, TX); 

const byte rs = 9, en = 8, d4 = 6, d5 = 5, d6 = 4, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// ThingSpeak
String AP = "Aurora_2.4";       // CHANGE ME
String PASS = "bladerunner066@"; // CHANGE ME
String API = "DZT780IUSGB11ET4";   // CHANGE ME
String HOST = "api.thingspeak.com";
String PORT = "80";
String field = "field1";
byte countTrueCommand;
byte countTimeCommand; 
boolean found = false; 
byte valSensor = 1;

// TIME
String atTimeString;
tmElements_t tm;

void setup() {
  Serial.begin(4800);
  esp8266.begin(9600);
  
  pinMode(THERMISTORPIN, INPUT);
  pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
  pinMode(MODE_BTN, INPUT_PULLUP);
  pinMode(STOPWATCH_BTN, INPUT);
  pinMode(TIMER_DECREASE_BTN, INPUT_PULLUP);
  pinMode(TIMER_INCREASE_BTN, INPUT_PULLUP);
  pinMode(TIMER_START_BTN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);


  // OlED setup
  #if RST_PIN >= 0
    oled.begin(&Adafruit128x64, I2C_ADDRESS, RST_PIN);
  #else
    oled.begin(&Adafruit128x64, I2C_ADDRESS);
  #endif
  oled.setFont(Adafruit5x7);
  uint32_t m = micros();
  oled.clear();

  // LCD setup
  analogWrite(LCD_BACKLIGHT_PIN, 180);
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  lcd.setCursor(0, 1);
  lcd.print("Syncing time...");

  // WiFi Setup
  sendCommand(F("AT"),5,"OK", 1);
  sendCommand(F("AT+CWMODE=1"),5,"OK", 1);
  sendCommand("AT+CWJAP=\""+ AP +"\",\""+ PASS +"\"",20,"OK", 1);
  
  // Get NTP time
  esp8266.println(F("AT+CIPSNTPCFG=1,6,\"0.pool.ntp.org\",\"time.google.com\""));
  while( esp8266.available() > 0 ) esp8266.readString();
  delay(2000);
  esp8266.println(F("AT+CIPSNTPTIME?"));

  while (esp8266.available() == 0) {};
  atTimeString = esp8266.readString();
  createTimeElements(atTimeString);
  setTime(makeTime(tm));
}

static const unsigned long REFRESH_INTERVAL = 1000;
static unsigned long lastRefreshTime = 0;

byte mode = 0;

bool running = false;     // Stopwatch state
unsigned long startTime = 0;
unsigned long elapsedBeforePause = 0;

bool lastButtonState = HIGH;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // Debounce time

static unsigned long lastSyncTime = 0;
const unsigned long syncInterval = 1800000; // 30 minutes


void loop() {
  analogWrite(LCD_BACKLIGHT_PIN, 180);
  // syncTimeWithNTP();

  // TEMPERATURE UPDATE
  if (millis() - lastRefreshTime >= REFRESH_INTERVAL) {
    lastRefreshTime += REFRESH_INTERVAL;
    printTemp();
  }

  // Periodically resync time with NTP
  if (millis() - lastSyncTime >= syncInterval) {
    syncTimeWithNTP();
    lastSyncTime = millis();
  }

  byte modeRead = digitalRead(MODE_BTN);
  if (modeRead == LOW) {
    lcd.clear();
    if (mode == 0) {
      lcd.setCursor(3, 0);
      lcd.print("Stopwatch");
      mode = 1;
    } else if (mode == 1) {
      handleTimer();
      mode = 2;
    } else if (mode == 2) {
      printTime();
      mode = 0;
    }
  }

  if (mode == 0) {
    printTime();
  }

  byte stopwatchButtonState = digitalRead(STOPWATCH_BTN);

  // **STOPWATCH FUNCTIONALITY**
  if (mode == 1) {
    handleStopwatch(stopwatchButtonState);
  }

  if (mode == 2) {
    handleTimer();
  }

  delay(200);
}

void syncTimeWithNTP() {
  Serial.println("Syncing time with NTP...");

  esp8266.println(F("AT+CIPSNTPTIME?"));
  delay(2000);

  if (esp8266.available()) {
    atTimeString = esp8266.readString();
    createTimeElements(atTimeString);
    setTime(makeTime(tm));
    Serial.println("Time synchronized.");
  } else {
    Serial.println("NTP sync failed.");
  }
}


// void loop() {
//   analogWrite(LCD_BACKLIGHT_PIN, 180);

//   // TEMPERATURE UPDATE
//   if (millis() - lastRefreshTime >= REFRESH_INTERVAL) {
//     lastRefreshTime += REFRESH_INTERVAL;
//     printTemp();
//   }

//   byte modeRead = digitalRead(MODE_BTN);

//   if (modeRead == LOW) {
//     lcd.clear();
//     if (mode == 0) {
//       lcd.setCursor(3, 0);
//       lcd.print("Stopwatch");
//       mode = 1;
//     } 
//     else if (mode == 1) {
//       handleTimer();
//       mode = 2;
//     } 
//     else if (mode == 2) {
//       printTime();
//       mode = 0;
//     }
//   }

//   if (mode == 0) {
//     printTime();
//   }

//   byte stopwatchButtonState = digitalRead(STOPWATCH_BTN);

//   // **STOPWATCH FUNCTIONALITY**
//   if (mode == 1) {
//     handleStopwatch(stopwatchButtonState);
//   }

//   // Reset on long press (hold for 1.5 seconds)
//   if (mode != 1 && elapsedBeforePause > 0) {
//     resetStopwatch();
//   }

//   if (mode == 2) {
//     handleTimer();
//   }

//   delay(200);
// }


// TIMER VARIABLES
int timerMinutes = 5;  // Default timer start at 1 minute
unsigned long timerStartMillis = 0;
bool timerRunning = false;
bool timerExpired = false;

void handleTimer() {
  updateTimerDisplay();
  byte increaseBtn = digitalRead(TIMER_INCREASE_BTN);
  byte decreaseBtn = digitalRead(TIMER_DECREASE_BTN);
  byte startBtn = digitalRead(TIMER_START_BTN);

  // Increase timer by 1 minute
  if (increaseBtn == LOW) {
    timerMinutes += 5;
    updateTimerDisplay();
    delay(250);  // Small delay to prevent multiple increments
  }

  // Decrease timer by 1 minute (minimum 1)
  if (decreaseBtn == LOW && timerMinutes > 1) {
    timerMinutes -= 5;
    updateTimerDisplay();
    delay(250);
  }

  // Start or Stop Timer
  if (startBtn == LOW) {
    if (!timerRunning) {
      timerStartMillis = millis();
      timerRunning = true;
      timerExpired = false;
    } else {
      timerRunning = false;
    }
    delay(250);
  }

  // Timer Countdown Logic
  if (timerRunning) {
    unsigned long elapsedMillis = millis() - timerStartMillis;
    int remainingSeconds = (timerMinutes * 60) - (elapsedMillis / 1000);

    if (remainingSeconds <= 0) {
      timerRunning = false;
      timerExpired = true;
      remainingSeconds = 0;
    }

    displayCountdown(remainingSeconds);
  }

  if (timerExpired) {
    if (mode == 2) {
      lcd.clear();
      lcd.setCursor(3, 0);
      lcd.print("Time's up!");
      delay(500);
    }
    digitalWrite(BUZZER, HIGH);
    delay(5000);
    digitalWrite(BUZZER, LOW);
    timerExpired = false;
  }
}

void updateTimerDisplay() {
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Set Timer");
  lcd.setCursor(5, 1);
  lcd.print(timerMinutes);
  lcd.print(" min");
}

void displayCountdown(int secondsLeft) {
  int minutes = secondsLeft / 60;
  int seconds = secondsLeft % 60;

  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Time Left");
  lcd.setCursor(5, 1);
  lcd.print((minutes < 10) ? "0" : ""); 
  lcd.print(minutes);
  lcd.print(":");
  lcd.print((seconds < 10) ? "0" : ""); 
  lcd.print(seconds);
}



void handleStopwatch(byte buttonState) {
  unsigned long currentMillis = millis();

  // Debounce check
  if (buttonState == HIGH && (currentMillis - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = currentMillis; // Update debounce time

    if (!running) {
      // Start or Resume Stopwatch
      startTime = millis() - elapsedBeforePause;
      running = true;
    } else {
      // Pause Stopwatch
      elapsedBeforePause = millis() - startTime;
      running = false;
    }
  }

  // Ensure buttonPressed resets properly
  if (buttonState == LOW) {
    buttonPressed = false;
  }

  // **Update Stopwatch Display**
  unsigned long elapsed = running ? millis() - startTime : elapsedBeforePause;
  printStopwatchTime(elapsed);
}


void resetStopwatch() {
  running = false;
  elapsedBeforePause = 0;
  startTime = 0;
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Stopwatch");
  printStopwatchTime(0);
}

void printStopwatchTime(unsigned long elapsed) {
  int hours = (elapsed / 3600000) % 24;
  int minutes = (elapsed / 60000) % 60;
  int seconds = (elapsed / 1000) % 60;

  lcd.setCursor(3, 1);
  lcd.print((hours < 10) ? "0" : ""); lcd.print(hours); lcd.print(":");
  lcd.print((minutes < 10) ? "0" : ""); lcd.print(minutes); lcd.print(":");
  lcd.print((seconds < 10) ? "0" : ""); lcd.print(seconds);
}


void printTime() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print(getDate());
  lcd.setCursor(3, 1);
  lcd.print(getTime());
}

void printTemp() {
  String temp = String(getTemp());
  oled.set1X();
  oled.setCursor(30, 2);
  oled.print("Temperature");
  oled.set2X();
  oled.setCursor(25, 4);
  oled.print(temp);
  oled.print(" C");
}




// From thermistor datasheet
#define RT0 10000
#define B 4100
// ----
#define VCC 4.95
#define R 10000

float RT, VR, ln, T, VRT;
float T0 = 25 + 273.15;

float getTemp() {
  VRT = analogRead(THERMISTORPIN);              //Acquisition analog value of VRT
  VRT  = (VCC / 1023.00) * VRT;      //Conversion to voltage
  VR = VCC - VRT;
  RT = VRT / (VR / R);                //Resistance of RT
  ln = log(RT / RT0);
  T = (1 / ((ln / B) + (1 / T0)));   //Temperature from thermistor
  T =  T - 273.15;                 //Conversion to Celsius

  return T;
}

int monthStringToInt(String month) {
  if (month.equals(F("Jan"))) {return 1;}
  else if (month.equals(F("Mar"))) {return 3;}
  else if (month.equals(F("Feb"))) {return 2;}
  else if (month.equals(F("Apr"))) {return 4;}
  else if (month.equals(F("May"))) {return 5;}
  else if (month.equals(F("Jun"))) {return 6;}
  else if (month.equals(F("Jul"))) {return 7;}
  else if (month.equals(F("Aug"))) {return 8;}
  else if (month.equals(F("Sep"))) {return 9;}
  else if (month.equals(F("Oct"))) {return 10;}
  else if (month.equals(F("Nov"))) {return 11;}
  else if (month.equals(F("Dec"))) {return 12;}
}

void createTimeElements(String atTimeString) {
  String justTimeDate = atTimeString.substring(atTimeString.indexOf(F(":"))+1, atTimeString.indexOf(F("202"))+4);
  tm.Month = monthStringToInt(justTimeDate.substring(4, 7));
  tm.Day = justTimeDate.substring(8, 10).toInt();
  tm.Hour = justTimeDate.substring(11, 13).toInt();
  tm.Minute = justTimeDate.substring(14, 16).toInt();
  tm.Second = justTimeDate.substring(17, 19).toInt()+2;
  tm.Year = CalendarYrToTm(justTimeDate.substring(20).toInt());
}

String getTime() {
  String time = String();
  time += hourFormat12();;
  time += F(":");
  if (minute() < 10)
    time += F("0");
  time += minute();
  time += F(":");
  if (second() < 10)
    time += F("0");
  time += second(); 
  
  time += F(" ");
  if (isAM())
    time += "AM";
  else if (isPM())
    time += "PM";

  return time;
}

String getDate() {
  String date = String();
  date += dayShortStr(weekday());
  date += " ";
  date += day();
  date += "-";
  date += monthShortStr(month());
  date += "-";
  date += year();
  return date;
}

void pushSensorData() {
 valSensor = getSensorData();
 String getData = "GET /update?api_key="+ API +"&"+ field +"="+String(valSensor);
 sendCommand(F("AT+CIPMUX=1"),5,"OK", valSensor);
 sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +F("\",")+ PORT,5,"OK", valSensor);
 sendCommand("AT+CIPSEND=0," +String(getData.length()+4),2,">", valSensor);
 esp8266.println(getData);delay(1500);countTrueCommand++;
 sendCommand(F("AT+CIPCLOSE=0"),2,"OK", valSensor);
}

int getSensorData(){
  return random(1000); // Replace with 
}

void sendCommand(String command, int maxTime, char readReplay[], int val) {
  Serial.print(countTrueCommand);
  Serial.print(F(". at command => "));
  Serial.print(command);
  Serial.print(F(" "));
  Serial.print(val);
  Serial.print(F(" "));
  while(countTimeCommand < (maxTime*1))
  {
    esp8266.println(command);//at+cipsend
    if(esp8266.find(readReplay))//ok
    {
      found = true;
      break;
    }
  
    countTimeCommand++;
  }
  
  if(found == true)
  {
    Serial.println(F("PASS"));
    countTrueCommand++;
    countTimeCommand = 0;
  }
  
  if(found == false)
  {
    Serial.println(F("FAIL"));
    countTrueCommand = 0;
    countTimeCommand = 0;
  }
  
  found = false;
 }