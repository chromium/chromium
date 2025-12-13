// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/omnibox/composebox/ios/composebox_image_helper_ios.h"

#import <UIKit/UIKit.h>

#include "base/memory/ref_counted_memory.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "ui/gfx/image/image.h"

namespace composebox {

namespace {

constexpr int kImageCompressionQuality = 30;
constexpr int kImageMaxArea = 1000000;
constexpr int kImageMaxHeight = 1000;
constexpr int kImageMaxWidth = 1000;

}  // namespace

class ComposeboxImageHelperIosTest : public PlatformTest {
 protected:
  // Creates a solid color UIImage of a given size in points.
  UIImage* CreateImage(CGSize point_size, bool opaque) {
    CGRect rect = CGRectMake(0, 0, point_size.width, point_size.height);
    // UIGraphicsBeginImageContextWithOptions with a scale of 0.0 will use the
    // device's main screen scale, which is what we want to simulate reality.
    UIGraphicsBeginImageContextWithOptions(rect.size, opaque, 0.0);
    CGContextRef context = UIGraphicsGetCurrentContext();
    [[UIColor greenColor] setFill];
    CGContextFillRect(context, rect);
    UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return image;
  }

  lens::ImageData DownscaleAndEncodeUiImage(UIImage* ui_image,
                                            bool enable_webp_encoding = true) {
    lens::ImageEncodingOptions image_options{
        .enable_webp_encoding = enable_webp_encoding,
        .max_size = kImageMaxArea,
        .max_height = kImageMaxHeight,
        .max_width = kImageMaxWidth,
        .compression_quality = kImageCompressionQuality,
    };
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
        base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
    return composebox::DownscaleAndEncodeImage(ui_image, ref_counted_logs,
                                               image_options);
  }

  // Verifies the encoded image data by decoding it and checking its properties.
  void VerifyEncodedData(const lens::ImageData& image_data,
                         int expected_pixel_width,
                         int expected_pixel_height) {
    // Check that the metadata correctly reports the expected pixel dimensions.
    EXPECT_EQ(expected_pixel_width, image_data.image_metadata().width());
    EXPECT_EQ(expected_pixel_height, image_data.image_metadata().height());

    // Perform a round-trip check to ensure the encoded bytes are valid.
    const std::string& bytes = image_data.payload().image_bytes();
    ASSERT_FALSE(bytes.empty());
    NSData* data = [NSData dataWithBytes:bytes.data() length:bytes.size()];
    UIImage* decoded_image = [UIImage imageWithData:data];
    ASSERT_TRUE(decoded_image);

    // The decoded image should have a scale of 1.0, so its point size should
    // match the pixel size in the metadata.
    EXPECT_EQ(image_data.image_metadata().width(),
              round(decoded_image.size.width));
    EXPECT_EQ(image_data.image_metadata().height(),
              round(decoded_image.size.height));
  }
};

TEST_F(ComposeboxImageHelperIosTest, DownscaleAndEncodeImageSmallSize) {
  UIImage* image = CreateImage(CGSizeMake(100, 100), /*opaque=*/true);
  lens::ImageData image_data = DownscaleAndEncodeUiImage(image);
  VerifyEncodedData(image_data, /*expected_pixel_width=*/100 * image.scale,
                    /*expected_pixel_height=*/100 * image.scale);
}

TEST_F(ComposeboxImageHelperIosTest, DownscaleAndEncodeImageLargeSize) {
  const int scale = 2;
  UIImage* image =
      CreateImage(CGSizeMake(kImageMaxWidth * scale, kImageMaxHeight * scale),
                  /*opaque=*/true);
  lens::ImageData image_data = DownscaleAndEncodeUiImage(image);
  VerifyEncodedData(image_data, kImageMaxWidth, kImageMaxHeight);
}

TEST_F(ComposeboxImageHelperIosTest, DownscaleAndEncodeImageHeightTooLarge) {
  const int scale = 2;
  UIImage* image = CreateImage(
      CGSizeMake(kImageMaxWidth, kImageMaxHeight * scale), /*opaque=*/true);
  lens::ImageData image_data = DownscaleAndEncodeUiImage(image);
  VerifyEncodedData(image_data, kImageMaxWidth / scale, kImageMaxHeight);
}

TEST_F(ComposeboxImageHelperIosTest, DownscaleAndEncodeImageWidthTooLarge) {
  const int scale = 2;
  UIImage* image = CreateImage(
      CGSizeMake(kImageMaxWidth * scale, kImageMaxHeight), /*opaque=*/true);
  lens::ImageData image_data = DownscaleAndEncodeUiImage(image);
  VerifyEncodedData(image_data, kImageMaxWidth, kImageMaxHeight / scale);
}

TEST_F(ComposeboxImageHelperIosTest, DownscaleAndEncodeImageTransparent) {
  UIImage* image = CreateImage(CGSizeMake(100, 100), /*opaque=*/false);
  lens::ImageData image_data = DownscaleAndEncodeUiImage(image);
  VerifyEncodedData(image_data, /*expected_pixel_width=*/100 * image.scale,
                    /*expected_pixel_height=*/100 * image.scale);
}

}  // namespace composebox
