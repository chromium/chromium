// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/pwg_encoder/pwg_encoder.h"

#include <stdint.h>

#include <memory>

#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "components/pwg_encoder/bitmap_image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pwg_encoder {

namespace {

const int kRasterWidth = 612;
const int kRasterHeight = 792;
const int kRasterDPI = 72;

std::unique_ptr<BitmapImage> MakeSampleBitmap() {
  auto bitmap_image = std::make_unique<BitmapImage>(
      gfx::Size(kRasterWidth, kRasterHeight), BitmapImage::RGBA);
  uint32_t* bitmap_data =
      reinterpret_cast<uint32_t*>(bitmap_image->pixel_data());
  for (int i = 0; i < kRasterWidth * kRasterHeight; i++)
    bitmap_data[i] = 0xFFFFFF;

  for (int i = 0; i < kRasterWidth; i++) {
    for (int j = 200; j < 300; j++) {
      int row_start = j * kRasterWidth;
      uint32_t red = (i * 255) / kRasterWidth;
      bitmap_data[row_start + i] = red;
    }
  }

  // To test run length encoding
  for (int i = 0; i < kRasterWidth; i++) {
    for (int j = 400; j < 500; j++) {
      int row_start = j * kRasterWidth;
      if ((i / 40) % 2 == 0) {
        bitmap_data[row_start + i] = 255 << 8;
      } else {
        bitmap_data[row_start + i] = 255 << 16;
      }
    }
  }

  return bitmap_image;
}

}  // namespace

TEST(PwgRasterTest, Encode) {
  // Encode in color by default.
  std::unique_ptr<BitmapImage> image = MakeSampleBitmap();
  PwgHeaderInfo header_info;
  header_info.dpi = gfx::Size(kRasterDPI, kRasterDPI);

  std::string output = PwgEncoder::GetDocumentHeader();
  output += PwgEncoder::EncodePage(*image, header_info);
  EXPECT_EQ(2970U, output.size());

  std::string sha1 = base::SHA1HashString(output);
  EXPECT_EQ("4AD7442998C8FEAE94BC9C8B177A7C94766CC9FB", base::HexEncode(sha1));

  // Encode again in monochrome.
  header_info.color_space = PwgHeaderInfo::SGRAY;

  output = PwgEncoder::GetDocumentHeader();
  output += PwgEncoder::EncodePage(*image, header_info);
  EXPECT_EQ(2388U, output.size());

  sha1 = base::SHA1HashString(output);
  EXPECT_EQ("4E718B0A69AC26A366A2E23AE1ECA6055079A1FF", base::HexEncode(sha1));
}

}  // namespace pwg_encoder
