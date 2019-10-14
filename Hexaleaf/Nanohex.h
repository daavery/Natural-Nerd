#include <stdint.h>
#include <FastLED.h>

/* Number of LEDs in each box/leaf */
#define LEDS_IN_BOX 20
/*The number of boxes */
#define NUM_BOXES 6
/* The pin the LED is connected to */
#define LED_PIN 19
/*Don't change unless you know what you're doing */
#define TOTAL_LEDS LEDS_IN_BOX *NUM_BOXES

CRGB leds[TOTAL_LEDS];

union FadeVector {
  struct
  {
    float r;
    float g;
    float b;

  };
  struct
  {
    float h;
    float s;
    float v;
  };
};

class Hexnode
{
  private:
    uint16_t ledStart;
    uint16_t ledEnd;
    uint16_t peakDelay;
    uint16_t fadeTimeMs;
    uint16_t pulseCount;
    uint16_t numberOfPulses;
    uint16_t colorIndex;
    uint16_t rainbowSpeed;
    CRGB color;
    CRGB colorTo;
    CRGB colorAlt;
    CRGBPalette16 colorPalette;
    unsigned long startDrawTimer;
    unsigned long lastDrawTimer;
    unsigned long peakTimer;
    unsigned long rainbowTimer;
    bool animating;
    bool pulsing;
    bool rainbow;
    FadeVector fadeVector;
    TBlendType    currentBlending;

  public:
    Hexnode(uint8_t index) : animating(false),
      pulsing(false),
      rainbow(false),
      fadeTimeMs(1000),
      numberOfPulses(0),
      color(CRGB(0, 0, 0)),
      colorPalette(CRGB::Black),
      rainbowSpeed(2)

    {
      ledStart = index * LEDS_IN_BOX;
      ledEnd = ledStart + LEDS_IN_BOX - 1;
      currentBlending = LINEARBLEND;
    }

    void compute_fade_vector()
    {
      fadeVector.r = (float)(colorTo.r - color.r) / (float)fadeTimeMs;
      fadeVector.g = (float)(colorTo.g - color.g) / (float)fadeTimeMs;
      fadeVector.b = (float)(colorTo.b - color.b) / (float)fadeTimeMs;
    }

    void set_color(CRGB c)
    {
      colorTo = c;
      lastDrawTimer = millis();
      startDrawTimer = millis();
      animating = true;
      rainbow = false;
      compute_fade_vector();
    }

    void set_static_color(CRGB c)
    {
      pulsing = false;
      animating = false;
      rainbow = false;
      color = c;
    }
    void set_rainbow(CRGBPalette16 inPalette, uint16_t pSpeed, uint16_t count, uint16_t rSpeed, bool force)
    {
      pulsing = false;
      animating = false;
      rainbow = true;
      fadeTimeMs = pSpeed;
      pulseCount = count;
      colorPalette = inPalette;
      rainbowSpeed = rSpeed;
    }
    void start_pulse(CRGB to, CRGB from, uint16_t pDelay, uint16_t pSpeed, uint16_t count, bool force)
    {
      if (!pulsing || force)
      {
        set_color(to);
        colorAlt = from;
        peakDelay = pDelay;
        fadeTimeMs = pSpeed;
        pulseCount = count;
        pulsing = true;
        rainbow = false;
      }
    }

    void set_fade_time(uint16_t t)
    {
      fadeTimeMs = t;
    }

    void color_update()
    {
      unsigned long delta_ms = millis() - lastDrawTimer;
      lastDrawTimer = millis();
      int16_t r = color.r + (fadeVector.r * delta_ms);
      int16_t g = color.g + (fadeVector.g * delta_ms);
      int16_t b = color.b + (fadeVector.b * delta_ms);
      (r >= 255) ? color.r = 255 : (r <= 0) ? color.r = 0 : color.r = r;
      (g >= 255) ? color.g = 255 : (g <= 0) ? color.g = 0 : color.g = g;
      (b >= 255) ? color.b = 255 : (b <= 0) ? color.b = 0 : color.b = b;
    }

    int draw()
    {
      if (pulsing && !animating)
      {
        if (millis() - peakTimer > peakDelay)
        {
          if (numberOfPulses++ >= pulseCount)
          {
            pulsing = false;
            numberOfPulses = 0;
          }
          else
          {
            if ((numberOfPulses % 2) == 0)
            {
              set_color(colorAlt);
              color = colorAlt;
            }
            else
            {
              set_color(colorTo);
              color = colorTo;
            }
            Serial.printf("p%d %d\n", numberOfPulses, numberOfPulses % 2);
            peakTimer = millis();
            //                    CRGB tmp = colorAlt;
            //                    colorAlt = colorTo;
            //                    set_color(tmp);
          }
        }
      }
      else if (animating)
      {
        color_update();
        if (millis() - startDrawTimer >= fadeTimeMs)
        {
          animating = false;
          peakTimer = millis();
        }
      }
      if (rainbow)
      {

        uint8_t brightness = 255;
        uint16_t a;
        if (millis() - rainbowTimer >= fadeTimeMs)
        {
          colorIndex += rainbowSpeed;
          rainbowTimer = millis();
          if (colorIndex > 255) colorIndex = 0;
          Serial.printf("%d\n", colorIndex);
          a = colorIndex;
          for (uint16_t ledPos = ledStart; ledPos <= ledEnd; ledPos++)
          {
            leds[ledPos] = ColorFromPalette( colorPalette, a, brightness, currentBlending);
            a += pulseCount;
            if ( a >= 255) a = 0;
          }
        }
      }
      else
      {
        for (uint16_t ledPos = ledStart; ledPos <= ledEnd; ledPos++)
        {
          leds[ledPos] = color;
        }
      }
      return 0;
    }
};

class Nanohex
{
  private:
    Hexnode *nodes[NUM_BOXES];
    uint16_t drawEveryNthMs;
    unsigned long lastDrew;
    unsigned long modeTimer;
    CRGB primary;
    CRGB secondary;
    uint32_t fadeTimeMin;
    uint32_t fadeTimeMax;
    uint16_t mode;
    uint16_t pulseCount1;
    uint16_t flashTime;
    uint8_t hueRandomness;
    bool interrupt;
    CRGB pattern[LEDS_IN_BOX];
    CRGBPalette16 palette;

  public:
    Nanohex() : lastDrew(0),
      modeTimer(0),
      mode(1),
      drawEveryNthMs(33),
      primary(CRGB(0, 60, 120)),
      secondary(CRGB(0, 0, 0)),
      fadeTimeMin(3000),
      fadeTimeMax(7000),
      hueRandomness(50),
      pulseCount1(1),
      interrupt(false),
      flashTime(300)
    {
      FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, TOTAL_LEDS);
      for (uint8_t i = 0; i < NUM_BOXES; i++)
        nodes[i] = new Hexnode(i);
    }

    void set_hue_randomness(uint8_t val)
    {
      hueRandomness = val;
      Serial.printf("Random %d \n", hueRandomness);
      interrupt = true;
    }

    void set_refresh_rate(uint8_t val)
    {
      drawEveryNthMs = val;
    }

    void set_fadetime_min(uint32_t val)
    {
      if (val > fadeTimeMax)
        val = fadeTimeMax;
      fadeTimeMin = val;
      Serial.printf("Minimum fade time %d s\n", (fadeTimeMin / 1000));
      interrupt = true;
    }

    void set_fadetime_max(uint32_t val)
    {
      if (val < fadeTimeMin)
        val = fadeTimeMin;
      fadeTimeMax = val;
      Serial.printf("Maximum fade time %d s \n", (fadeTimeMax / 1000));
      interrupt = true;
    }

    void set_primary(CRGB c)
    {
      modeTimer = 0;
      primary = c;
      interrupt = true;
    }

    void set_secondary(CRGB c)
    {
      modeTimer = 0;
      secondary = c;
      interrupt = true;
    }

    void set_color_of(uint8_t idx, CRGB color)
    {
      nodes[idx]->set_color(color);
    }

    void set_mode(uint8_t m)
    {
      modeTimer = 0;
      mode = m;
    }
    void set_pulse_count(uint8_t c)
    {
      pulseCount1 = c;
      Serial.printf("Pulse Count %d\n", pulseCount1);
    }
    void set_flash_time(uint8_t c)
    {
      flashTime = c;
      Serial.printf("Flash Time %d\n", flashTime);
    }
    void mode_static_color()
    {
      for (uint8_t i = 0; i < NUM_BOXES; i++)
        nodes[i]->set_static_color(primary);
    }

    void mode_binary_twinkle()
    {
      for (uint8_t i = 0; i < NUM_BOXES; i++)
        nodes[i]->start_pulse(primary, secondary, flashTime, random(fadeTimeMin, fadeTimeMax), pulseCount1, interrupt);

    }

    void mode_hue_twinkle()
    {
      CHSV c = rgb2hsv_approximate(primary);
      for (uint8_t i = 0; i < NUM_BOXES; i++)
      {
        CHSV color2 = CHSV(c.hue + random(-hueRandomness / 2, hueRandomness / 2), 255, 255);
        CHSV color1 = CHSV(c.hue + random(-hueRandomness / 2, hueRandomness / 2), c.sat, c.val);
        nodes[i]->start_pulse(color1, color2, flashTime, random(fadeTimeMin, fadeTimeMax), pulseCount1, interrupt);
      }
    }

    void mode_rainbow()
    {
      palette = CRGBPalette16(primary, secondary, primary);
      for (uint8_t i = 0; i < NUM_BOXES; i++)
      {
        nodes[i]->set_rainbow(palette, random(fadeTimeMin / 10, fadeTimeMax / 10), pulseCount1, hueRandomness, interrupt);
      }
    }

    void mode_rainbow2()
    {
      palette = RainbowColors_p;
      for (uint8_t i = 0; i < NUM_BOXES; i++)
      {
        nodes[i]->set_rainbow(palette, random(fadeTimeMin / 10, fadeTimeMax / 10), pulseCount1, hueRandomness, interrupt);
      }
    }

    void update()
    {
      if (millis() - modeTimer > 3000)
      {
        switch (mode)
        {
          case 1:
            mode_static_color();
            break;
          case 2:
            mode_binary_twinkle();
            break;
          case 3:
            mode_hue_twinkle();
            break;
          case 4:
            mode_rainbow();
            break;
          case 5:
            mode_rainbow2();
            break;
          default:
            mode_static_color();
            break;
        }
        if (interrupt)
          interrupt = false;
        modeTimer = millis();
      }

      if (millis() - lastDrew > drawEveryNthMs)
      {
        for (uint8_t i = 0; i < NUM_BOXES; i++)
          int ret = nodes[i]->draw();
        FastLED.show();
        lastDrew = millis();
      }
    }

    void update_fade_time(uint16_t ms)
    {
      for (uint8_t i = 0; i < NUM_BOXES; i++)
        nodes[i]->set_fade_time(ms);
    }

    void color_all(CRGB color)
    {
      for (uint8_t i = 0; i < NUM_BOXES; i++)
        set_color_of(i, color);
    }
};
