// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pwg_encoder/pwg_encoder.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "components/pwg_encoder/bitmap_image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace pwg_encoder {

namespace {

const int kRasterWidth = 612;
const int kRasterHeight = 792;
const int kRasterDPI = 72;

std::unique_ptr<BitmapImage> MakeSampleBitmap() {
  auto bitmap_image =
      std::make_unique<BitmapImage>(gfx::Size(kRasterWidth, kRasterHeight));
  base::span<uint32_t> bitmap_data = bitmap_image->pixels();
  for (int i = 0; i < kRasterWidth * kRasterHeight; i++) {
    bitmap_data[i] = 0xFFFFFF;
  }

  for (int i = 0; i < kRasterWidth; i++) {
    for (int j = 200; j < 300; j++) {
      int row_start = j * kRasterWidth;
      uint32_t red = (i * 255) / kRasterWidth;
      bitmap_data[row_start + i] = red << 16;
    }
  }

  // To test run length encoding
  for (int i = 0; i < kRasterWidth; i++) {
    for (int j = 400; j < 500; j++) {
      int row_start = j * kRasterWidth;
      if ((i / 40) % 2 == 0) {
        bitmap_data[row_start + i] = 255 << 8;
      } else {
        bitmap_data[row_start + i] = 255;
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
  output += PwgEncoder::EncodePageFromBGRAColorspace(*image, header_info);
  EXPECT_EQ(2970U, output.size());

  std::string sha1 = base::SHA1HashString(output);
  EXPECT_EQ("4AD7442998C8FEAE94BC9C8B177A7C94766CC9FB", base::HexEncode(sha1));

  // Encode again in monochrome.
  header_info.color_space = PwgHeaderInfo::SGRAY;

  output = PwgEncoder::GetDocumentHeader();
  output += PwgEncoder::EncodePageFromBGRAColorspace(*image, header_info);
  EXPECT_EQ(2388U, output.size());

  sha1 = base::SHA1HashString(output);
  EXPECT_EQ("4E718B0A69AC26A366A2E23AE1ECA6055079A1FF", base::HexEncode(sha1));
}

auto ImageWithPixels(int width, int height) {
  return fuzztest::Map(
      [width, height](const std::vector<uint32_t>& pixels) {
        auto image = std::make_unique<BitmapImage>(gfx::Size(width, height));
        image->pixels().copy_from_nonoverlapping(pixels);
        return image;
      },
      fuzztest::VectorOf(fuzztest::Arbitrary<uint32_t>())
          .WithSize(width * height));
}

auto AnyBitmapImage() {
  return fuzztest::FlatMap(ImageWithPixels, fuzztest::InRange(0, 500),
                           fuzztest::InRange(0, 500));
}

void EncodeDoesNotCrash(std::unique_ptr<BitmapImage> image,
                        int dpi_width,
                        int dpi_height,
                        uint32_t total_pages,
                        bool flipx,
                        bool flipy,
                        PwgHeaderInfo::ColorSpace color_space,
                        bool duplex,
                        bool tumble) {
  PwgHeaderInfo header_info;
  header_info.dpi = gfx::Size(dpi_width, dpi_height);
  header_info.total_pages = total_pages;
  header_info.flipx = flipx;
  header_info.flipy = flipy;
  header_info.color_space = color_space;
  header_info.duplex = duplex;
  header_info.tumble = tumble;

  std::string output =
      PwgEncoder::EncodePageFromBGRAColorspace(*image, header_info);
}

FUZZ_TEST(PwgEncoderFuzzTest, EncodeDoesNotCrash)
    .WithDomains(
        /*image=*/AnyBitmapImage(),
        /*dpi_width=*/fuzztest::InRange(0, 2400),
        /*dpi_height=*/fuzztest::InRange(0, 2400),
        /*total_pages=*/fuzztest::Arbitrary<uint32_t>(),
        /*flipx=*/fuzztest::Arbitrary<bool>(),
        /*flipy=*/fuzztest::Arbitrary<bool>(),
        /*color_space=*/
        fuzztest::ElementOf({PwgHeaderInfo::SGRAY, PwgHeaderInfo::SRGB}),
        /*duplex=*/fuzztest::Arbitrary<bool>(),
        /*tumble=*/fuzztest::Arbitrary<bool>());

}  // namespace pwg_encoder
