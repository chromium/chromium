// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/visual_utils.h"

#include <array>

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "components/safe_browsing/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace safe_browsing::visual_utils {

namespace {

const SkPMColor kSkPMRed = SkPackARGB32(255, 255, 0, 0);
const SkPMColor kSkPMGreen = SkPackARGB32(255, 0, 255, 0);
const SkPMColor kSkPMBlue = SkPackARGB32(255, 0, 0, 255);

}  // namespace

using ::testing::FloatEq;

class VisualUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    base::DiscardableMemoryAllocator::SetInstance(&test_allocator_);

    sk_sp<SkColorSpace> rec2020 = SkColorSpace::MakeRGB(
        {2.22222f, 0.909672f, 0.0903276f, 0.222222f, 0.0812429f, 0, 0},
        SkNamedGamut::kRec2020);
    SkImageInfo bitmap_info = SkImageInfo::MakeN32(
        1000, 1000, SkAlphaType::kUnpremul_SkAlphaType, rec2020);

    ASSERT_TRUE(bitmap_.tryAllocPixels(bitmap_info));
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

  void ExpectPixel(base::span<const unsigned char> pixel,
                   const VisualFeatures::BlurredImage& image,
                   size_t row,
                   size_t col) {
    ASSERT_LT(static_cast<int>(row), image.height());
    ASSERT_LT(static_cast<int>(col), image.width());
    EXPECT_EQ(pixel[0], static_cast<unsigned char>(
                            image.data()[3 * row * image.width() + 3 * col]))
        << "R component of pixel at row " << row << " and column " << col
        << " is incorrect.";
    EXPECT_EQ(pixel[1],
              static_cast<unsigned char>(
                  image.data()[3 * row * image.width() + 3 * col + 1]))
        << "G component of pixel at row " << row << " and column " << col
        << " is incorrect.";
    EXPECT_EQ(pixel[2],
              static_cast<unsigned char>(
                  image.data()[3 * row * image.width() + 3 * col + 2]))
        << "B component of pixel at row " << row << " and column " << col
        << " is incorrect.";
  }

  // A test bitmap to work with. Initialized to be 1000x1000 in the Rec 2020
  // color space.
  SkBitmap bitmap_;

 private:
  // A DiscardableMemoryAllocator is needed for certain Skia operations.
  base::TestDiscardableMemoryAllocator test_allocator_;
};

TEST_F(VisualUtilsTest, BlurImageWhite) {
  VisualFeatures::BlurredImage blurred;

  // Draw white over the image
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1000, 1000));

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));

  constexpr std::array<const unsigned char, 3> kWhite = {0xff, 0xff, 0xff};
#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(18, blurred.width());
  ASSERT_EQ(32, blurred.height());
  ASSERT_EQ(3u * 18u * 32u, blurred.data().size());
  for (size_t i = 0; i < 32u; i++) {
    for (size_t j = 0; i < 18u; i++) {
      ExpectPixel(kWhite, blurred, i, j);
    }
  }
#else
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  for (size_t i = 0; i < 48u; i++) {
    for (size_t j = 0; i < 48u; i++) {
      ExpectPixel(kWhite, blurred, i, j);
    }
  }
#endif
}

TEST_F(VisualUtilsTest, BlurImageRed) {
  VisualFeatures::BlurredImage blurred;

  // Draw red over the image.
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kSkPMRed;

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  constexpr std::array<const unsigned char, 3> kRed = {0xff, 0x00, 0x00};
#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(18, blurred.width());
  ASSERT_EQ(32, blurred.height());
  ASSERT_EQ(3u * 18u * 32u, blurred.data().size());
  for (size_t i = 0; i < 32u; i++) {
    for (size_t j = 0; i < 18u; i++) {
      ExpectPixel(kRed, blurred, i, j);
    }
  }
#else
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  for (size_t i = 0; i < 48u; i++) {
    for (size_t j = 0; i < 48u; i++) {
      ExpectPixel(kRed, blurred, i, j);
    }
  }
#endif
}

TEST_F(VisualUtilsTest, BlurImageHalfWhiteHalfBlack) {
  VisualFeatures::BlurredImage blurred;

  // Draw black over half the image.
  bitmap_.erase(SK_ColorBLACK, SkIRect::MakeXYWH(0, 0, 1000, 500));

  // Draw white over half the image
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 500, 1000, 1000));

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  constexpr std::array<const unsigned char, 3> kBlack = {0x00, 0x00, 0x00};
  constexpr std::array<const unsigned char, 3> kWhite = {0xff, 0xff, 0xff};
#if BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(18, blurred.width());
  ASSERT_EQ(32, blurred.height());
  ASSERT_EQ(3u * 18u * 32u, blurred.data().size());
  // The middle blocks may have been blurred to something between white and
  // black, so only verify the first 14 and last 14 rows.
  for (size_t i = 0; i < 14u; i++) {
    for (size_t j = 0; j < 18u; j++) {
      ExpectPixel(kBlack, blurred, i, j);
    }
  }

  for (size_t i = 18u; i < 32u; i++) {
    for (size_t j = 0; j < 18u; j++) {
      ExpectPixel(kWhite, blurred, i, j);
    }
  }
#else
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  // The middle blocks may have been blurred to something between white and
  // black, so only verify the first 22 and last 22 rows.
  for (size_t i = 0; i < 22u; i++) {
    for (size_t j = 0; j < 48u; j++) {
      ExpectPixel(kBlack, blurred, i, j);
    }
  }

  for (size_t i = 26u; i < 48u; i++) {
    for (size_t j = 0; j < 48u; j++) {
      ExpectPixel(kWhite, blurred, i, j);
    }
  }
#endif
}

TEST_F(VisualUtilsTest, BlockMeanAverageOneBlock) {
  // Draw black over half the image.
  bitmap_.erase(SK_ColorBLACK, SkIRect::MakeXYWH(0, 0, 1000, 500));

  // Draw white over half the image
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 500, 1000, 1000));

  std::unique_ptr<SkBitmap> blocks = BlockMeanAverage(bitmap_, 1000);
  ASSERT_EQ(1, blocks->width());
  ASSERT_EQ(1, blocks->height());
  EXPECT_EQ(blocks->getColor(0, 0), SkColorSetRGB(127, 127, 127));
}

TEST_F(VisualUtilsTest, BlockMeanAveragePartialBlocks) {
  // Draw a white, red, green, and blue box with the expected block sizes.
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 600, 600));

  for (int x = 600; x < 1000; x++)
    for (int y = 0; y < 600; y++)
      *bitmap_.getAddr32(x, y) = kSkPMRed;

  for (int x = 0; x < 600; x++)
    for (int y = 600; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kSkPMGreen;

  for (int x = 600; x < 1000; x++)
    for (int y = 600; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kSkPMBlue;

  std::unique_ptr<SkBitmap> blocks = BlockMeanAverage(bitmap_, 600);
  ASSERT_EQ(2, blocks->width());
  ASSERT_EQ(2, blocks->height());
  EXPECT_EQ(blocks->getColor(0, 0), SK_ColorWHITE);

  EXPECT_EQ(*blocks->getAddr32(1, 0), kSkPMRed);
  EXPECT_EQ(*blocks->getAddr32(0, 1), kSkPMGreen);
  EXPECT_EQ(*blocks->getAddr32(1, 1), kSkPMBlue);
}

TEST_F(VisualUtilsTest, NonSquareBlurredImage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kVisualFeaturesSizes, {{"phash_width", "108"}, {"phash_height", "192"}});

  VisualFeatures::BlurredImage blurred;

  // Draw white over the image
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1000, 1000));

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  ASSERT_EQ(18, blurred.width());
  ASSERT_EQ(32, blurred.height());
  ASSERT_EQ(3u * 18u * 32u, blurred.data().size());
  for (size_t i = 0; i < 18u * 32u; i++) {
    EXPECT_EQ('\xff', blurred.data()[3 * i]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 2]);
  }
}

}  // namespace safe_browsing::visual_utils
