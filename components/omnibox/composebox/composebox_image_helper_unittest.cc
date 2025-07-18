// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_image_helper.h"

#include <array>

#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/geometry/rect.h"

namespace composebox {

constexpr int kImageCompressionQuality = 30;
constexpr int kImageMaxArea = 1000000;
constexpr int kImageMaxHeight = 1000;
constexpr int kImageMaxWidth = 1000;

class ComposeboxImageHelperTest : public testing::Test {
 protected:
  const SkBitmap CreateOpaqueBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorGREEN);
    bitmap.setAlphaType(kOpaque_SkAlphaType);
    return bitmap;
  }

  lens::ImageData DownscaleAndEncodeBitmap(
      SkBitmap bitmap,
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      bool enable_webp_encoding = true) {
    ImageEncodingOptions image_options{
        .enable_webp_encoding = enable_webp_encoding,
        .max_size = kImageMaxArea,
        .max_height = kImageMaxHeight,
        .max_width = kImageMaxWidth,
        .compression_quality = kImageCompressionQuality,
    };
    return composebox::DownscaleAndEncodeBitmap(bitmap, ref_counted_logs,
                                                image_options);
  }

  std::string GetJpegBytesForBitmap(const SkBitmap& bitmap) {
    std::optional<std::vector<uint8_t>> data =
        gfx::JPEGCodec::Encode(bitmap, kImageCompressionQuality);
    return std::string(base::as_string_view(data.value()));
  }

  std::string GetWebpBytesForBitmap(const SkBitmap& bitmap) {
    std::optional<std::vector<uint8_t>> data =
        gfx::WebpCodec::Encode(bitmap, kImageCompressionQuality);
    return std::string(base::as_string_view(data.value()));
  }
};

TEST_F(ComposeboxImageHelperTest, DownscaleAndEncodeBitmapMaxSize) {
  const SkBitmap bitmap = CreateOpaqueBitmap(kImageMaxWidth, kImageMaxHeight);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      DownscaleAndEncodeBitmap(bitmap, ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  EXPECT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  EXPECT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  EXPECT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(ComposeboxImageHelperTest, DownscaleAndEncodeBitmapSmallSize) {
  const SkBitmap bitmap = CreateOpaqueBitmap(/*width=*/100, /*height=*/100);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      DownscaleAndEncodeBitmap(bitmap, ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  EXPECT_EQ(bitmap.width(), image_data.image_metadata().width());
  EXPECT_EQ(bitmap.height(), image_data.image_metadata().height());
  EXPECT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(ComposeboxImageHelperTest, DownscaleAndEncodeBitmapLargeSize) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateOpaqueBitmap(kImageMaxWidth * scale, kImageMaxHeight * scale);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      DownscaleAndEncodeBitmap(bitmap, ref_counted_logs);

  const SkBitmap expected_bitmap =
      CreateOpaqueBitmap(kImageMaxWidth, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  EXPECT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  EXPECT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  EXPECT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(ComposeboxImageHelperTest, DownscaleAndEncodeBitmapHeightTooLarge) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateOpaqueBitmap(kImageMaxWidth, kImageMaxHeight * scale);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      DownscaleAndEncodeBitmap(bitmap, ref_counted_logs);

  const SkBitmap expected_bitmap =
      CreateOpaqueBitmap(kImageMaxWidth / scale, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  EXPECT_EQ(kImageMaxWidth / scale, image_data.image_metadata().width());
  EXPECT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  EXPECT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(ComposeboxImageHelperTest, DownscaleAndEncodeBitmapWidthTooLarge) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateOpaqueBitmap(kImageMaxWidth * scale, kImageMaxHeight);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      DownscaleAndEncodeBitmap(bitmap, ref_counted_logs);

  const SkBitmap expected_bitmap =
      CreateOpaqueBitmap(kImageMaxWidth, kImageMaxHeight / scale);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  EXPECT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  EXPECT_EQ(kImageMaxHeight / scale, image_data.image_metadata().height());
  EXPECT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(ComposeboxImageHelperTest, DownscaleAndEncodeBitmapTransparent) {
  // Create a bitmap. Since it isn't marked with kOpaque_SkAlphaType the
  // output should be WebP instead of JPEG.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(/*width=*/100, /*height=*/100);
  bitmap.eraseColor(SK_ColorGREEN);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      DownscaleAndEncodeBitmap(bitmap, ref_counted_logs);
  std::string expected_output = GetWebpBytesForBitmap(bitmap);

  EXPECT_EQ(bitmap.width(), image_data.image_metadata().width());
  EXPECT_EQ(bitmap.height(), image_data.image_metadata().height());
  EXPECT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(ComposeboxImageHelperTest,
       DownscaleAndEncodeBitmapTransparentWebpDisabled) {
  // Create a bitmap. Since it isn't marked with kOpaque_SkAlphaType the
  // output should be WebP instead of JPEG.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(/*width=*/100, /*height=*/100);
  bitmap.eraseColor(SK_ColorGREEN);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data = DownscaleAndEncodeBitmap(
      bitmap, ref_counted_logs, /*enable_webp_encoding=*/false);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  EXPECT_EQ(bitmap.width(), image_data.image_metadata().width());
  EXPECT_EQ(bitmap.height(), image_data.image_metadata().height());
  EXPECT_EQ(expected_output, image_data.payload().image_bytes());
}

}  // namespace composebox
