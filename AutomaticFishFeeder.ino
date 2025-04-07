#include <Servo.h>             // Servo
#include <Wire.h>              // Button
#include <LiquidCrystal_I2C.h> // LCD
#include <DS3232RTC.h>         // DS3231
#include <Streaming.h>         // DS3231
#include <TimeLib.h>           // DS3231
#include <EEPROM.h>

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Setting up buttons and servo
const int bt1 = 3;
const int bt2 = 4;
const int bt3 = 5;
const int bt4 = 6;
char pressedButton = '0';

const int servopin = 9;

// Menu Level
// 0 = Homepage; 1 = Date; 2 = Time; 3 = Times to Feed; 4 = Time to feed & Quantity of Food (Loop) 5 = On sleep mode
byte menu = 0;
bool needsUpdate = true;

// Initializing values
int Tmont = 1;
int Tday = 1;
int Tyear = 2024;
int Thour = 1;
int Tmint = 0;
int Ffeed = 1; // Frequency of feeding
int Fqty = 1;  // Feeding quantity in mL

bool feedHourSet = false;
bool feedMinSet = false;
bool feedManual = false;

int *feedHour;
int *feedMin;
int *timeStore;

unsigned long previousMillis = 0; // Stores the last time the display was updated
const long interval = 15000;      // Interval at which to update the time (1 minute)

static int currentFeedIndex = 0;
int pos = 0; // Servo position

// Min,Max values
// MINMAX[10] = {minMonth, maxMonth, minDay, maxDay, minYear, maxYear, minHour, maxHour, minMinute, maxMinute, minFeed, maxFeed};
const int MINMAX[12] = {1, 12, 0, 31, 2000, 2200, 0, 23, 0, 59, 1, 20};

DS3232RTC myRTC; // Custom RTC functions
tmElements_t tm;
Servo myservo;

void setup()
{
    // Initialization of Serial, LCD, RTC
    Serial.begin(9600);
    lcd.begin();
    myRTC.begin();
    setSyncProvider(myRTC.get);
    
    // Gather EEPROM values for offline function
    Ffeed = EEPROM.read(0);
    Fqty = EEPROM.read(1);
    
    lcd.backlight();

    // Set up button pins
    pinMode(bt1, INPUT_PULLUP);
    pinMode(bt2, INPUT_PULLUP);
    pinMode(bt3, INPUT_PULLUP);
    pinMode(bt4, INPUT_PULLUP);

    // Servo pin
    myservo.attach(servopin);
    myservo.write(0);

    feedHour = new int[Ffeed];
    feedMin = new int[Ffeed];
    timeStore = new int[Ffeed * 2];

    // Saving offline values to timeStore array
    gatherEEPROMValues(timeStore, Ffeed * 2);

    // Accessing new timeStore array with EEPROM values to extract feedHour and feedMin values
    for (int i = 0; i < Ffeed; i++)
    {
        feedHour[i] = timeStore[2 * i];
        feedMin[i] = timeStore[2 * i + 1];
    }
}

void loop()
{
    checkButton();

    // Update current time display every minute
    if (menu == 12)
    {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval)
        {
            previousMillis = currentMillis;
            needsUpdate = true; // Trigger a menu update
        }
    }
    feedIni(timeStore, Ffeed, Fqty); // timeStore[0] is Ffeed
}

void checkButton()
{
    // Check if button is pressed or not
    bool stateBt1 = digitalRead(bt1);
    bool stateBt2 = digitalRead(bt2);
    bool stateBt3 = digitalRead(bt3);
    bool stateBt4 = digitalRead(bt4);

    // Check what button is pressed
    if (stateBt1 == LOW)
    {
        pressedButton = 'B'; // Back
    }
    else if (stateBt2 == LOW)
    {
        pressedButton = 'L'; // Left
    }
    else if (stateBt3 == LOW)
    {
        pressedButton = 'R'; // Right
    }
    else if (stateBt4 == LOW)
    {
        pressedButton = 'E'; // Enter/Set
    }
    else
    {
        pressedButton = '0';
    }

    processButton(pressedButton);
}

void processButton(char pressedButton)
{
    switch (menu)
    {
    case 0:
        if (pressedButton == 'E') {
            changeMenu(1, true, 200); // Goes to menu page 1
        }
        break;
    // Select adjust time or not
    case 1:
        yesOrNoValue(pressedButton, 0, 2, 7);
        break;
    // Adjusted case (+ 1 )
    case 2:
        adjustValue(pressedButton, &Thour, MINMAX[6], MINMAX[7], 1, 3);
        break;
    case 3:
        adjustValue(pressedButton, &Tmint, MINMAX[8], MINMAX[9], 2, 4);
        break;
    case 4:
        adjustValue(pressedButton, &Tmont, MINMAX[0], MINMAX[1], 3, 5);
        break;
    case 5:
        adjustValue(pressedButton, &Tday, MINMAX[2], MINMAX[3], 4, 6);
        break;
    case 6:
        adjustValue(pressedButton, &Tyear, MINMAX[4], MINMAX[5], 5, 7);
        setTime(Thour, Tmint, 0, Tday, Tmont, Tyear); // Set time
        myRTC.set(now());
        break;
    case 7: // change
        manualFeed(pressedButton, 6, 7, 8);
        break;
    case 8: // change
        yesOrNoValue(pressedButton, 7, 9, 14);
        break;
    case 9: // case + 3
        adjustValue(pressedButton, &Ffeed, MINMAX[10], MINMAX[11], 8, 10); // Setting the frequency of feeding
        updateFeedArrays();
        break;
    case 101: // case + 3
        adjustValue(pressedButton, &Fqty, MINMAX[10], MINMAX[11], 9, 11); // Adjust feeding quantity per feed
        break;
    case 11: // case + 3
        adjustValue(pressedButton, &feedHour[currentFeedIndex], MINMAX[6], MINMAX[7], 10, 12); // Adjust feeding hour
        break;
    case 12: // case + 3
        adjustValue(pressedButton, &feedMin[currentFeedIndex], MINMAX[8], MINMAX[9], 11, 13); // Adjust feeding minute
        break;
    case 13: // case + 3
        setFeedTime();
        //printTimeStore();
        break;
    case 14: // case + 3
        saveOffline();
        staticValue(pressedButton, 9, 15); // Show current time
        break;
    case 15: // case + 3
        staticValue(pressedButton, 14, 0); // Sleep mode, save battery
        //printTimeStore();
        break;
    }

    if (needsUpdate)
    {
        updateMenu();
        needsUpdate = false;
    }
}

void updateFeedArrays()
{
    // Updated: Avoid memory leak by deleting the old arrays before re-allocating
    delete[] feedHour;
    delete[] feedMin;
    feedHour = new int[Ffeed];
    feedMin = new int[Ffeed];
    
    for (int i = 0; i < Ffeed; i++)
    {
        feedHour[i] = 0;
        feedMin[i] = 0;
    }
}

// Set Feeding Time Loop
void setFeedTime()
{
    timeStore[2 * currentFeedIndex] = feedHour[currentFeedIndex];
    timeStore[2 * currentFeedIndex + 1] = feedMin[currentFeedIndex];
    currentFeedIndex++;

    feedHourSet = true;
    feedMinSet = true;

    if (currentFeedIndex >= Ffeed)
    {
        changeMenu(12, true, 200);
    }
    else
    {
        changeMenu(9, true, 200);
    }
    
}

void updateMenu()
{
    lcd.clear();
    switch (menu)
    {
    case 0: // Homepage
        lcd.print("Automatic");
        lcd.setCursor(0, 1);
        lcd.print("Fish Feeder");
        break;
    // Select adjust time or not
    case 1:
        yesOrNoMenu();
        break;
    // Adjusted case (+ 1 )
    case 2: // HH
        timeMenu();
        break;
    case 3: // MM
        timeMenu();
        break;
    case 4: // MM
        dateMenu();
        break;
    case 5: // DD
        dateMenu();
        break;
    case 6: // YY
        dateMenu();
        break;
    // Manual or Auto
    case 7: //
        manualOrAuto();
        break;
    case 8: // case + 2
        yesOrNoFeedMenu();
        break;
    case 9: // Frequency of Feeding // case + 3
        lcd.print("Freq. of Feed:");
        lcd.setCursor(0, 1);
        lcd.print(Ffeed);
        break;
    case 10: // Feeding quantity in mL // case + 3
        lcd.print("Feed in mL:");
        lcd.setCursor(4, 1);
        lcd.print(Fqty);
        break;
    case 11:   // case + 3
        feedMenuPrompt(Ffeed, currentFeedIndex); // Hour
        break;
    case 12: // case + 3
        feedMenuPrompt(Ffeed, currentFeedIndex); // Minute
        break;
    case 13: // case + 3
        lcd.print("Next Feed Time");
        break;
    case 14: // case + 3
        lcd.backlight();
        lcd.print("Current Time:");
        lcd.setCursor(0, 1);
        printCurrentTime();
        break;
    case 15: // case + 3
        lcd.print("Sleep Mode");
        delay(1000);
        lcd.noBacklight();
        break;
    }
}

// changeMenu after clicking button 'R' or 'B'
void changeMenu(byte newMenu, bool update, int delayTime)
{
    menu = newMenu; // Goes to menu page x
    needsUpdate = update;
    delay(delayTime);
}

// Adjusting Values with Min and Max values
void adjustValue(char pressedButton, int *baseValue, int minValue, int maxValue, int backMenu, int nextMenu)
{
    switch (pressedButton)
    {
    case 'B':
        changeMenu(backMenu, true, 200); // Goes back
        break;
    case 'L':
        (*baseValue)--; // Adjust hours
        if (*baseValue < minValue)
            (*baseValue) = minValue; // Stops at minimum limit
        needsUpdate = true;
        delay(200);
        break;
    case 'R':
        (*baseValue)++; // Adjust hours
        if (*baseValue > maxValue)
            (*baseValue) = maxValue; // Stops at maximum limit
        needsUpdate = true;
        delay(200);
        break;
    case 'E':
        changeMenu(nextMenu, true, 200); // Goes next menu
        break;
    }
}

void staticValue(char pressedButton, int backMenu, int nextMenu)
{
    switch (pressedButton)
    {
    case 'B':
        changeMenu(backMenu, true, 200); // Goes back
        needsUpdate = true;
        delay(200);
        break;
    case 'E':
        changeMenu(nextMenu, true, 200); // Goes next menu
        needsUpdate = true;
        delay(200);
        break;
    }
}

void yesOrNoValue(char pressedButton, int back2Menu, int next2Menu, int skipMenu)
{
    switch (pressedButton)
    {
    case 'B':
        changeMenu(back2Menu, true, 200); // Goes back to home page
        break;
    case 'L':
        // settingTime = true; // Yes
        needsUpdate = true;
        changeMenu(next2Menu, true, 200);
        delay(200);
        break;
    case 'R':
        // settingTime = false; // No
        needsUpdate = true;
        changeMenu(skipMenu, true, 200);
        delay(200);
        break;
    }
}

void manualFeed(char pressedButton, int back2Menu, int next2Menu, int skipMenu)
{
    switch (pressedButton)
    {
    case 'B':
        changeMenu(back2Menu, true, 200); // Goes back to home page
        break;
    case 'L':
        // settingTime = true; // Yes
        servoMove(1);
        break;
    case 'R':
        // settingTime = false; // No
        needsUpdate = true;
        changeMenu(skipMenu, true, 200);
        delay(200);
        break;
    }
}

void yesOrNoMenu()
{
    lcd.print("Set Time?");
    lcd.setCursor(5, 1);
    lcd.print("Yes");
    lcd.setCursor(9, 1);
    lcd.print("No");
}

void yesOrNoFeedMenu()
{
    lcd.print("Set Feed Time?");
    lcd.setCursor(5, 1);
    lcd.print("Yes");
    lcd.setCursor(9, 1);
    lcd.print("No");
}

void manualOrAuto()
{
    lcd.print("Manual Or Auto?");
    lcd.setCursor(2, 1);
    lcd.print("Manual");
    lcd.setCursor(10, 1);
    lcd.print("Auto");
}

void timeMenu()
{
    lcd.print("Time HH:MM");
    lcd.setCursor(0, 1);
    lcd.print("      :  "); // Print placeholders for HH and MM
    lcd.setCursor(5, 1);
    lcd.print(Thour < 10 ? "0" : ""); // Print leading zero if hour is less than 10
    lcd.print(Thour);
    lcd.setCursor(8, 1);              // Move cursor to position for minutes
    lcd.print(Tmint < 10 ? "0" : ""); // Print leading zero if minute is less than 10
    lcd.print(Tmint);
}

void dateMenu()
{
    lcd.print("Date MM-DD-YY");
    lcd.setCursor(0, 1);
    lcd.print("      :  :  "); // Print placeholders for HH and MM
    lcd.setCursor(5, 1);
    lcd.print(Tmont < 10 ? "0" : ""); // Print leading zero if hour is less than 10
    lcd.print(Tmont);
    lcd.setCursor(8, 1);             // Move cursor to position for minutes
    lcd.print(Tday < 10 ? "0" : ""); // Print leading zero if minute is less than 10
    lcd.print(Tday);
    lcd.setCursor(11, 1);             // Move cursor to position for minutes
    lcd.print(Tyear < 10 ? "0" : ""); // Print leading zero if minute is less than 10
    lcd.print(Tyear);
}

// Edit start here

void feedMenuPrompt(int ftime, int currentFeedIndex)
{
    lcd.clear();
    lcd.print("Time ");
    lcd.print(currentFeedIndex + 1);
    lcd.print(" of ");
    lcd.print(ftime);
    lcd.print(":");
    lcd.setCursor(0, 1);
    lcd.print(feedHour[currentFeedIndex] < 10 ? "0" : "");
    lcd.print(feedHour[currentFeedIndex]);
    lcd.print(":");
    lcd.print(feedMin[currentFeedIndex] < 10 ? "0" : "");
    lcd.print(feedMin[currentFeedIndex]);
}

// Save values even Arduino is OFF 100,000 cycles only
void saveOffline()
{
    EEPROM.update(0, Ffeed);
    EEPROM.update(1, Fqty);
    for (int i = 0; i < Ffeed * 2; i++) // Adjusted
    {
        EEPROM.update(i + 2, timeStore[i]);
    }
}

// Read EEPROM values for usage: For cases that the gadget is turned off
void gatherEEPROMValues(int *timeStore, int arrSize)
{
    for (int i = 0, max = arrSize + 2; i < max; i++)
    {
        timeStore[i] = EEPROM.read(i);
    }
}

void feedIni(int *timeArr, int Ffeed, int Fqty)
{  
  for (int i = 0; i < Ffeed; i++)
  {    
    int feedHourVal = feedHourSet ? feedHour[i] : timeArr[2 * i + 2]; // changed
    int feedMinVal = feedMinSet ? feedMin[i] : timeArr[2 * i + 3]; // changed

    if(feedHourVal == hour() && feedMinVal == minute() && second() == 0)
    {
      servoMove(Fqty);
    }
  }
}

// Current Time display
void printCurrentTime()
{
    lcd.print(hour() < 10 ? "0" : "");
    lcd.print(hour());
    lcd.print(":");
    lcd.print(minute() < 10 ? "0" : "");
    lcd.print(minute());
}

// Done; if to change, only the values
void servoMove(int Fqty)
{
    // Send feed to hole
    for (int i = 0, max = Fqty; i < max; i++)
    {
        for (pos = 0; pos <= 180; pos++)
        {
            myservo.write(pos);
            delay(30);
        }

        // Initial position return
        for (pos = 180; pos >= 0; pos--)
        {
            myservo.write(pos);
            delay(30);
        }
    }
}
