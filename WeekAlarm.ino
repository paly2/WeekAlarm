#include <SevSeg.h>

#include <Wire.h>
#include <Time.h>
#include <DS1307RTC.h>

#include <IRremote.h>
#include "commands.h"

#include <NewTone.h>

//Create an instance of the objects.
SevSeg Display;
IRrecv irrecv(11); // Using the pin 11 for the IR receiver

// Alarm time
tmElements_t alarm[7] = {{0,0,0,0,0,0,0}};
bool noalarm = false;
bool alarm_triggered = false;

int brightness = 100;

// The weekday() doesn't return the right value with the RTC, so I add this to make the value right.
int dayDifference = 0;

void setup() {
  //Serial.begin(9600);
  setSyncProvider(RTC.get);
  setSyncInterval(60); // Re-sync every minute
  
  // Common anode 4 digits display
  Display.Begin(COMMON_ANODE, 4,  0,1,2,3,  4,5,6,7,8,9,10,12);
  Display.SetBrightness(100); //Set the display to 100% brightness level
  
  irrecv.enableIRIn(); // Start the receiver
}

int get_new_digit(char* displayString, int blinky, int point) {
  irrecv.resume();
  int new_digit = -1;
  decode_results results;
  do {
    //Serial.println(displayString);
    char display_string[10]; strcpy(display_string, displayString);
    if (second()%2 == 0)
      display_string[blinky] = ' ';
    Display.DisplayString(display_string, point);
    if (irrecv.decode(&results)) {
      switch (results.value) {
        // The first hour digit must be between 0 and 2.
        case RC_0:
          new_digit = 0;
          break;
        case RC_1:
          new_digit = 1;
          break;
        case RC_2:
          new_digit = 2;
          break;
        case RC_3:
          new_digit = 3;
          break;
        case RC_4:
          new_digit = 4;
          break;
        case RC_5:
          new_digit = 5;
          break;
        case RC_6:
          new_digit = 6;
          break;
        case RC_7:
          new_digit = 7;
          break;
        case RC_8:
          new_digit = 8;
          break;
        case RC_9:
          new_digit = 9;
          break;
      }
      irrecv.resume();
    }
  } while (new_digit == -1);
  
  return new_digit;
}

void settime(tmElements_t *tm) {
  char timeString[5];
  sprintf(timeString, "%02d%02d", tm->Hour, tm->Minute);
  
  // First hour digit
  int new_digit = 0;
  do {
    new_digit = get_new_digit(timeString, 0, 2);
  } while (new_digit > 2); // The digit must be between 0 and 2
  tm->Hour = tm->Hour%10 + new_digit*10;
  timeString[0] = new_digit+'0';
  
  // Second hour digit
  do {
    new_digit = get_new_digit(timeString, 1, 2);
  } while (new_digit > 3 && timeString[0] == '2'); // The digit must be between 0 and 3 if the first digit is 2
  tm->Hour = tm->Hour/10*10 + new_digit;
  timeString[1] = new_digit+'0';
  
  // First minute digit
  do {
    new_digit = get_new_digit(timeString, 2, 2);
  } while (new_digit > 5); // The digit must be between 0 and 5
  tm->Minute = tm->Minute%10 + new_digit*10;
  timeString[2] = new_digit+'0';
  
  // Second minute digit
  tm->Minute = tm->Minute/10*10 + get_new_digit(timeString, 3, 2);
  
  
  tm->Second = tm->Day = tm->Wday = tm->Month = tm->Year = 1;
  
  irrecv.resume();
}

void setalarm() {
  int day = 0;
  char tempString[10] = "DAY ";
  tempString[3] = (weekday()+dayDifference-1)%7+'1';
  do {
    day = get_new_digit(tempString, 3, 0);
  } while (day > 7 || day < 1); // The digit must be between 1 and 7
  day--; // The index used to access to a value in the alarm array must be between 0 and 6.
  settime(&(alarm[day]));
}

void loop() {
  char tempString[10]; //Used for sprintf
  sprintf(tempString, "%02d%02d", hour(), minute());

  // 1. Display time
  if (second()%2 == 0)
    Display.DisplayString(tempString, 2);
  else
    Display.DisplayString(tempString, 0);
  
  // 2. Check for IR receiver
  decode_results results;
  int Day = 0;
  tmElements_t tm;
  RTC.read(tm);
  if (irrecv.decode(&results)) {
    if (alarm_triggered) {
      noNewTone(13);
      alarm_triggered = false;
    }
    else {
      switch (results.value) {
        case RC_settime:
          settime(&tm);
          
          // Day
          sprintf(tempString, "DAY%c", (weekday()+dayDifference-1)%7+'1');
          do {
            Day = get_new_digit(tempString, 3, 0);
          } while (Day > 7 || Day < 1); // The day number must be between 1 and 7
          dayDifference = Day-weekday();
          
          RTC.write(tm);
          setSyncProvider(RTC.get);
          break;
        case RC_setalarm:
          setalarm();
          break;
        case RC_morelight:
          if (brightness < 100-10) {
            brightness+=10;
            Display.SetBrightness(brightness);
          }
          break;
        case RC_lesslight:
          if (brightness > 10) {
            brightness-=10;
            Display.SetBrightness(brightness);
          }
          break;
        case RC_noalarm:
          noalarm = !noalarm;
          unsigned long timer = millis()+500;
          while (millis() < timer) {
            if (noalarm)
              Display.DisplayString("NO  ", 0);
            else
              Display.DisplayString("YES ", 0);
          }
          break;
      }
    }
    irrecv.resume(); // Receive the next value
  }
  
  // 3. Check for alarm time
  tmElements_t alarm_tm = alarm[(weekday()+dayDifference-1)%7];
  if (hour() == alarm_tm.Hour && minute() == alarm_tm.Minute && second() == 1 && !alarm_triggered
      && !(alarm_tm.Hour == 0 && alarm_tm.Minute == 0)) {
    if (!noalarm) {
      NewTone(13, 2000); // 13 is the buzzer pin
      alarm_triggered = true;
    }
  }
  
  delay(10);
}
