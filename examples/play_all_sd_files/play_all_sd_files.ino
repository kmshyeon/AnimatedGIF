#include <AnimatedGIF.h>
#include <bb_spi_lcd.h>
#include <SPI.h>
#include <SD.h>

// Demo sketch to play all GIF files in a directory
// Tested on TTGO T-Camera Plus

#define LED_PIN       2
#define CS_PIN        12
#define SD_CS_PIN 0
#define RESET_PIN     -1
#define DC_PIN        15
#define MOSI_PIN 19
#define SCK_PIN 21
#define MISO_PIN 22


AnimatedGIF gif;
File f;
int x_offset, y_offset;

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y;

    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    
    s = pDraw->pPixels;
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
      int x, iCount;
      pEnd = s + pDraw->iWidth;
      x = 0;
      iCount = 0; // count non-transparent pixels
      while(x < pDraw->iWidth)
      {
        c = ucTransparent-1;
        d = usTemp;
        while (c != ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent) // done, stop
          {
            s--; // back up to treat it like transparent
          }
          else // opaque
          {
             *d++ = usPalette[c];
             iCount++;
          }
        } // while looking for opaque pixels
        if (iCount) // any opaque pixels?
        {
          spilcdSetPosition(pDraw->iX+x+x_offset, y+y_offset, iCount, 1, 1);
          spilcdWriteDataBlock((uint8_t *)usTemp, iCount*2, 1);
          x += iCount;
          iCount = 0;
        }
        // no, look for a run of transparent pixels
        c = ucTransparent;
        while (c == ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent)
             iCount++;
          else
             s--; 
        }
        if (iCount)
        {
          x += iCount; // skip these
          iCount = 0;
        }
      }
    }
    else
    {
      s = pDraw->pPixels;
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      for (x=0; x<pDraw->iWidth; x++)
        usTemp[x] = usPalette[*s++];
      spilcdSetPosition(pDraw->iX+x_offset, y+y_offset, pDraw->iWidth, 1, 1);
      spilcdWriteDataBlock((uint8_t *)usTemp, pDraw->iWidth*2, 1);
    }
} /* GIFDraw() */

void * GIFOpenFile(char *fname, int32_t *pSize)
{
  f = SD.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{ 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

void setup() {
  Serial.begin(115200);
  while (!Serial);

// Need to initialize SPI before calling SD.begin()
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card init failed!");
    while (1); // SD initialisation failed so wait here
  }
  Serial.println("SD Card init success!");

  spilcdInit(LCD_ST7789, 0, 0, 0, 40000000, CS_PIN, DC_PIN, RESET_PIN, LED_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);
  gif.begin(BIG_ENDIAN_PIXELS);
}

void ShowGIF(char *name)
{
  spilcdFill(0,1);
  
  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    x_offset = (240 - gif.getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (240 - gif.getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    while (gif.playFrame(true, NULL))
    {      
    }
    gif.close();
  }

} /* ShowGIF() */

//
// Return true if a file's leaf name starts with a "." (it's been erased)
//
int ErasedFile(char *fname)
{
int iLen = strlen(fname);
int i;
  for (i=iLen-1; i>0; i--) // find start of leaf name
  {
    if (fname[i] == '/')
       break;
  }
  return (fname[i+1] == '.'); // found a dot?
}
void loop() {

char *szDir = "/GIF"; // play all GIFs in this directory on the SD card
char fname[256];
File root, temp;

   while (1) // run forever
   {
      root = SD.open(szDir);
      if (root)
      {
         temp = root.openNextFile();
            while (temp)
            {
              if (!temp.isDirectory()) // play it
              {
                strcpy(fname, temp.name());
                if (!ErasedFile(fname))
                {
                  Serial.printf("Playing %s\n", temp.name());
                  Serial.flush();
                  ShowGIF((char *)temp.name());
                }
              }
              temp.close();
              temp = root.openNextFile();
            }
         root.close();
      } // root
      delay(4000); // pause before restarting
   } // while
} /* loop() */
