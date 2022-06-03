// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include "chrome/common/chrome_render_frame.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeRenderFrameObserverTest : public testing::Test {
 public:
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
};

TEST_F(ChromeRenderFrameObserverTest,
       NeedsDownscale_RequestLargeThanOriginalReturnFalse) {
  EXPECT_FALSE(
      NeedsDownscale(/* original_image_size */ gfx::Size(10, 10),
                     /* requested_image_min_area_pixels */ 200,
                     /* requested_image_max_size */ gfx::Size(20, 20)));
}

TEST_F(ChromeRenderFrameObserverTest, NeedsDownscale_SameSizeReturnFalse) {
  EXPECT_FALSE(
      NeedsDownscale(/* original_image_size */ gfx::Size(10, 10),
                     /* requested_image_min_area_pixels */ 100,
                     /* requested_image_max_size */ gfx::Size(10, 10)));
}

TEST_F(ChromeRenderFrameObserverTest, NeedsDownscale_OnlyWidthShortReturnTrue) {
  EXPECT_TRUE(NeedsDownscale(/* original_image_size */ gfx::Size(10, 10),
                             /* requested_image_min_area_pixels */ 100,
                             /* requested_image_max_size */ gfx::Size(9, 10)));
}

TEST_F(ChromeRenderFrameObserverTest, NeedsDownscale_OnlyAreaSmallReturnFalse) {
  EXPECT_FALSE(
      NeedsDownscale(/* original_image_size */ gfx::Size(10, 10),
                     /* requested_image_min_area_pixels */ 20,
                     /* requested_image_max_size */ gfx::Size(20, 20)));
}

TEST_F(ChromeRenderFrameObserverTest, NeedsEncodeImage_JpegFormat) {
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
}

TEST_F(ChromeRenderFrameObserverTest, NeedsEncodeImage_PngFormat) {
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
}

TEST_F(ChromeRenderFrameObserverTest, NeedsEncodeImage_OriginalFormat) {
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
}
