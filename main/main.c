#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "st7735s.h"
#include "fontx.h"

#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

// SD
#define SD_GPIO_CS    4
#define MOUNT_POINT "/sdcard"

// M5stickC stuff
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 80
#define OFFSET_X 0
#define OFFSET_Y 24
#define GPIO_MOSI 5
#define GPIO_MISO 6
#define GPIO_SCLK 7
#define GPIO_CS 2
#define GPIO_DC 3
#define GPIO_RESET 10

#define INTERVAL 10
#define WAIT vTaskDelay(INTERVAL)

#define FX16_WIDTH  8
#define FX16_HEIGHT  16

static const char *TAG = "DDD Flappy Bird";
//lcd controller
ST7735_t tft;
// set font file
FontxFile fx16[2];
uint8_t ascii[20] = {0};


///////////////////////

#define BUTTON  9
#define HIGH 	1
#define LOW		0
// TTGO: 
#define TFTW SCREEN_WIDTH                 // screen width
#define TFTH SCREEN_HEIGHT                 // screen height
#define TFTW2 (TFTW / 2) // half screen width
#define TFTH2 (TFTH / 2) // half screen height

// game constant
#define SPEED 1
#define GRAVITY 6.5
#define JUMP_FORCE 1.15
#define SKIP_TICKS 15.0 // 1000 / 50fps
#define MAX_FRAMESKIP 2

// bird size
#define BIRDW 15 // bird width
#define BIRDH 10 // bird height
#define BIRDW2 8 // half width
#define BIRDH2 5 // half height
// pipe size
#define PIPEW 12         // pipe width
#define GAPHEIGHT 33 // pipe gap height
// floor size
#define FLOORH 20 // floor height (from bottom of the screen)
// grass size
#define GRASSH 4 // grass height (inside floor, starts at floor y)

#define GAMEH  (TFTH - FLOORH)
#define GAMEH2 (GAMEH / 2)

#define COLOR565(r, g, b) ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

uint16_t maxScore = 0;

// background
#define BCKGRDCOL COLOR565(138, 235, 244)
// bird
#define BIRDCOL COLOR565(255, 254, 174)
// pipe
#define PIPECOL COLOR565(99, 255, 78)
// pipe highlight
#define PIPEHIGHCOL COLOR565(250, 255, 250)
// pipe seam
#define PIPESEAMCOL COLOR565(0, 0, 0)
// floor
#define FLOORCOL COLOR565(246, 240, 163)
// grass (col2 is the stripe color)
#define GRASSCOL COLOR565(141, 225, 87)
#define GRASSCOL2 COLOR565(156, 239, 88)

// bird sprite
// bird sprite colors (Cx name for values to keep the array readable)
#define C0 BCKGRDCOL
#define C1 COLOR565(0, 0, 0)
#define C2 BIRDCOL
#define C3 WHITE
#define C4 RED
#define C5 COLOR565(251, 216, 114)

// static const unsigned int birdcol[] = {
//     C0, C0, C1, C1, C1, C1, C1, C0, C0, C0, C1, C1, C1, C1, C1, C0,
//     C0, C1, C2, C2, C2, C1, C3, C1, C0, C1, C2, C2, C2, C1, C3, C1,
//     C0, C2, C2, C2, C2, C1, C3, C1, C0, C2, C2, C2, C2, C1, C3, C1,
//     C1, C1, C1, C2, C2, C3, C1, C1, C1, C1, C1, C2, C2, C3, C1, C1,
//     C1, C2, C2, C2, C2, C2, C4, C4, C1, C2, C2, C2, C2, C2, C4, C4,
//     C1, C2, C2, C2, C1, C5, C4, C0, C1, C2, C2, C2, C1, C5, C4, C0,
//     C0, C1, C2, C1, C5, C5, C5, C0, C0, C1, C2, C1, C5, C5, C5, C0,
//     C0, C0, C1, C5, C5, C5, C0, C0, C0, C0, C1, C5, C5, C5, C0, C0
// };
static const unsigned int birdcol[] = {
    C0,     C0,     C0,     C0,     C0,     C0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     C0,     C0,     C0, 
    C0,     C0,     C0,     C0, 0x0000, 0x0000, 0xffd7, 0xffd7, 0xff80, 0x0000, 0xffff, 0xffff, 0x0000,     C0,     C0, 
    C0,     C0,     C0, 0x0000, 0xffd7, 0xff80, 0xff80, 0xff80, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x0000,     C0, 
    C0, 0x0000, 0x0000, 0x0000, 0x0000, 0xff80, 0xff80, 0xff80, 0x0000, 0xffff, 0xffff, 0xffff, 0x0000, 0xffff, 0x0000, 
0x0000, 0xffd7, 0xffff, 0xffff, 0xffff, 0x0000, 0xff80, 0xff80, 0xff80, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0x0000, 
0x0000, 0xff80, 0xffff, 0xffff, 0xfe01, 0x0000, 0xff80, 0xff80, 0xff80, 0x0000, 0xe8e4, 0xe8e4, 0xe8e4, 0xe8e4, 0xe8e4, 
    C0, 0x0000, 0xfe01, 0xfe01, 0x0000, 0xfe01, 0xfe01, 0xfe01, 0x0000, 0xe8e4, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    C0,     C0, 0x0000, 0x0000, 0xfe01, 0xfe01, 0xfe01, 0xfe01, 0xfe01, 0x0000, 0xe8e4, 0xe8e4, 0xe8e4, 0xe8e4, 0x0000, 
    C0,     C0,     C0, 0x0000, 0x0000, 0xfe01, 0xfe01, 0xfe01, 0xfe01, 0xfe01, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
    C0,     C0,     C0,     C0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,     C0,     C0,     C0,     C0
};

// bird structure
static struct BIRD
{
    int16_t x, y, old_y;
    int16_t col;
    float vel_y;
} bird;

// pipe structure
static struct PIPES
{
    int16_t x, gap_y;
    int16_t col;
} pipes;

// score
int score = 0;
// temporary x and y var
static short tmpx, tmpy;

void game_init()
{
    // clear screen
	lcdFillScreen(&tft, BCKGRDCOL);
    // reset score
    score = 0;
    // init bird
    bird.x = 20;
    bird.y = bird.old_y = TFTH2 - BIRDH2;
    bird.vel_y = -JUMP_FORCE;
    tmpx = tmpy = 0;
    // generate new random seed for the pipe gape
    // init pipe
    pipes.x = TFTW;
    pipes.gap_y = 5 + random() % (GAMEH - (15 + GAPHEIGHT));
}

void game_start() 
{
	lcdFillScreen(&tft, BLACK);
	strcpy((char *)ascii, "FLAPPY");
	lcdDrawString(&tft, fx16, TFTW2 - (3 * FX16_WIDTH), TFTH2 - FX16_HEIGHT, ascii, WHITE);
	strcpy((char *)ascii, "-BIRD-");
	lcdDrawString(&tft, fx16, TFTW2 - (3 * FX16_WIDTH), TFTH2 + FX16_HEIGHT * 2, ascii, WHITE);

    //set the GPIO as a input
    gpio_pad_select_gpio(BUTTON);
    gpio_reset_pin(BUTTON);
    gpio_set_direction(BUTTON, GPIO_MODE_DEF_INPUT);

    // wait for push button
    while (gpio_get_level(BUTTON) == HIGH)
    {
        WAIT;
    }
    
    // init game settings
    game_init();
}

uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

bool check_collision_and_update_score()
{
    // passed pipe flag to count score
    static bool passed_pipe = false;
    // ===============
    // collision
    // ===============
    // if the bird hit the ground game over
    if (bird.y > GAMEH - BIRDH)
        return false;

    // checking for bird collision with pipe
    if ((bird.x + BIRDW >= pipes.x - BIRDW2) && (bird.x <= pipes.x + PIPEW - BIRDW))
    {
        // bird entered a pipe, check for collision
        if ((bird.y < pipes.gap_y) || bird.y + BIRDH > (pipes.gap_y + GAPHEIGHT))
            return false;
        else
            passed_pipe = true;
    }
    // if bird has passed the pipe increase score
    else if ((bird.x > pipes.x + PIPEW - BIRDW) && passed_pipe)
    {
        passed_pipe = false;
        // erase score with background color
        sprintf((char *)ascii, "%d", score);
        lcdDrawString(&tft, fx16, TFTW - 20, 16, ascii, BCKGRDCOL);
        // increase score since we successfully passed a pipe
        score++;
    }
    // update score
    // ---------------
    // set text color back to white for new score
    sprintf((char *)ascii, "%d", score);
    lcdDrawString(&tft, fx16, TFTW - 20, 16, ascii, RED);

    return true;
}

void draw_pipe()
{
    // ===============
    // draw pipe
    // ===============
    // we save cycles if we avoid drawing the pipe when outside the screen
    if (pipes.x >= 0 && pipes.x + PIPEW < TFTW)
    {
        // Top pipe color
        lcdDrawFillRect(&tft, pipes.x, 0, pipes.x + PIPEW, pipes.gap_y, PIPECOL);
        // border left
        lcdDrawFillRect(&tft, pipes.x, 0, pipes.x, pipes.gap_y, BLACK);
        // border right
        lcdDrawFillRect(&tft, pipes.x+ PIPEW, 0, pipes.x + PIPEW, pipes.gap_y, BLACK);
        // l
        lcdDrawFillRect(&tft, pipes.x -1, pipes.gap_y -4, pipes.x -1, pipes.gap_y, BLACK);
        // r
        lcdDrawFillRect(&tft, pipes.x + PIPEW, pipes.gap_y - 4, pipes.x + PIPEW, pipes.gap_y, BLACK);
        // u
        lcdDrawFillRect(&tft, pipes.x, pipes.gap_y - 4, pipes.x + PIPEW, pipes.gap_y - 4, BLACK);
        // d
        lcdDrawFillRect(&tft, pipes.x, pipes.gap_y, pipes.x + PIPEW, pipes.gap_y, BLACK);

        // Bottom pipe color
        lcdDrawFillRect(&tft, pipes.x, pipes.gap_y + GAPHEIGHT, pipes.x + PIPEW, GAMEH, PIPECOL);
        // border left
        lcdDrawFillRect(&tft, pipes.x, pipes.gap_y + GAPHEIGHT, pipes.x, GAMEH, BLACK);
        // border right
        lcdDrawFillRect(&tft, pipes.x+ PIPEW, pipes.gap_y + GAPHEIGHT, pipes.x + PIPEW, GAMEH, BLACK);
        // l
        lcdDrawFillRect(&tft, pipes.x -1, pipes.gap_y + GAPHEIGHT, pipes.x -1, pipes.gap_y + GAPHEIGHT + 4, BLACK);
        // r
        lcdDrawFillRect(&tft, pipes.x + PIPEW, pipes.gap_y + GAPHEIGHT, pipes.x + PIPEW, pipes.gap_y + GAPHEIGHT + 4, BLACK);
        // u
        lcdDrawFillRect(&tft, pipes.x, pipes.gap_y + GAPHEIGHT + 4, pipes.x + PIPEW, pipes.gap_y + GAPHEIGHT + 4, BLACK);
        // d
        lcdDrawFillRect(&tft, pipes.x, pipes.gap_y + GAPHEIGHT, pipes.x + PIPEW, pipes.gap_y + GAPHEIGHT, BLACK);
        
    }
    // erase behind pipe
    if (pipes.x <= TFTW)
    {
        lcdDrawFillRect(&tft, pipes.x + PIPEW +1, 0, pipes.x + PIPEW + MAX_FRAMESKIP * SPEED, pipes.gap_y + 1, BCKGRDCOL);
        lcdDrawFillRect(&tft, pipes.x + PIPEW +1, pipes.gap_y + GAPHEIGHT - 1, pipes.x + PIPEW + MAX_FRAMESKIP * SPEED, GAMEH, BCKGRDCOL);
    }
}

void draw_bird()
{
    // temp var for setAddrWindow
    unsigned char px;

    // bird
    // ---------------
    tmpx = BIRDW - 1;
    do
    {
        px = bird.x + tmpx + BIRDW;
        // clear bird at previous position stored in old_y
        // we can't just erase the pixels before and after current position
        // because of the non-linear bird movement (it would leave 'dirty' pixels)
        tmpy = BIRDH - 1;
        do
        {
            lcdDrawPixel(&tft, px, bird.old_y + tmpy, BCKGRDCOL);
        } while (tmpy--);

        // draw bird sprite at new position
        tmpy = BIRDH - 1;
        do
        {
            lcdDrawPixel(&tft, px, bird.y + tmpy, birdcol[tmpx + (tmpy * BIRDW)]);
        } while (tmpy--);

    } while (tmpx--);
    // save position to erase bird on next draw
    bird.old_y = bird.y;
}

void draw_floor()
{
    // draw floor
    // ===============    
    // draw the floor once, we will not overwrite on this area in-game
    // black line
	lcdDrawLine(&tft, 0, GAMEH + 1, TFTW, GAMEH + 1, BLACK);
    // grass and stripe
    lcdDrawFillRect(&tft, 0, GAMEH + 2, TFTW, GAMEH + 1 + GRASSH - 1, GRASSCOL);
    // black line
	lcdDrawLine(&tft, 0, GAMEH + GRASSH, TFTW, GAMEH + GRASSH, BLACK);
    // mud
    lcdDrawFillRect(&tft, 0, GAMEH + GRASSH + 1, TFTW, TFTH, FLOORCOL);
}

// void draw_grassstripes()
// {
//     // grass x position (for stripe animation)
//     int16_t grassx = TFTW;
//     // grass stripes
//     // ---------------
//     grassx -= SPEED;
//     if (grassx < 0)
//         grassx = TFTW;

//     lcdDrawLine(&tft, grassx % TFTW, GAMEH + 1, grassx % TFTW, GRASSH - 1, GRASSCOL);
//     lcdDrawLine(&tft, (grassx + 10) % TFTW, GAMEH + 1, (grassx + 10) % TFTW, GRASSH - 1, GRASSCOL2);
// }

void game_loop()
{
    bool check;
    // ===============
    // prepare game variables

    // game loop time variables
    double delta, old_time, next_game_tick, current_time;
    next_game_tick = current_time = millis();
    
    draw_floor();

    while (true)
    {
        int loops = 0;
        while (millis() > next_game_tick && loops < MAX_FRAMESKIP)
        {
            // ===============
            // input
            // ===============
            if (gpio_get_level(BUTTON) == LOW)
            {
                // if the bird is not too close to the top of the screen apply jump force
                if (bird.y > BIRDH2 * 0.5)
                    bird.vel_y = -JUMP_FORCE;
                // else zero velocity
                else
                    bird.vel_y = 0;
            }

            // ===============
            // update
            // ===============
            // calculate delta time
            // ---------------
            old_time = current_time;
            current_time = millis();
            delta = (current_time - old_time) / 1000;

            // bird
            // ---------------
            bird.vel_y += GRAVITY * delta;
            bird.y += bird.vel_y;

            // pipe
            // ---------------
            pipes.x -= SPEED;
            // if pipe reached edge of the screen reset its position and gap
            if (pipes.x < -PIPEW)
            {
                pipes.x = TFTW;
                pipes.gap_y = 5 + random() % (GAMEH - (15 + GAPHEIGHT));
            }

            // ---------------
            next_game_tick += SKIP_TICKS;
            loops++;
        }

        draw_pipe();
        draw_bird();
        // draw_grassstripes();

        check = check_collision_and_update_score();
        if(!check)
            break;
    }
    // add a small delay to show how the player lost
    WAIT;
}

// ---------------
// game over
// ---------------
void game_over()
{
	// lcdFillScreen(&tft, BLACK);
    // maxScore = EEPROM.readUInt(0);
	// maxScore = 100;

    // if (score > maxScore)
    // {
    //     // EEPROM.writeUInt(0, score);
    //     maxScore = score;
	// 	strcpy((char *)ascii, "NEW HIGHSCORE");
	// 	lcdDrawString(&tft, fx16, TFTW2 - (13 * 6), TFTH2 - 26, ascii, RED);
    // }

    // half width - num char * char width in pixels
	strcpy((char *)ascii, "GAME OVER");
	lcdDrawString(&tft, fx16, TFTW2 - (4 * FX16_WIDTH), TFTH2, ascii, RED);
	// lcdDrawString(&tft, fx16, 10, 20, String(score), WHITE);
    // tft.setCursor(TFTW2 - (12 * 6), TFTH2 + 18);
    // tft.println("press button");
    // tft.setCursor(10, 28);
    // tft.print("Max Score:");
    // tft.print(maxScore);

    // wait for push button
    while (gpio_get_level(BUTTON) == HIGH)
    {
        WAIT;
    }

    lcdFillScreen(&tft, BLACK);
    WAIT;
        
}
///////////////////////

// static void SPIFFS_Directory(char * path) {
//     DIR* dir = opendir(path);
//     assert(dir != NULL);
//     while (true) {
//         struct dirent*pe = readdir(dir);
//         if (!pe) break;
//         ESP_LOGI(TAG,"d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
//     }
//     closedir(dir);
// }
void font_init()
{
    InitFontx(fx16,"/sdcard/font/ILGH16XB.FNT",""); // 8x16Dot Gothic
    uint8_t buffer[FontxGlyphBufSize];
    uint8_t fontWidth;
	uint8_t fontHeight;
	GetFontx(fx16, 0, buffer, &fontWidth, &fontHeight);
	ESP_LOGI(TAG,"fontWidth=%d fontHeight=%d",fontWidth,fontHeight);
}

void tft_init()
{
    lcdInit(&tft, SCREEN_WIDTH, SCREEN_HEIGHT, OFFSET_X, OFFSET_Y);
    // lcdFillScreen(&tft, BLACK);
}

void sd_card_init()
{
    esp_err_t ret;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    // ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_GPIO_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // get font
    font_init();

    // // All done, unmount partition and disable SPI peripheral
    // esp_vfs_fat_sdcard_unmount(mount_point, card);
    // ESP_LOGI(TAG, "Card unmounted");
    // //deinitialize the bus after all devices are removed
    // spi_bus_free(host.slot);
}


void app_main(void)
{
    spi_master_init(&tft, GPIO_MOSI, GPIO_MISO, GPIO_SCLK, GPIO_CS, GPIO_DC, GPIO_RESET);
    
    sd_card_init();
    tft_init();
    
    ESP_LOGD(TAG, "game_start");
    while (1) {
        game_start();
        game_loop();
        game_over();
		WAIT;
    } // end while
}

