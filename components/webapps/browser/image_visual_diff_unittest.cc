// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/image_visual_diff.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

class ImageDiffValueTest : public testing::Test {
 public:
  ImageDiffValueTest() = default;

  const SkBitmap CreateBitmap(int width, int height, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(color);
    return bitmap;
  }
};

const int kWidth = 100;
const int kHeight = 100;

TEST_F(ImageDiffValueTest, NullImageDiff) {
  EXPECT_FALSE(HasMoreThanTenPercentImageDiff(nullptr, nullptr));

  const SkBitmap bitmap1(CreateBitmap(kWidth, kHeight, SK_ColorBLACK));

  EXPECT_TRUE(HasMoreThanTenPercentImageDiff(&bitmap1, nullptr));
}

TEST_F(ImageDiffValueTest, TwoIdenticalImageDiff) {
  const SkBitmap bitmap1(CreateBitmap(kWidth, kHeight, SK_ColorBLACK));
  const SkBitmap bitmap2(CreateBitmap(kWidth, kHeight, SK_ColorBLACK));

  EXPECT_FALSE(HasMoreThanTenPercentImageDiff(&bitmap1, &bitmap2));
}

TEST_F(ImageDiffValueTest, ImageWithMoreThanTenPercentImageDiff) {
  const SkBitmap bitmap1(CreateBitmap(kWidth, kHeight, SK_ColorBLACK));
  const SkBitmap bitmap2(CreateBitmap(kWidth, kHeight, SK_ColorWHITE));

  EXPECT_TRUE(HasMoreThanTenPercentImageDiff(&bitmap1, &bitmap2));
}

TEST_F(ImageDiffValueTest, ImageWithLessThanTenPercentImageDiff) {
  const SkBitmap bitmap1(CreateBitmap(kWidth, kHeight, SK_ColorBLACK));
  const SkBitmap bitmap2(CreateBitmap(kWidth, kHeight, SK_ColorBLACK));

  // To achieve less than 10% difference, change fewer than 1334 pixels. Change
  // the first 13 rows. 13 rows * 100 columns = 1300 pixels. 1300 pixels * 0.75
  // diff/pixel = 975 total difference sum. Percentage = 100 * 975 / 10000
  // = 9.75, which is < 10%.
  bitmap2.eraseArea(SkIRect::MakeXYWH(0, 0, kWidth, 13), SK_ColorWHITE);

  EXPECT_FALSE(HasMoreThanTenPercentImageDiff(&bitmap1, &bitmap2));
}

TEST_F(ImageDiffValueTest, BlackImageComparedWithColoredImageDiff) {
  const SkBitmap bitmap1(CreateBitmap(kWidth, kHeight, SK_ColorBLACK));
  const SkBitmap bitmap2(CreateBitmap(kWidth, kHeight, SK_ColorRED));

  EXPECT_TRUE(HasMoreThanTenPercentImageDiff(&bitmap1, &bitmap2));
}
}  // namespace web_app
