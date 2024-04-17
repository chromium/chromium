// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/qr_code_generator/bitmap_generator.h"

#include "base/containers/span.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace qr_code_generator {

TEST(QRBitmapGeneratorTest, SquareDinoWithQuietMargins) {
  auto bitmap = GenerateBitmap(
      base::as_byte_span(std::string_view("https://example.com")),
      ModuleStyle::kSquares, LocatorStyle::kSquare, CenterImage::kDino,
      QuietZone::kIncluded);

  ASSERT_TRUE(bitmap.has_value());
  EXPECT_EQ(bitmap->width(), bitmap->height());
  EXPECT_EQ(bitmap->width(), 450);

  // More detailed test coverage of the bitmap contents is provided by the pixel
  // tests at //chrome/browser/share/qr_code_generator_pixeltest.cc
}

#if !BUILDFLAG(IS_IOS)
TEST(QRBitmapGeneratorTest, RoundPassKeyWithNoMargins) {
  auto bitmap =
      GenerateBitmap(base::as_byte_span(std::string_view("0123456789")),
                     ModuleStyle::kCircles, LocatorStyle::kRounded,
                     CenterImage::kPasskey, QuietZone::kWillBeAddedByClient);

  ASSERT_TRUE(bitmap.has_value());
  EXPECT_EQ(bitmap->width(), bitmap->height());
  EXPECT_EQ(bitmap->width(), 370);

  // More detailed test coverage of the bitmap contents is provided by the pixel
  // tests at //chrome/browser/share/qr_code_generator_pixeltest.cc
}

TEST(QRBitmapGeneratorTest, RoundProductLogoWithNoMargins) {
  auto bitmap = GenerateBitmap(
      base::as_byte_span(std::string_view("0123456789")), ModuleStyle::kCircles,
      LocatorStyle::kRounded, CenterImage::kProductLogo,
      QuietZone::kWillBeAddedByClient);

  ASSERT_TRUE(bitmap.has_value());
  EXPECT_EQ(bitmap->width(), bitmap->height());
  EXPECT_EQ(bitmap->width(), 370);

  // More detailed test coverage of the bitmap contents is provided by the pixel
  // tests at //chrome/browser/share/qr_code_generator_pixeltest.cc
}
#endif

}  // namespace qr_code_generator
