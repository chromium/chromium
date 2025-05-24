// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/language_detection/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeRenderFrameObserverTest : public testing::Test,
                                      public testing::WithParamInterface<bool> {
 public:
  ChromeRenderFrameObserverTest() {
    scoped_feature_list_.InitWithFeatureState(
        language_detection::features::kLazyUpdateTranslateModel, GetParam());
  }

  bool NeedsDownscale(const gfx::Size& original_image_size,
                      int32_t requested_image_min_area_pixels,
                      const gfx::Size& requested_image_max_size) {
    return ChromeRenderFrameObserver::NeedsDownscale(
        original_image_size, requested_image_min_area_pixels,
        requested_image_max_size);
  }

  bool NeedsEncodeImage(const std::string& image_extension,
                        chrome::mojom::ImageFormat image_format) {
    return ChromeRenderFrameObserver::NeedsEncodeImage(image_extension,
                                                       image_format);
  }

  bool IsAnimatedWebp(const std::vector<uint8_t>& image_data) {
    return ChromeRenderFrameObserver::IsAnimatedWebp(image_data);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, ChromeRenderFrameObserverTest, testing::Bool());

TEST_P(ChromeRenderFrameObserverTest,
       NeedsDownscale_RequestLargeThanOriginalReturnFalse) {
  EXPECT_FALSE(
      NeedsDownscale(/* original_image_size */ gfx::Size(10, 10),
                     /* requested_image_min_area_pixels */ 200,
                     /* requested_image_max_size */ gfx::Size(20, 20)));
}

TEST_P(ChromeRenderFrameObserverTest, NeedsDownscale_SameSizeReturnFalse) {
  EXPECT_FALSE(
      NeedsDownscale(/* original_image_size */ gfx::Size(10, 10),
                     /* requested_image_min_area_pixels */ 100,
                     /* requested_image_max_size */ gfx::Size(10, 10)));
}

TEST_P(ChromeRenderFrameObserverTest, NeedsDownscale_OnlyWidthShortReturnTrue) {
  EXPECT_TRUE(NeedsDownscale(/* original_image_size */ gfx::Size(10, 10),
                             /* requested_image_min_area_pixels */ 100,
                             /* requested_image_max_size */ gfx::Size(9, 10)));
}

TEST_P(ChromeRenderFrameObserverTest, NeedsDownscale_OnlyAreaSmallReturnFalse) {
  EXPECT_FALSE(
      NeedsDownscale(/* original_image_size */ gfx::Size(10, 10),
                     /* requested_image_min_area_pixels */ 20,
                     /* requested_image_max_size */ gfx::Size(20, 20)));
}

TEST_P(ChromeRenderFrameObserverTest, NeedsEncodeImage_JpegFormat) {
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".png",
                       /* image_format */ chrome::mojom::ImageFormat::JPEG));
  EXPECT_FALSE(
      NeedsEncodeImage(/* image_extension */ ".jpg",
                       /* image_format */ chrome::mojom::ImageFormat::JPEG));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".gif",
                       /* image_format */ chrome::mojom::ImageFormat::JPEG));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".bmp",
                       /* image_format */ chrome::mojom::ImageFormat::JPEG));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".webp",
                       /* image_format */ chrome::mojom::ImageFormat::JPEG));
}

TEST_P(ChromeRenderFrameObserverTest, NeedsEncodeImage_PngFormat) {
  EXPECT_FALSE(
      NeedsEncodeImage(/* image_extension */ ".png",
                       /* image_format */ chrome::mojom::ImageFormat::PNG));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".jpg",
                       /* image_format */ chrome::mojom::ImageFormat::PNG));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".gif",
                       /* image_format */ chrome::mojom::ImageFormat::PNG));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".bmp",
                       /* image_format */ chrome::mojom::ImageFormat::PNG));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".webp",
                       /* image_format */ chrome::mojom::ImageFormat::PNG));
}

TEST_P(ChromeRenderFrameObserverTest, NeedsEncodeImage_WebpFormat) {
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".png",
                       /* image_format */ chrome::mojom::ImageFormat::WEBP));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".jpg",
                       /* image_format */ chrome::mojom::ImageFormat::WEBP));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".gif",
                       /* image_format */ chrome::mojom::ImageFormat::WEBP));
  EXPECT_TRUE(
      NeedsEncodeImage(/* image_extension */ ".bmp",
                       /* image_format */ chrome::mojom::ImageFormat::WEBP));
  EXPECT_FALSE(
      NeedsEncodeImage(/* image_extension */ ".webp",
                       /* image_format */ chrome::mojom::ImageFormat::WEBP));
}

TEST_P(ChromeRenderFrameObserverTest, NeedsEncodeImage_OriginalFormat) {
  EXPECT_FALSE(NeedsEncodeImage(
      /* image_extension */ ".png",
      /* image_format */ chrome::mojom::ImageFormat::ORIGINAL));
  EXPECT_FALSE(NeedsEncodeImage(
      /* image_extension */ ".jpg",
      /* image_format */ chrome::mojom::ImageFormat::ORIGINAL));
  EXPECT_FALSE(NeedsEncodeImage(
      /* image_extension */ ".gif",
      /* image_format */ chrome::mojom::ImageFormat::ORIGINAL));
  EXPECT_TRUE(NeedsEncodeImage(
      /* image_extension */ ".bmp",
      /* image_format */ chrome::mojom::ImageFormat::ORIGINAL));
  EXPECT_TRUE(NeedsEncodeImage(
      /* image_extension */ ".webp",
      /* image_format */ chrome::mojom::ImageFormat::ORIGINAL));
}

TEST_P(ChromeRenderFrameObserverTest,
       IsAnimatedWebp_HeaderTooSmall_ReturnsFalse) {
  // First 10 bytes taken from a real animated webp image.
  const std::vector<uint8_t> broken_image_data{82, 73, 70, 70, 228,
                                               26, 0,  0,  87};
  EXPECT_FALSE(IsAnimatedWebp(broken_image_data));
}

TEST_P(ChromeRenderFrameObserverTest, IsAnimatedWebp_StaticWebp_ReturnsFalse) {
  // First 75 bytes taken from a real animated webp image.
  const std::vector<uint8_t> static_webp_image_data{
      82,  73,  70,  70,  88,  59,  1,   0,   87,  69,  66,  80,  86,
      80,  56,  32,  76,  59,  1,   0,   208, 98,  5,   157, 1,   42,
      0,   4,   64,  2,   62,  109, 50,  148, 71,  36,  35,  34,  38,
      166, 86,  124, 72,  208, 13,  137, 103, 106, 155, 24,  53,  233,
      164, 233, 87,  115, 20,  237, 92,  128, 131, 202, 189, 198, 245,
      91,  174, 63,  84,  114, 136, 231, 31,  212, 255};
  EXPECT_FALSE(IsAnimatedWebp(static_webp_image_data));
}

TEST_P(ChromeRenderFrameObserverTest, IsAnimatedWebp_Jpeg_ReturnsFalse) {
  // First 75 bytes taken from a real jpeg image.
  const std::vector<uint8_t> jpeg_image_data{
      255, 216, 255, 224, 0,  16,  74,  70,  73,  70, 0,  1,  1,  1,   0,
      72,  0,   72,  0,   0,  255, 226, 12,  88,  73, 67, 67, 95, 80,  82,
      79,  70,  73,  76,  69, 0,   1,   1,   0,   0,  12, 72, 76, 105, 110,
      111, 2,   16,  0,   0,  109, 110, 116, 114, 82, 71, 66, 32, 88,  89,
      90,  32,  7,   206, 0,  2,   0,   9,   0,   6,  0,  49, 0,  0,   97};
  EXPECT_FALSE(IsAnimatedWebp(jpeg_image_data));
}

TEST_P(ChromeRenderFrameObserverTest, IsAnimatedWebp_AnimatedWebp_ReturnsTrue) {
  // First 75 bytes taken from a real animated webp image.
  const std::vector<uint8_t> animated_webp_image_data{
      82, 73, 70, 70, 228, 26, 0, 0, 87,  69,  66,  80,  86,  80,  56,
      88, 10, 0,  0,  0,   18, 0, 0, 0,   219, 1,   0,   23,  1,   0,
      65, 78, 73, 77, 6,   0,  0, 0, 255, 255, 255, 255, 0,   0,   65,
      78, 77, 70, 20, 2,   0,  0, 0, 0,   0,   0,   0,   0,   219, 1,
      0,  23, 1,  0,  60,  0,  0, 3, 86,  80,  56,  76,  251, 1,   0};
  EXPECT_TRUE(IsAnimatedWebp(animated_webp_image_data));
}
