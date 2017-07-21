/*
Assignment 2 part 2:
Names: Tymoore Jamal and Dorsa Nahid
ID's: 1452978 (Tymoore) and 1463449 (Dorsa)
Aknowlegements: We both worked on this assignment together. With no other outside
help other than the cmput 274 proffessors and the code they provided which we
used.
*/
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "lcd_image.h"
#include <SPI.h>
#include <SD.h>
#define SD_CS 5
#define TFT_CS 6
#define TFT_DC 7
#define TFT_RST 8
#define JOY_SEL 9
#define JOY_VERT_ANALOG 0
#define JOY_HORIZ_ANALOG 1
#define TFT_WIDTH 128
#define TFT_HEIGHT 160
#define MILLIS_PER_FRAME 10
#define SPEED_MODIFIER 40 // multiplies delta by 1/SPEED_MODIFIER
#define JOY_DEADZONE 64
#define map_width 2048
#define map_height 2048
#define lat_north 5361858
#define lat_south 5340953
#define lon_west -11368652
#define lon_east -11333496
#define NUM_RESTAURANTS 1066
#define REST_START_BLOCK 4000000
#define RATING_DIAL 2       // Analog input A2 - restaurant dial selector
#define RATING_LED_0 2      // rating leds 0-4
#define RATING_LED_1 3
#define RATING_LED_2 4
#define RATING_LED_3 10
#define RATING_LED_4 11
int number_of_rests_to_display=0;
int skip=0;
int readPOT=0;
int lastPOT=-1;
int min_rating;
int g_joyX = TFT_WIDTH/2; // X-position of cursor in pixels
int g_joyY = TFT_HEIGHT/2; // Y-position of cursor in pixels
int g_cursorX = -1; // Drawn cursor position
int g_cursorY = -1;
int x_center;
int y_center;
bool update = 1;
bool update_name = 0;
int mode = 0;
bool move_up;
bool move_down;
int selection = 0, old_selection = 0;
char names [20][55];
int32_t position[20][2];
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Sd2Card card;

lcd_image_t map_image = { "yeg-big.lcd", 2048, 2048 };
int16_t icol = 1024;
int16_t irow = 1024;

// restaurant =
//    4-byte lat
//    4-byte lon
//    1-byte rating
//   55-char name
struct restaurant {
  int32_t lat;
  int32_t lon;
  uint8_t rating;
  char name[55];
};

// RestDist =
//    2-byte index
//    2-byte distance
struct RestDist{
  uint16_t index;     // index of restaurant from 0 to NUM_RESTAURANTS-1
  uint16_t dist;
};

RestDist all_restaurants[1066]={0};

/*
This function sets up the Arduino, its Serial monitor, the sd card, the LEd's
and the joystick's initial position. Also tests for SD card reading errors.
*/

void setup() {
  init();
  Serial.begin(9600);
  x_center=analogRead(JOY_HORIZ_ANALOG);
  y_center=analogRead(JOY_VERT_ANALOG);
  pinMode(RATING_LED_0, OUTPUT);
  pinMode(RATING_LED_1, OUTPUT);
  pinMode(RATING_LED_2, OUTPUT);
  pinMode(RATING_LED_3, OUTPUT);
  pinMode(RATING_LED_4, OUTPUT);
  pinMode(JOY_SEL, INPUT);
  digitalWrite(JOY_SEL, HIGH);
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
    while (true) {} // something is wrong
  }
  else {
    Serial.println("OK!");
  }
    Serial.print("Initializing SPI communication for raw reads...");
    if (!card.init(SPI_HALF_SPEED, SD_CS)) {
      Serial.println("failed!");
      while (true) {}
    }
    else {
      Serial.println("OK!");
    }
}

/*
When the value of the potentiometer is changed then this Function will change
which LEDs are lit up. To choose where to seperate the diffrent "levels" we just
divided 1023 (the max value of the potentiometer) by 6.
*/

void update_LEDs(){
  int x=analogRead(RATING_DIAL);
  if(x<=171){
    digitalWrite(RATING_LED_0, LOW);
    digitalWrite(RATING_LED_1, LOW);
    digitalWrite(RATING_LED_2, LOW);
    digitalWrite(RATING_LED_3, LOW);
    digitalWrite(RATING_LED_4, LOW);
    min_rating=0;
  }
  else if(x>171 && x<=341){
    digitalWrite(RATING_LED_0, HIGH);
    digitalWrite(RATING_LED_1, LOW);
    digitalWrite(RATING_LED_2, LOW);
    digitalWrite(RATING_LED_3, LOW);
    digitalWrite(RATING_LED_4, LOW);
    min_rating=1;
  }
  else if(x>341 && x<=512){
    digitalWrite(RATING_LED_0, HIGH);
    digitalWrite(RATING_LED_1, HIGH);
    digitalWrite(RATING_LED_2, LOW);
    digitalWrite(RATING_LED_3, LOW);
    digitalWrite(RATING_LED_4, LOW);
    min_rating=2;
  }
  else if(x>512 && x<= 682){
    digitalWrite(RATING_LED_0, HIGH);
    digitalWrite(RATING_LED_1, HIGH);
    digitalWrite(RATING_LED_2, HIGH);
    digitalWrite(RATING_LED_3, LOW);
    digitalWrite(RATING_LED_4, LOW);
    min_rating=3;
  }
  else if(x> 682 && x<=853){
    digitalWrite(RATING_LED_0, HIGH);
    digitalWrite(RATING_LED_1, HIGH);
    digitalWrite(RATING_LED_2, HIGH);
    digitalWrite(RATING_LED_3, HIGH);
    digitalWrite(RATING_LED_4, LOW);
    min_rating=4;
  }
  else if(x>853 && x<=1023){
    digitalWrite(RATING_LED_0, HIGH);
    digitalWrite(RATING_LED_1, HIGH);
    digitalWrite(RATING_LED_2, HIGH);
    digitalWrite(RATING_LED_3, HIGH);
    digitalWrite(RATING_LED_4, HIGH);
    min_rating=5;
  }
}



// Stores the value of the last buffer and last_block for get_restaurant_fast function.
restaurant last_buffer[8];
uint32_t last_block=0;

/*
Given a buffer "ptr" to hold 1 restaurant and the index of the restaurant we want,
load that buffer. Only loads it if nessecary, if we already have that buffer loaded
then it gets the restaurant from there.
*/

void get_restaurant_fast(restaurant* ptr, int i){
  uint32_t block = REST_START_BLOCK + i/8;
  if((block)!=last_block){
    while(!card.readBlock(block, (uint8_t*) last_buffer)){
    }
    *ptr = last_buffer[i%8]; // the correct restaurant
    last_block=block;
  }
  else{
    *ptr = last_buffer[i%8];
  }
}

// Converts a longitude value into an X value on the map

int16_t lon_to_x(int32_t lon) {
  return map(lon, lon_west, lon_east, 0, map_width);
}

// Converts a latitude value into an Y value on the map

int16_t lat_to_y(int32_t lat) {
  return map(lat, lat_north, lat_south, 0, map_height);
}

// Swap two restaurants of RestDist struct

void swap_rest(RestDist *ptr_rest1, RestDist *ptr_rest2) {
  RestDist tmp = *ptr_rest1;
  *ptr_rest1 = *ptr_rest2;
  *ptr_rest2 = tmp;
}

/*
Given an array, this function picks a pivot. The pivot it is the center of the
array or atmost off by 1
*/

int pick_pivot(RestDist* rest_dist, int len) {
  return len/2;
}

/*
Given an array and a pivot point this fucntion moves all numbers less than the
pivot to the right of the pivot and all the numbers greater than the pivot to
the right of the pivot.
*/

int partition(RestDist* rest_dist, int len, int pivot_idx) {
  //  Strategy:
  //  1. Swap the pivot to the end (at position len-1)
  swap_rest(&rest_dist[pivot_idx],&rest_dist[len-1]);
  pivot_idx=len-1;
  //  2. Keep two indices pointing at either end of the
  //     unprocessed array (initially 0 and len-2)
  uint32_t low=0;
  uint32_t high=len-2;
  //  3. If the item in the lower index is <= pivot value then increase
  //     the lower index. Similarly, if the value at the higher
  //     index exceeds the pivot value then decrease the
  //     higher index.
  //  4. If neither holds, swap them.
  while(true){
    if (high==low){
      break;
    }
    if(rest_dist[low].dist<=rest_dist[pivot_idx].dist){
      low++;
    }
    else if(rest_dist[high].dist>rest_dist[pivot_idx].dist){
      high--;
    }
    else {
      swap_rest(&rest_dist[high],&rest_dist[low]);
    }
  }
  //  5. Once the above loop finishes, put the pivot back
  //     into the correct position and return its new index.
  for(int i=0;i<pivot_idx;i++){
    if(rest_dist[pivot_idx].dist<rest_dist[i].dist){
      swap_rest(&rest_dist[pivot_idx],&rest_dist[i]);
      pivot_idx=i;
      break;
    }
  }
  return pivot_idx;
}

/*
For a given array this function picks a pivot, partitions it
(please read the description for the above function) and then recursivley does
the same for both sides of the pivot
*/

void qsort(RestDist* rest_dist, int len) {
  if (len <= 1) return; // sorted already

  // choose the pivot
  int pivot_idx = pick_pivot(rest_dist, len);

  // partition around the pivot and get the new
  // pivot position
  pivot_idx = partition(rest_dist, len, pivot_idx);

  // recurse on the halves before and after the pivot
  qsort(rest_dist, pivot_idx);
  qsort(rest_dist + pivot_idx + 1, len - pivot_idx - 1);
}

/*
This is one of the most vital parts of the code for mode 1. It goes through every
restaurant, calculates the distance between them and the cursor. Sorts the
restaurants in order of increasing distance while excluding all restaurants that
have a rating below the minimum rating set by the potentiometer.
*/

void top_restaurants(){
  restaurant r;
  int32_t x1;
  int32_t y1;
  int32_t rest_rating;
  skip=0;
  for (int i = 0; i < NUM_RESTAURANTS; ++i) {
  get_restaurant_fast(&r, i);
  rest_rating=floor(((r.rating+1)/2));
  if(rest_rating>=min_rating){
    x1 = lon_to_x(r.lon);
    y1 = lat_to_y(r.lat);
    all_restaurants[i-skip].index=i;
    all_restaurants[i-skip].dist= (abs((g_joyX+icol)-x1)+abs((g_joyY+irow)-y1));
  }
  else{
    skip+=1;
  }
  }
  number_of_rests_to_display=NUM_RESTAURANTS-skip;

  qsort(all_restaurants,number_of_rests_to_display);
}

/*
Given a list of names, a start point and an end point, this function will cycle
through the list of names and print the ones between the start and end points (inclusive)
*/

void print_names(int start, int end) {
  tft.setTextWrap(false);
  for (int i = start; i <= end; ++i) {
    int tft_row = (i-start)*8;
    int tft_col = 0;
    tft.setCursor(tft_col, tft_row);
    if (i == selection) {
      // black font, white backgrouns
      tft.setTextColor(ST7735_BLACK, ST7735_WHITE);
    }
    else {
      // white font, black background
      tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
    }
    tft.print(names[i]);
  }
}

/*
When a new name is selected this function changes the background of the last
selected name back to black and the text back to white and changes the current
selection background to white and the text to black.
*/

void update_names() {
  // print old selection normally
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  tft.setCursor(0, (old_selection%20)*8);
  tft.print(names[(old_selection%20)]);

  //highlight new selection
  tft.setTextColor(ST7735_BLACK, ST7735_WHITE);
  tft.setCursor(0, (selection%20)*8);
  tft.print(names[(selection%20)]);
}

/*
Scans the joystick vetically to see if the user is pressing up or down on the
joystick, if they are pressing up then it sets move_up to 1 which we will use later
as an indicator to determine to switch the highlighted name to the one above it,
and the same thing for move_down.
*/

void scanJoystickNames(){
  int vert = analogRead(JOY_VERT_ANALOG);
  if(abs(vert - y_center) > JOY_DEADZONE){
    update_name=1;
    int delta= vert-y_center;
    if (delta>0){
      move_down=1;
    }
    else if(delta<0){
      move_up=1;
    }
  }
}

/*
Prints the closest restaurant names to the lcd by calling the print_names function. Then
enters an infinite loop that allows the user to select a restaurant and once
the user selects a restaurant by pressing down the joystick the map and cursor
move to the position of the restaurant so that when we enter mode 0 the map and cursor
will be centered at the restaurant (unless its at the edge then as close as possible)
then breaks the loop.
*/

void print_to_lcd(){
  restaurant r;
  int k=0;
  int j=-1;
  int select;
  while(true){
    k=selection/20;
    k*=20;
    k=constrain(k,0,number_of_rests_to_display);
    if(k!=j){
      tft.fillScreen(ST7735_BLACK);
      for(int i=k;i<k+20;i++){
        get_restaurant_fast(&r,all_restaurants[i].index);
        strcpy(names[i-k],r.name);
        position[i-k][0]=lon_to_x(r.lon);
        position[i-k][1]=lat_to_y(r.lat);
    }
    if(k<number_of_rests_to_display-20){
      print_names(0, 20-1);
      update_names();
    }
    else{
      print_names(0, number_of_rests_to_display-k-1);
      tft.setTextColor(ST7735_BLACK, ST7735_WHITE);
      tft.setCursor(0, (selection%20)*8);
      tft.print(names[(selection%20)]);
    }
    j=k;
    }
    scanJoystickNames();
    if(update_name==1){
      old_selection = selection;
      if (move_down==1){
        selection = constrain(selection+1,0,number_of_rests_to_display-1);
      }
      else if(move_up==1){
      selection = constrain(selection-1,0,number_of_rests_to_display-1);
      }
      update_names();
      update_name=0;
      move_up=0;
      move_down=0;
    }
    delay(100);
    select = digitalRead(JOY_SEL);
    if(select==LOW){
      icol=constrain(position[selection-k][0]-TFT_WIDTH/2,0,2048-TFT_WIDTH);
      irow=constrain(position[selection-k][1]-TFT_HEIGHT/2,0,2048-TFT_HEIGHT);
      g_joyX=constrain(position[selection-k][0]-icol,2,TFT_WIDTH-3);
      g_joyY=constrain(position[selection-k][1]-irow,2,TFT_HEIGHT-3);
      mode=0;
      selection=0;
      old_selection=0;
      break;
    }
  }
}

/*
Scans the joystck and if the the joystick is past the deadzone then if moves the cursor
in the direction that the joystick is moving. The movement speed is inversly proportional
to SPEED_MODIFIER.
*/

void scanJoystick(){
  int vert = analogRead(JOY_VERT_ANALOG);
  int horiz = analogRead(JOY_HORIZ_ANALOG);
  int delta;
  if(abs(horiz - x_center) > JOY_DEADZONE){
    delta=(horiz -x_center)/SPEED_MODIFIER;
    g_joyX=constrain(g_joyX+delta,2,TFT_WIDTH-3);
    update=1;
  }
  if(abs(vert - y_center) > JOY_DEADZONE){
    delta=(vert -y_center)/SPEED_MODIFIER;
    g_joyY=constrain(g_joyY+delta,2,TFT_HEIGHT-3);
    update=1;
  }
}

/*
When the cursor is moved it first checks to see if it is at the edge of the screen,
if so then we redraw the map so that it gives the illusion of the cursor moving
to a diffrent section of the map. If not then it draws over the cursors old
position and redraws the cursor in its new position.
*/

void updateScreen(){
  if(g_joyX<=2 && icol>0){
    icol=constrain(icol-TFT_WIDTH,0,2048-TFT_WIDTH);
    lcd_image_draw(&map_image, &tft,
                   icol, irow,
                   0, 0,
                   TFT_WIDTH, TFT_HEIGHT);
    g_joyX=TFT_WIDTH/2;
    g_joyY=TFT_HEIGHT/2;
  }
  else if(g_joyX>=TFT_WIDTH-3 && icol<(2048-TFT_WIDTH)){
    icol=constrain(icol+TFT_WIDTH,0,2048-TFT_WIDTH);
    lcd_image_draw(&map_image, &tft,
                   icol, irow,
                   0, 0,
                   TFT_WIDTH, TFT_HEIGHT);
    g_joyX=TFT_WIDTH/2;
    g_joyY=TFT_HEIGHT/2;
  }
  else if(g_joyY<=2 && irow>0){
    irow=constrain(irow-TFT_HEIGHT,0,2048-TFT_HEIGHT);
    lcd_image_draw(&map_image, &tft,
                   icol, irow,
                   0, 0,
                   TFT_WIDTH, TFT_HEIGHT);
    g_joyX=TFT_WIDTH/2;
    g_joyY=TFT_HEIGHT/2;
  }
  else if(g_joyY>=TFT_HEIGHT-3 && irow<(2048-TFT_HEIGHT)){
    irow=constrain(irow+TFT_HEIGHT,0,2048-TFT_HEIGHT);
    lcd_image_draw(&map_image, &tft,
                   icol, irow,
                   0, 0,
                   TFT_WIDTH, TFT_HEIGHT);
    g_joyX=TFT_WIDTH/2;
    g_joyY=TFT_HEIGHT/2;
  }
  else {
    lcd_image_draw(&map_image, &tft, g_cursorX-2+icol, g_cursorY-2+irow, g_cursorX-2, g_cursorY-2, 5, 5);
    tft.drawLine(g_joyX-2,g_joyY-2,g_joyX+2,g_joyY-2,ST7735_RED);
    tft.drawLine(g_joyX-2,g_joyY-1,g_joyX+2,g_joyY-1,ST7735_RED);
    tft.drawLine(g_joyX-2,g_joyY,g_joyX+2,g_joyY,ST7735_RED);
    tft.drawLine(g_joyX-2,g_joyY+1,g_joyX+2,g_joyY+1,ST7735_RED);
    tft.drawLine(g_joyX-2,g_joyY+2,g_joyX+2,g_joyY+2,ST7735_RED);
    g_cursorX=g_joyX;
    g_cursorY=g_joyY;
    update = 0;
  }
}

/*
Runs mode 0, fist draws the map and updates the screen to draw the cursor. Then
enters an infinite loop waiting for the user to move the joystick and when they
do then it updates the screen again, it does all of this while keeping a somewhat
consistent framerate.
*/

void run_mode_0(){
  lcd_image_draw(&map_image, &tft,
                 icol, irow,
                 0, 0,
                 TFT_WIDTH, TFT_HEIGHT);
  updateScreen();
  int prevTime = millis();
  int t;
  int select;
  while(true){
    readPOT=analogRead(RATING_DIAL);
    if(readPOT!= lastPOT){
      update_LEDs();
      lastPOT=readPOT;
    }
    select = digitalRead(JOY_SEL);
    if(select==LOW){
      mode=1;
      break;
    }
    scanJoystick();
    if (update==1){
      updateScreen();
    }
    t = millis();
    if (t - prevTime < MILLIS_PER_FRAME) {
      delay(MILLIS_PER_FRAME - (t - prevTime));
    }
    prevTime = millis();
  }
}

/*
Runs mode 1, flls the screen with black gets the top 20 closest restaurants and
then prints them to the lcd where it stays in an infinite loop until broken.
*/

void run_mode_1(){
  tft.fillScreen(ST7735_BLACK);
  top_restaurants();
  print_to_lcd();
}

/*
The main function. Runs on an infinite loop while conintually checking to see which
mode to run. This is based off of the value of mode.
Then finally (never will be reached) ends the serial communication and returns 0.
*/

int main(){
  setup();
  while(true){
    if(mode==0){
      run_mode_0();
    }
    else if (mode==1){
      run_mode_1();
    }
  }
  Serial.end();
  return 0;
}
