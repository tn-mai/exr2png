/** @file man.cpp
*/
#include <ImfRgbaFile.h>
#include <ImfArray.h>
#include <iostream>
#include <algorithm>
#include <memory>

enum class Result {
  Success = 0;
  Error = 1;
};

void ReadRgbaExrFile(const char* filename, Imf::Array2D<Imf::Rgba>* pPixels, int* pWidth, int* pHeight) {
  Imf::RgbaInputFile file (fileName);
  Imf::Box2i dw = file.dataWindow();

  if (pWidth) {
    *pWidth  = dw.max.x - dw.min.x + 1;
  }
  if (pHeight) {
    *pHeight = dw.max.y - dw.min.y + 1;
  }
  if (pPixels) {
    pPixels->resizeErase (height, width);
    file.setFrameBuffer(&(*pPixels)[0][0] - dw.min.x - dw.min.y * width, 1, width);
    file.readPixels(dw.min.y, dw.max.y);
  }
}

void PrintUsage() {
}

void SetPixel(png_bytep buf, const Imf::Rgba& rgba) {
  uint8_t* p = static_cast<uint8_t*>(buf);
  half biggest = rgba.r;
  if (biggest < rgba.g) biggest = rgba.g;
  if (biggest < rgba.b) biggest = rgba.b;
  if (biggest < 255.0f) {
    p[0] = static_cast<uint8_t>(rgba.r);
    p[1] = static_cast<uint8_t>(rgba.g);
    p[2] = static_cast<uint8_t>(rgba.b);
    p[3] = 255;
  } else {
    const float strength = biggest / 255.0f;
    p[0] = std::min(255, static_cast<uint8_t>(rgba.r / strength));
    p[1] = std::min(255, static_cast<uint8_t>(rgba.g / strength));
    p[2] = std::min(255, static_cast<uint8_t>(rgba.b / strength));
    p[3] = std::max(1, static_cast<uint8_t>(255.0f / strength));
  }
}

int main(int argc, char** argv) {
  const char* infilename = nullptr;
  std::string outfilename;
  for (int i = 1; i < argc; ++i) {
    if (!infilename) {
      infilename = argv[i];
    } else if (outfilename.empty()) {
      outfilename = argv[i];  
      break;
    }
  }
  if (!infilename) {
    PrintUsage();
    return Result::Success;
  }
  if (outfilename.empty()) {
    outfilename = infilename;
    auto dotPos = outfilename.find_last_of('.');
    if (dotPos != std::string::npos) {
      outfilename.erase(dotPos, std::string::npos);
    }
    outfilename += ".png";
  }
  Imf::Array2D<Imf::Rgba> pixels;
  int w, h;
  ReadRgbaExrFile(infilename, &pixels, &w, &h);

  auto del = [](png_structp p){ png_destroy_write_struct(&p, nullptr); }
  std::unique_ptr<png_struct, decltype(del)> pngWriter(png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr), del);
  if (!pngWriter) {
    return Result::Error;
  }
  png_infop pngInfo = png_create_info_struct(pngWriter);
  if (!pngInfo) {
    return Result::Error;
  }

  std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(outfilename.c_str(), "wb"), &fclose);
  if (!fp) {
    return Result::Error;
  }

  if (setjmp(png_impbuf(pngWriter.get()))) {
    return Result::Error;
  }
  png_init_io(pngWriter.get(), fp.get());
  png_set_IHDR(pngWriter.get(), pngInfo, w, h, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(pngWriter.get(), pngInfo);
  std::vector<png_byte> row;
  row.resize(4/*8bit RGBA*/ * w * sizeof(png_byte));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      setPixel(row[x * 4], pixels[y][x]);
    }
    png_write_row(pngWriter.get(), row.data());
  }

  return Result::Success;
}