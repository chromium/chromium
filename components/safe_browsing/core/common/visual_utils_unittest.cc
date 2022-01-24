// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/visual_utils.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "components/safe_browsing/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorPriv.h"

namespace safe_browsing {
namespace visual_utils {

namespace {

const SkPMColor kSkPMRed = SkPackARGB32(255, 255, 0, 0);
const SkPMColor kSkPMGreen = SkPackARGB32(255, 0, 255, 0);
const SkPMColor kSkPMBlue = SkPackARGB32(255, 0, 0, 255);

// Use to add noise to the 5 lower bits of each color component. Use to
// introduce values directly into an N32 bitmap's memory. Use to diversify input
// in tests that cover function that will only look at the most significant
// three bits of each component. |i| is used to get different noise that changes
// in sequence. Any number can be provided. Noise applied can be identical for
// two different values of |i|.
SkPMColor AddNoiseToLowerBits(SkColor color, unsigned int i) {
  // Get a mask between 00000 and 11111 from index.
  unsigned int mask = i % 0x1f;

  // Apply noise to each color component separately.
  color = SkColorSetARGB(SkColorGetA(color) | mask, SkColorGetR(color) | mask,
                         SkColorGetG(color) | mask, SkColorGetB(color) | mask);

  return color;
}

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

  std::string GetBlurredBitmapHash() {
    VisualFeatures::BlurredImage blurred_image;
    EXPECT_TRUE(visual_utils::GetBlurredImage(bitmap_, &blurred_image));
    std::string blurred_image_hash =
        visual_utils::GetHashFromBlurredImage(blurred_image);
    return blurred_image_hash;
  }

  VisualFeatures::ColorHistogram GetBitmapHistogram() {
    VisualFeatures::ColorHistogram histogram;
    EXPECT_TRUE(GetHistogramForImage(bitmap_, &histogram));
    return histogram;
  }

  // A test bitmap to work with. Initialized to be 1000x1000 in the Rec 2020
  // color space.
  SkBitmap bitmap_;

 private:
  // A DiscardableMemoryAllocator is needed for certain Skia operations.
  base::TestDiscardableMemoryAllocator test_allocator_;
};

TEST_F(VisualUtilsTest, TestkColorToQuantizedColor) {
  // Test quantization
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 31)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 32)), 1u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 31, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 32, 0)), 8u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(31, 0, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(32, 0, 0)), 64u);

  // Test composition of RGB quantized values
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 255)), 7u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 255, 255)), 63u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(255, 255, 255)), 511u);
}

TEST_F(VisualUtilsTest, GetQuantizedR) {
  EXPECT_EQ(GetQuantizedR(0), 0);
  EXPECT_EQ(GetQuantizedR(64), 1);
  EXPECT_EQ(GetQuantizedR(448), 7);
}

TEST_F(VisualUtilsTest, GetQuantizedG) {
  EXPECT_EQ(GetQuantizedG(0), 0);
  EXPECT_EQ(GetQuantizedG(8), 1);
  EXPECT_EQ(GetQuantizedG(56), 7);
}

TEST_F(VisualUtilsTest, GetQuantizedB) {
  EXPECT_EQ(GetQuantizedB(0), 0);
  EXPECT_EQ(GetQuantizedB(1), 1);
  EXPECT_EQ(GetQuantizedB(7), 7);
}

TEST_F(VisualUtilsTest, GetHistogramForImageWhite) {
  VisualFeatures::ColorHistogram histogram;

  // Draw white over half the image
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 1000; y++)
      // NOTE: getAddr32 used since byte ordering does not matter for white and
      // repeated erase() calls might make tests time out.
      *bitmap_.getAddr32(x, y) = AddNoiseToLowerBits(SK_ColorWHITE, x + y);

  ASSERT_TRUE(GetHistogramForImage(bitmap_, &histogram));
  ASSERT_EQ(histogram.bins_size(), 1);
  EXPECT_THAT(histogram.bins(0).centroid_x(),
              FloatEq(0.4995));  // All pixels are the same color, so centroid_x
                                 // is (0+1+...+999)/1000/1000 = 0.4995
  EXPECT_THAT(histogram.bins(0).centroid_y(), FloatEq(0.4995));
  EXPECT_EQ(histogram.bins(0).quantized_r(), 7);
  EXPECT_EQ(histogram.bins(0).quantized_g(), 7);
  EXPECT_EQ(histogram.bins(0).quantized_b(), 7);
  EXPECT_THAT(histogram.bins(0).weight(), FloatEq(1.0));
}

TEST_F(VisualUtilsTest, GetHistogramForImageHalfWhiteHalfBlack) {
  VisualFeatures::ColorHistogram histogram;

  // Draw white over half the image
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 500; y++)
      // NOTE: getAddr32 used since byte ordering does not matter for white and
      // repeated erase() calls might make tests time out.
      *bitmap_.getAddr32(x, y) = AddNoiseToLowerBits(SK_ColorWHITE, x + y);

  // Draw black over half the image.
  for (int x = 0; x < 1000; x++)
    for (int y = 500; y < 1000; y++)
      // NOTE: getAddr32 used since byte ordering does not matter for black and
      // repeated erase() calls might make tests time out.
      *bitmap_.getAddr32(x, y) = AddNoiseToLowerBits(SK_ColorBLACK, x + y);

  ASSERT_TRUE(GetHistogramForImage(bitmap_, &histogram));
  ASSERT_EQ(histogram.bins_size(), 2);

  EXPECT_THAT(histogram.bins(0).centroid_x(), FloatEq(0.4995));
  EXPECT_THAT(histogram.bins(0).centroid_y(), FloatEq(0.7495));
  EXPECT_EQ(histogram.bins(0).quantized_r(), 0);
  EXPECT_EQ(histogram.bins(0).quantized_g(), 0);
  EXPECT_EQ(histogram.bins(0).quantized_b(), 0);
  EXPECT_THAT(histogram.bins(0).weight(), FloatEq(0.5));

  EXPECT_THAT(histogram.bins(1).centroid_x(), FloatEq(0.4995));
  EXPECT_THAT(histogram.bins(1).centroid_y(), FloatEq(0.2495));
  EXPECT_EQ(histogram.bins(1).quantized_r(), 7);
  EXPECT_EQ(histogram.bins(1).quantized_g(), 7);
  EXPECT_EQ(histogram.bins(1).quantized_b(), 7);
  EXPECT_THAT(histogram.bins(1).weight(), FloatEq(0.5));
}

TEST_F(VisualUtilsTest, BlurImageWhite) {
  VisualFeatures::BlurredImage blurred;

  // Draw white over the image
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1000, 1000));

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  for (size_t i = 0; i < 48u * 48u; i++) {
    EXPECT_EQ('\xff', blurred.data()[3 * i]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 2]);
  }
}

TEST_F(VisualUtilsTest, BlurImageRed) {
  VisualFeatures::BlurredImage blurred;

  // Draw red over the image.
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kSkPMRed;

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  for (size_t i = 0; i < 48u * 48u; i++) {
    EXPECT_EQ('\xff', blurred.data()[3 * i]);
    EXPECT_EQ('\x00', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\x00', blurred.data()[3 * i + 2]);
  }
}

TEST_F(VisualUtilsTest, BlurImageHalfWhiteHalfBlack) {
  VisualFeatures::BlurredImage blurred;

  // Draw black over half the image.
  bitmap_.erase(SK_ColorBLACK, SkIRect::MakeXYWH(0, 0, 1000, 500));

  // Draw white over half the image
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 500, 1000, 1000));

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  // The middle blocks may have been blurred to something between white and
  // black, so only verify the first 22 and last 22 rows.
  for (size_t i = 0; i < 22u * 48u; i++) {
    EXPECT_EQ('\x00', blurred.data()[3 * i]);
    EXPECT_EQ('\x00', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\x00', blurred.data()[3 * i + 2]);
  }

  for (size_t i = 26u * 48u; i < 48u * 48u; i++) {
    EXPECT_EQ('\xff', blurred.data()[3 * i]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 2]);
  }
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

TEST_F(VisualUtilsTest, IsVisualMatchHash) {
  {
    // An all-white image should hash to all 1-bits.
    bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1000, 1000));

    std::vector<unsigned char> target_hash;
    target_hash.push_back('\x30');
    for (int i = 0; i < 288; i++)
      target_hash.push_back('\xff');

    VisualTarget target;
    target.set_hash(target_hash.data(), target_hash.size());
    target.mutable_match_config()->add_match_rule()->set_hash_distance(0.0);
    EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                              GetBitmapHistogram(), target)
                    .has_value());
  }

  {
    // Make the top quarter black, and the corresponding bits of the hash should
    // be 0.
    bitmap_.erase(SK_ColorBLACK, SkIRect::MakeXYWH(0, 0, 1000, 250));

    std::vector<unsigned char> target_hash;
    target_hash.push_back('\x30');
    for (int i = 0; i < 72; i++)
      target_hash.push_back('\x00');
    for (int i = 0; i < 216; i++)
      target_hash.push_back('\xff');
    VisualTarget target;
    target.set_hash(target_hash.data(), target_hash.size());

    target.mutable_match_config()->add_match_rule()->set_hash_distance(0.0);
    EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                              GetBitmapHistogram(), target)
                    .has_value());
  }
}

TEST_F(VisualUtilsTest, IsVisualMatchHashPartialMatch) {
  // Make the top quarter black, and the corresponding bits of the hash should
  // be 0.
  bitmap_.erase(SK_ColorBLACK, SkIRect::MakeXYWH(0, 0, 1000, 250));
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 250, 1000, 1000));

  std::vector<unsigned char> target_hash;
  target_hash.push_back('\x30');
  for (int i = 0; i < 75; i++)
    target_hash.push_back('\x00');
  for (int i = 0; i < 213; i++)
    target_hash.push_back('\xff');

  VisualTarget target;
  target.set_hash(target_hash.data(), target_hash.size());
  target.mutable_match_config()->add_match_rule()->set_hash_distance(23.0);
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());
  target.mutable_match_config()->add_match_rule()->set_hash_distance(24.0);
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());
}

TEST_F(VisualUtilsTest, IsVisualMatchHashStrideComparison) {
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1000, 1000));

  std::vector<unsigned char> target_hash;
  target_hash.push_back('\x30');
  for (int i = 0; i < 288; i++)
    target_hash.push_back('\xff');

  VisualTarget target;
  target.set_hash(target_hash.data(), target_hash.size());
  target.mutable_match_config()->add_match_rule()->set_hash_distance(0.0);
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());

  target_hash[0] = '\x00';
  target.set_hash(target_hash.data(), target_hash.size());
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());
}

TEST_F(VisualUtilsTest, IsVisualMatchHistogramOnly) {
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 1000; y++)
      // NOTE: getAddr32 used since byte ordering does not matter for white and
      // repeated erase calls might make tests time out.
      *bitmap_.getAddr32(x, y) = AddNoiseToLowerBits(SK_ColorWHITE, x + y);

  {
    VisualTarget target;
    VisualFeatures::ColorHistogramBin* bin = target.add_bins();
    // Our coordinates range from 0 to 999, giving an average of 0.4995 instead
    // of 0.5.
    bin->set_centroid_x(0.4995);
    bin->set_centroid_y(0.4995);
    bin->set_quantized_r(7);
    bin->set_quantized_g(7);
    bin->set_quantized_b(7);
    bin->set_weight(1.0);
    target.mutable_match_config()->add_match_rule()->set_color_distance(0.0);
    EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                              GetBitmapHistogram(), target)
                    .has_value());
  }

  {
    // Move the target histogram to have centroid 0,0. This leads to a distance
    // just under 0.5 between the actual and target histograms.
    VisualTarget target;
    VisualFeatures::ColorHistogramBin* bin = target.add_bins();
    bin->set_centroid_x(0);
    bin->set_centroid_y(0);
    bin->set_quantized_r(7);
    bin->set_quantized_g(7);
    bin->set_quantized_b(7);
    bin->set_weight(1.0);

    MatchRule* match_rule = target.mutable_match_config()->add_match_rule();
    match_rule->set_color_distance(0.5);
    EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                              GetBitmapHistogram(), target)
                    .has_value());

    match_rule->set_color_distance(0.4);
    EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                               GetBitmapHistogram(), target)
                     .has_value());
  }

  {
    // Change the target histogram color slightly. This leads to a distance
    // of roughly 0.12 between the actual and target histogram.
    VisualTarget target;
    VisualFeatures::ColorHistogramBin* bin = target.add_bins();
    bin->set_centroid_x(0.4995);
    bin->set_centroid_y(0.4995);
    bin->set_quantized_r(6);
    bin->set_quantized_g(6);
    bin->set_quantized_b(6);
    bin->set_weight(1.0);

    MatchRule* match_rule = target.mutable_match_config()->add_match_rule();
    match_rule->set_color_distance(0.2);
    EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                              GetBitmapHistogram(), target)
                    .has_value());

    match_rule->set_color_distance(0.1);
    EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                               GetBitmapHistogram(), target)
                     .has_value());
  }
}

TEST_F(VisualUtilsTest, IsVisualMatchColorRange) {
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1000, 1000));

  *bitmap_.getAddr32(0, 0) = kSkPMBlue;

  SkScalar hsv[3];
  SkColorToHSV(bitmap_.getColor(0, 0), hsv);
  SkScalar target_hue = hsv[0];
  VisualTarget target;
  MatchRule::ColorRange* color_range =
      target.mutable_match_config()->add_match_rule()->add_color_range();
  color_range->set_low(target_hue);
  color_range->set_high(target_hue);

  // Blue hue present
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());

  // Color range too high
  color_range->set_low(target_hue + 1);
  color_range->set_high(target_hue + 1);
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());

  // Color range too low
  color_range->set_low(target_hue - 1);
  color_range->set_high(target_hue - 1);
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());

  // No blue hue present
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());
}

TEST_F(VisualUtilsTest, IsVisualMatchMultipleColorRanges) {
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1000, 1000));

  *bitmap_.getAddr32(0, 0) = kSkPMBlue;
  *bitmap_.getAddr32(1, 0) = kSkPMGreen;

  SkScalar hsv[3];
  SkColorToHSV(bitmap_.getColor(0, 0), hsv);
  SkScalar blue_hue = hsv[0];
  VisualTarget target;
  MatchRule* match_rule = target.mutable_match_config()->add_match_rule();
  MatchRule::ColorRange* color_range = match_rule->add_color_range();
  color_range->set_low(blue_hue);
  color_range->set_high(blue_hue);

  SkColorToHSV(bitmap_.getColor(1, 0), hsv);
  SkScalar green_hue = hsv[0];
  color_range = match_rule->add_color_range();
  color_range->set_low(green_hue);
  color_range->set_high(green_hue);

  // Both hues present
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());

  // No blue hue present
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());

  // No green hue present
  *bitmap_.getAddr32(0, 0) = kSkPMBlue;
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(1, 0, 1, 1));

  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());

  // Neither hue present
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());
}

TEST_F(VisualUtilsTest, IsVisualMatchMultipleMatchRules) {
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));

  *bitmap_.getAddr32(0, 0) = kSkPMBlue;
  *bitmap_.getAddr32(1, 0) = kSkPMGreen;

  // Create a target with two match rules, one matching blue pixels and one
  // matching green.
  VisualTarget target;
  MatchRule* match_rule_blue = target.mutable_match_config()->add_match_rule();
  MatchRule::ColorRange* color_range = match_rule_blue->add_color_range();
  SkScalar hsv[3];
  SkColorToHSV(bitmap_.getColor(0, 0), hsv);
  SkScalar blue_hue = hsv[0];
  color_range->set_low(blue_hue);
  color_range->set_high(blue_hue);

  MatchRule* match_rule_green = target.mutable_match_config()->add_match_rule();
  color_range = match_rule_green->add_color_range();
  SkColorToHSV(bitmap_.getColor(1, 0), hsv);
  SkScalar green_hue = hsv[0];
  color_range->set_low(green_hue);
  color_range->set_high(green_hue);

  // Both hues present
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());

  // No blue hue present
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());

  // No green hue present
  *bitmap_.getAddr32(0, 0) = kSkPMBlue;
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(1, 0, 1, 1));
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());

  // Neither hue present
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());
}

TEST_F(VisualUtilsTest, IsVisualMatchFloatColorRange) {
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));

  // Adding a little noise in the red component makes the target_hue not an
  // integer. Testing for this color requires float ranges.
  *bitmap_.getAddr32(0, 0) = kSkPMBlue | 0x0f;

  SkScalar hsv[3];
  SkColorToHSV(bitmap_.getColor(0, 0), hsv);
  SkScalar target_hue = hsv[0];
  VisualTarget target;
  MatchRule::FloatColorRange* color_range =
      target.mutable_match_config()->add_match_rule()->add_float_color_range();
  color_range->set_low(target_hue);
  color_range->set_high(target_hue);

  // Blue hue present
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());

  // Color range too high
  color_range->set_low(target_hue + 0.1);
  color_range->set_high(target_hue + 0.1);
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());

  // Color range too low
  color_range->set_low(target_hue - 0.1);
  color_range->set_high(target_hue - 0.1);
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());

  // No blue hue present
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());
}

TEST_F(VisualUtilsTest, IsVisualMatchMultipleFloatColorRanges) {
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));

  *bitmap_.getAddr32(0, 0) = kSkPMBlue;
  *bitmap_.getAddr32(1, 0) = kSkPMGreen;

  SkScalar hsv[3];
  SkColorToHSV(bitmap_.getColor(0, 0), hsv);
  SkScalar blue_hue = hsv[0];
  VisualTarget target;
  MatchRule* match_rule = target.mutable_match_config()->add_match_rule();
  MatchRule::FloatColorRange* color_range = match_rule->add_float_color_range();
  color_range->set_low(blue_hue);
  color_range->set_high(blue_hue);

  SkColorToHSV(bitmap_.getColor(1, 0), hsv);
  SkScalar green_hue = hsv[0];
  color_range = match_rule->add_float_color_range();
  color_range->set_low(green_hue);
  color_range->set_high(green_hue);

  // Both hues present
  EXPECT_TRUE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                            GetBitmapHistogram(), target)
                  .has_value());

  // No blue hue present
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());

  // No green hue present
  *bitmap_.getAddr32(0, 0) = kSkPMBlue;
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(1, 0, 1, 1));
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());

  // Neither hue present
  bitmap_.erase(SK_ColorWHITE, SkIRect::MakeXYWH(0, 0, 1, 1));
  EXPECT_FALSE(IsVisualMatch(bitmap_, GetBlurredBitmapHash(),
                             GetBitmapHistogram(), target)
                   .has_value());
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

}  // namespace visual_utils
}  // namespace safe_browsing
