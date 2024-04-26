// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/lens/lens_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"

namespace lens {

constexpr int kImageCompressionQuality = 30;
constexpr int kImageMaxArea = 1000000;
constexpr int kImageMaxHeight = 1000;
constexpr int kImageMaxWidth = 1000;

class LensOverlayImageHelperTest : public testing::Test {
 public:
  void SetUp() override {
    // Set all the feature params here to keep the test consistent if future
    // default values are changed.
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay,
        {{"image-compression-quality",
          base::StringPrintf("%d", kImageCompressionQuality)},
         {"image-dimensions-max-area", base::StringPrintf("%d", kImageMaxArea)},
         {"image-dimensions-max-height",
          base::StringPrintf("%d", kImageMaxHeight)},
         {"image-dimensions-max-width",
          base::StringPrintf("%d", kImageMaxWidth)}});
  }

  const SkBitmap CreateNonEmptyBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorGREEN);
    return bitmap;
  }

  std::string GetJpegBytesForBitmap(const SkBitmap bitmap) {
    std::vector<unsigned char> data;
    gfx::JPEGCodec::Encode(bitmap, kImageCompressionQuality, &data);
    return std::string(data.begin(), data.end());
  }

  // Helper to create a region search region for the given rect.
  lens::mojom::CenterRotatedBoxPtr CenterBoxForRegion(const gfx::Rect& region) {
    auto box = lens::mojom::CenterRotatedBox::New();
    box->box = gfx::RectF(region.CenterPoint().x(), region.CenterPoint().y(),
                          region.width(), region.height());
    box->coordinate_type = lens::mojom::CenterRotatedBox_CoordinateType::kImage;
    return box;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapMaxSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapSmallSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(/*width=*/100, /*height=*/100);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  ASSERT_EQ(bitmap.width(), image_data.image_metadata().width());
  ASSERT_EQ(bitmap.height(), image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapLargeSize) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth * scale, kImageMaxHeight * scale);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);

  const SkBitmap expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapHeightTooLarge) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight * scale);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);

  const SkBitmap expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth / scale, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  ASSERT_EQ(kImageMaxWidth / scale, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapWidthTooLarge) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth * scale, kImageMaxHeight);
  lens::ImageData image_data = lens::DownscaleAndEncodeBitmap(bitmap);

  const SkBitmap expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight / scale);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight / scale, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionNonRegionRequest) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  lens::mojom::CenterRotatedBoxPtr region;
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(bitmap, std::move(region));
  ASSERT_FALSE(image_crop.has_value());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapRegionMaxSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  gfx::Rect region(0, 0, kImageMaxWidth, kImageMaxHeight);
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(bitmap,
                                                   CenterBoxForRegion(region));
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  ASSERT_EQ(kImageMaxWidth, image_crop->zoomed_crop().parent_width());
  ASSERT_EQ(kImageMaxHeight, image_crop->zoomed_crop().parent_height());
  ASSERT_EQ(1, image_crop->zoomed_crop().zoom());
  ASSERT_EQ(kImageMaxWidth * .5, image_crop->zoomed_crop().crop().center_x());
  ASSERT_EQ(kImageMaxHeight * .5, image_crop->zoomed_crop().crop().center_y());
  ASSERT_EQ(kImageMaxWidth, image_crop->zoomed_crop().crop().width());
  ASSERT_EQ(kImageMaxHeight, image_crop->zoomed_crop().crop().height());
  ASSERT_EQ(0, image_crop->zoomed_crop().crop().rotation_z());
  ASSERT_EQ(lens::CoordinateType::IMAGE,
            image_crop->zoomed_crop().crop().coordinate_type());
  ASSERT_EQ(expected_output, image_crop->image().image_content());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapRegionSmallSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(/*width=*/100, /*height=*/100);
  gfx::Rect region(10, 10, 50, 50);
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(bitmap,
                                                   CenterBoxForRegion(region));

  const SkBitmap region_bitmap =
      CreateNonEmptyBitmap(/*width=*/50, /*height=*/50);
  std::string expected_output = GetJpegBytesForBitmap(region_bitmap);

  ASSERT_EQ(100, image_crop->zoomed_crop().parent_width());
  ASSERT_EQ(100, image_crop->zoomed_crop().parent_height());
  ASSERT_EQ(1, image_crop->zoomed_crop().zoom());
  ASSERT_EQ(35, image_crop->zoomed_crop().crop().center_x());
  ASSERT_EQ(35, image_crop->zoomed_crop().crop().center_y());
  ASSERT_EQ(50, image_crop->zoomed_crop().crop().width());
  ASSERT_EQ(50, image_crop->zoomed_crop().crop().height());
  ASSERT_EQ(0, image_crop->zoomed_crop().crop().rotation_z());
  ASSERT_EQ(lens::CoordinateType::IMAGE,
            image_crop->zoomed_crop().crop().coordinate_type());
  ASSERT_EQ(expected_output, image_crop->image().image_content());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionLargeFullImageSize) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth * scale, kImageMaxHeight * scale);

  gfx::Rect region(10, 10, 50, 50);
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(bitmap,
                                                   CenterBoxForRegion(region));

  const SkBitmap region_bitmap =
      CreateNonEmptyBitmap(/*width=*/50, /*height=*/50);
  std::string expected_output = GetJpegBytesForBitmap(region_bitmap);

  ASSERT_EQ(kImageMaxWidth * scale, image_crop->zoomed_crop().parent_width());
  ASSERT_EQ(kImageMaxHeight * scale, image_crop->zoomed_crop().parent_height());
  ASSERT_EQ(1, image_crop->zoomed_crop().zoom());
  ASSERT_EQ(35, image_crop->zoomed_crop().crop().center_x());
  ASSERT_EQ(35, image_crop->zoomed_crop().crop().center_y());
  ASSERT_EQ(50, image_crop->zoomed_crop().crop().width());
  ASSERT_EQ(50, image_crop->zoomed_crop().crop().height());
  ASSERT_EQ(0, image_crop->zoomed_crop().crop().rotation_z());
  ASSERT_EQ(lens::CoordinateType::IMAGE,
            image_crop->zoomed_crop().crop().coordinate_type());
  ASSERT_EQ(expected_output, image_crop->image().image_content());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionLargeRegionSize) {
  const int full_image_scale = 3;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidth * full_image_scale, kImageMaxHeight * full_image_scale);

  const int region_scale = 2;
  gfx::Rect region(10, 10, kImageMaxWidth * region_scale,
                   kImageMaxHeight * region_scale);
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(bitmap,
                                                   CenterBoxForRegion(region));

  const SkBitmap region_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(region_bitmap);

  ASSERT_EQ(kImageMaxWidth * full_image_scale,
            image_crop->zoomed_crop().parent_width());
  ASSERT_EQ(kImageMaxHeight * full_image_scale,
            image_crop->zoomed_crop().parent_height());
  ASSERT_EQ(.5, image_crop->zoomed_crop().zoom());
  ASSERT_EQ(10 + kImageMaxWidth, image_crop->zoomed_crop().crop().center_x());
  ASSERT_EQ(10 + kImageMaxHeight, image_crop->zoomed_crop().crop().center_y());
  ASSERT_EQ(kImageMaxWidth * region_scale,
            image_crop->zoomed_crop().crop().width());
  ASSERT_EQ(kImageMaxHeight * region_scale,
            image_crop->zoomed_crop().crop().height());
  ASSERT_EQ(0, image_crop->zoomed_crop().crop().rotation_z());
  ASSERT_EQ(lens::CoordinateType::IMAGE,
            image_crop->zoomed_crop().crop().coordinate_type());
  ASSERT_EQ(expected_output, image_crop->image().image_content());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionWidthTooLarge) {
  const int full_image_scale = 3;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidth * full_image_scale, kImageMaxHeight * full_image_scale);

  const int region_scale = 2;
  gfx::Rect region(10, 10, kImageMaxWidth * region_scale, kImageMaxHeight);
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(bitmap,
                                                   CenterBoxForRegion(region));

  const SkBitmap region_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight / region_scale);
  std::string expected_output = GetJpegBytesForBitmap(region_bitmap);

  ASSERT_EQ(kImageMaxWidth * full_image_scale,
            image_crop->zoomed_crop().parent_width());
  ASSERT_EQ(kImageMaxHeight * full_image_scale,
            image_crop->zoomed_crop().parent_height());
  ASSERT_EQ(.5, image_crop->zoomed_crop().zoom());
  ASSERT_EQ(10 + kImageMaxWidth, image_crop->zoomed_crop().crop().center_x());
  ASSERT_EQ(10 + kImageMaxHeight / 2,
            image_crop->zoomed_crop().crop().center_y());
  ASSERT_EQ(kImageMaxWidth * region_scale,
            image_crop->zoomed_crop().crop().width());
  ASSERT_EQ(kImageMaxHeight, image_crop->zoomed_crop().crop().height());
  ASSERT_EQ(0, image_crop->zoomed_crop().crop().rotation_z());
  ASSERT_EQ(lens::CoordinateType::IMAGE,
            image_crop->zoomed_crop().crop().coordinate_type());
  ASSERT_EQ(expected_output, image_crop->image().image_content());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionHeightTooLarge) {
  const int full_image_scale = 3;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidth * full_image_scale, kImageMaxHeight * full_image_scale);

  const int region_scale = 2;
  gfx::Rect region(10, 10, kImageMaxWidth, kImageMaxHeight * region_scale);
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(bitmap,
                                                   CenterBoxForRegion(region));

  const SkBitmap region_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth / region_scale, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(region_bitmap);

  ASSERT_EQ(kImageMaxWidth * full_image_scale,
            image_crop->zoomed_crop().parent_width());
  ASSERT_EQ(kImageMaxHeight * full_image_scale,
            image_crop->zoomed_crop().parent_height());
  ASSERT_EQ(.5, image_crop->zoomed_crop().zoom());
  ASSERT_EQ(10 + kImageMaxWidth / 2,
            image_crop->zoomed_crop().crop().center_x());
  ASSERT_EQ(10 + kImageMaxHeight, image_crop->zoomed_crop().crop().center_y());
  ASSERT_EQ(kImageMaxWidth, image_crop->zoomed_crop().crop().width());
  ASSERT_EQ(kImageMaxHeight * region_scale,
            image_crop->zoomed_crop().crop().height());
  ASSERT_EQ(0, image_crop->zoomed_crop().crop().rotation_z());
  ASSERT_EQ(lens::CoordinateType::IMAGE,
            image_crop->zoomed_crop().crop().coordinate_type());
  ASSERT_EQ(expected_output, image_crop->image().image_content());
}

}  // namespace lens
