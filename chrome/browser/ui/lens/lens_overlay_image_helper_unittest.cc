// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/lens/lens_overlay_colors.h"
#include "components/lens/lens_features.h"
#include "lens_overlay_image_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/geometry/rect.h"

namespace lens {

constexpr int kImageCompressionQuality = 30;
constexpr int kImageMaxArea = 1000000;
constexpr int kImageMaxHeight = 1000;
constexpr int kImageMaxWidth = 1000;
constexpr int kImageMaxAreaTier3 = 3000000;
constexpr int kImageMaxHeightTier3 = 3000;
constexpr int kImageMaxWidthTier3 = 3000;
constexpr int kImageMaxAreaTier2 = 2000000;
constexpr int kImageMaxHeightTier2 = 1500;
constexpr int kImageMaxWidthTier2 = 1500;
constexpr int kImageMaxAreaTier1 = 400000;
constexpr int kImageMaxHeightTier1 = 500;
constexpr int kImageMaxWidthTier1 = 500;
constexpr int kImageDownscaleUIScalingFactor = 2;

class LensOverlayImageHelperTest : public testing::Test {
 public:
  void SetUp() override {
    // Set all the feature params here to keep the test consistent if future
    // default values are changed.
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay,
        {{"enable-tiered-downscaling", "false"},
         {"image-compression-quality",
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

  std::string GetJpegBytesForBitmap(const SkBitmap& bitmap) {
    std::vector<unsigned char> data;
    gfx::JPEGCodec::Encode(bitmap, kImageCompressionQuality, &data);
    return std::string(data.begin(), data.end());
  }

  std::string GetWebpBytesForBitmap(const SkBitmap& bitmap) {
    std::vector<unsigned char> data;
    gfx::WebpCodec::Encode(bitmap, kImageCompressionQuality, &data);
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

  void EnableTieredDownscaling() {
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay,
        {{"enable-tiered-downscaling", "true"},
         {"image-compression-quality",
          base::StringPrintf("%d", kImageCompressionQuality)},
         {"image-dimensions-max-area", base::StringPrintf("%d", kImageMaxArea)},
         {"image-dimensions-max-height",
          base::StringPrintf("%d", kImageMaxHeight)},
         {"image-dimensions-max-width",
          base::StringPrintf("%d", kImageMaxWidth)},
         {"image-dimensions-max-area-tier-3",
          base::StringPrintf("%d", kImageMaxAreaTier3)},
         {"image-dimensions-max-height-tier-3",
          base::StringPrintf("%d", kImageMaxHeightTier3)},
         {"image-dimensions-max-width-tier-3",
          base::StringPrintf("%d", kImageMaxWidthTier3)},
         {"image-dimensions-max-area-tier-2",
          base::StringPrintf("%d", kImageMaxAreaTier2)},
         {"image-dimensions-max-height-tier-2",
          base::StringPrintf("%d", kImageMaxHeightTier2)},
         {"image-dimensions-max-width-tier-2",
          base::StringPrintf("%d", kImageMaxWidthTier2)},
         {"image-dimensions-max-area-tier-1",
          base::StringPrintf("%d", kImageMaxAreaTier1)},
         {"image-dimensions-max-height-tier-1",
          base::StringPrintf("%d", kImageMaxHeightTier1)},
         {"image-dimensions-max-width-tier-1",
          base::StringPrintf("%d", kImageMaxWidthTier1)},
         {"image-downscale-ui-scaling-factor",
          base::StringPrintf("%d", kImageDownscaleUIScalingFactor)}});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapMaxSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, 2, ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      1,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(6239, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(0)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapSmallSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(/*width=*/100, /*height=*/100);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, 2, ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  ASSERT_EQ(bitmap.width(), image_data.image_metadata().width());
  ASSERT_EQ(bitmap.height(), image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      1,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(359, ref_counted_logs->client_logs()
                     .phase_latencies_metadata()
                     .phase(0)
                     .image_encode_data()
                     .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapLargeSize) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth * scale, kImageMaxHeight * scale);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, 2, ref_counted_logs);

  const SkBitmap expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      2,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight * scale * scale,
            ref_counted_logs->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight, ref_counted_logs->client_logs()
                                                  .phase_latencies_metadata()
                                                  .phase(0)
                                                  .image_downscale_data()
                                                  .downscaled_image_size());
  ASSERT_EQ(6239, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapHeightTooLarge) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight * scale);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, 2, ref_counted_logs);

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
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, 2, ref_counted_logs);

  const SkBitmap expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight / scale);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // The image should have been resized and scaled down.
  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight / scale, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      2,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight * scale,
            ref_counted_logs->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight / scale,
            ref_counted_logs->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(3309, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest, AddSignificantRegions) {
  lens::ImageData image_data;
  std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes;
  gfx::Rect view_bounds(100, 150, 200, 100);
  gfx::Rect image_bounds(125, 25, 50, 50);
  significant_region_boxes.emplace_back(
      GetCenterRotatedBoxFromTabViewAndImageBounds(view_bounds, view_bounds,
                                                   image_bounds));

  AddSignificantRegions(image_data, std::move(significant_region_boxes));

  ASSERT_EQ(1, image_data.significant_regions_size());
  ASSERT_EQ(0.75, image_data.significant_regions(0).bounding_box().center_x());
  ASSERT_EQ(0.5, image_data.significant_regions(0).bounding_box().center_y());
  ASSERT_EQ(0.25, image_data.significant_regions(0).bounding_box().width());
  ASSERT_EQ(0.5, image_data.significant_regions(0).bounding_box().height());
  ASSERT_EQ(lens::CoordinateType::NORMALIZED,
            image_data.significant_regions(0).bounding_box().coordinate_type());
}

TEST_F(LensOverlayImageHelperTest, CropBitmapToRegion) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  gfx::Rect region(10, 10, 50, 50);
  std::string expected_output =
      GetJpegBytesForBitmap(CreateNonEmptyBitmap(50, 50));

  const SkBitmap cropped =
      lens::CropBitmapToRegion(bitmap, CenterBoxForRegion(region));

  ASSERT_EQ(expected_output, GetJpegBytesForBitmap(cropped));
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionNonRegionRequest) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  lens::mojom::CenterRotatedBoxPtr region;
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          bitmap, std::move(region), std::nullopt, ref_counted_logs);
  ASSERT_FALSE(image_crop.has_value());
  ASSERT_EQ(
      0,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapRegionMaxSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight);
  gfx::Rect region(0, 0, kImageMaxWidth, kImageMaxHeight);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          bitmap, CenterBoxForRegion(region), std::nullopt, ref_counted_logs);
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
  ASSERT_EQ(
      2,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight, ref_counted_logs->client_logs()
                                                  .phase_latencies_metadata()
                                                  .phase(0)
                                                  .image_downscale_data()
                                                  .original_image_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight, ref_counted_logs->client_logs()
                                                  .phase_latencies_metadata()
                                                  .phase(0)
                                                  .image_downscale_data()
                                                  .downscaled_image_size());
  ASSERT_EQ(6239, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest, DownscaleAndEncodeBitmapRegionSmallSize) {
  const SkBitmap bitmap = CreateNonEmptyBitmap(/*width=*/100, /*height=*/100);
  gfx::Rect region(10, 10, 50, 50);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          bitmap, CenterBoxForRegion(region), std::nullopt, ref_counted_logs);

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
  ASSERT_EQ(
      2,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(10000, ref_counted_logs->client_logs()
                       .phase_latencies_metadata()
                       .phase(0)
                       .image_downscale_data()
                       .original_image_size());
  ASSERT_EQ(2500, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(0)
                      .image_downscale_data()
                      .downscaled_image_size());
  ASSERT_EQ(309, ref_counted_logs->client_logs()
                     .phase_latencies_metadata()
                     .phase(1)
                     .image_encode_data()
                     .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionLargeFullImageSize) {
  const int scale = 2;
  const SkBitmap bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth * scale, kImageMaxHeight * scale);

  gfx::Rect region(10, 10, 50, 50);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          bitmap, CenterBoxForRegion(region), std::nullopt, ref_counted_logs);

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
  ASSERT_EQ(
      2,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight * scale * scale,
            ref_counted_logs->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(2500, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(0)
                      .image_downscale_data()
                      .downscaled_image_size());
  ASSERT_EQ(309, ref_counted_logs->client_logs()
                     .phase_latencies_metadata()
                     .phase(1)
                     .image_encode_data()
                     .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionLargeRegionSize) {
  const int full_image_scale = 3;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidth * full_image_scale, kImageMaxHeight * full_image_scale);

  const int region_scale = 2;
  gfx::Rect region(10, 10, kImageMaxWidth * region_scale,
                   kImageMaxHeight * region_scale);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          bitmap, CenterBoxForRegion(region), std::nullopt, ref_counted_logs);

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
  ASSERT_EQ(
      2,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(
      kImageMaxWidth * kImageMaxHeight * full_image_scale * full_image_scale,
      ref_counted_logs->client_logs()
          .phase_latencies_metadata()
          .phase(0)
          .image_downscale_data()
          .original_image_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight, ref_counted_logs->client_logs()
                                                  .phase_latencies_metadata()
                                                  .phase(0)
                                                  .image_downscale_data()
                                                  .downscaled_image_size());
  ASSERT_EQ(6239, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionWidthTooLarge) {
  const int full_image_scale = 3;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidth * full_image_scale, kImageMaxHeight * full_image_scale);

  const int region_scale = 2;
  gfx::Rect region(10, 10, kImageMaxWidth * region_scale, kImageMaxHeight);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          bitmap, CenterBoxForRegion(region), std::nullopt, ref_counted_logs);

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
  ASSERT_EQ(
      2,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(
      kImageMaxWidth * kImageMaxHeight * full_image_scale * full_image_scale,
      ref_counted_logs->client_logs()
          .phase_latencies_metadata()
          .phase(0)
          .image_downscale_data()
          .original_image_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight / region_scale,
            ref_counted_logs->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(3309, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapRegionHeightTooLarge) {
  const int full_image_scale = 3;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidth * full_image_scale, kImageMaxHeight * full_image_scale);

  const int region_scale = 2;
  gfx::Rect region(10, 10, kImageMaxWidth, kImageMaxHeight * region_scale);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          bitmap, CenterBoxForRegion(region), std::nullopt, ref_counted_logs);

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
  ASSERT_EQ(
      2,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(
      kImageMaxWidth * kImageMaxHeight * full_image_scale * full_image_scale,
      ref_counted_logs->client_logs()
          .phase_latencies_metadata()
          .phase(0)
          .image_downscale_data()
          .original_image_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight / region_scale,
            ref_counted_logs->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(3309, ref_counted_logs->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapWithOpaqueRegionBytes) {
  const SkBitmap image_bitmap = CreateNonEmptyBitmap(1000, 1000);
  SkBitmap region_bitmap = CreateNonEmptyBitmap(300, 300);
  region_bitmap.setAlphaType(kOpaque_SkAlphaType);
  gfx::Rect region(0, 0, 100, 100);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          image_bitmap, CenterBoxForRegion(region),
          std::make_optional<SkBitmap>(region_bitmap), ref_counted_logs);
  std::string expected_output = GetJpegBytesForBitmap(region_bitmap);

  ASSERT_EQ(1000, image_crop->zoomed_crop().parent_width());
  ASSERT_EQ(1000, image_crop->zoomed_crop().parent_height());
  ASSERT_EQ(3, image_crop->zoomed_crop().zoom());
  ASSERT_EQ(50, image_crop->zoomed_crop().crop().center_x());
  ASSERT_EQ(50, image_crop->zoomed_crop().crop().center_y());
  ASSERT_EQ(100, image_crop->zoomed_crop().crop().width());
  ASSERT_EQ(100, image_crop->zoomed_crop().crop().height());
  ASSERT_EQ(0, image_crop->zoomed_crop().crop().rotation_z());
  ASSERT_EQ(lens::CoordinateType::IMAGE,
            image_crop->zoomed_crop().crop().coordinate_type());
  ASSERT_EQ(expected_output, image_crop->image().image_content());
  ASSERT_EQ(
      1,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(827, ref_counted_logs->client_logs()
                     .phase_latencies_metadata()
                     .phase(0)
                     .image_encode_data()
                     .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest,
       DownscaleAndEncodeBitmapWithTransparentRegionBytes) {
  const SkBitmap image_bitmap = CreateNonEmptyBitmap(1000, 1000);
  const SkBitmap region_bitmap = CreateNonEmptyBitmap(300, 300);
  gfx::Rect region(0, 0, 100, 100);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  std::optional<lens::ImageCrop> image_crop =
      lens::DownscaleAndEncodeBitmapRegionIfNeeded(
          image_bitmap, CenterBoxForRegion(region),
          std::make_optional<SkBitmap>(region_bitmap), ref_counted_logs);
  std::string expected_output = GetWebpBytesForBitmap(region_bitmap);

  ASSERT_EQ(1000, image_crop->zoomed_crop().parent_width());
  ASSERT_EQ(1000, image_crop->zoomed_crop().parent_height());
  ASSERT_EQ(3, image_crop->zoomed_crop().zoom());
  ASSERT_EQ(50, image_crop->zoomed_crop().crop().center_x());
  ASSERT_EQ(50, image_crop->zoomed_crop().crop().center_y());
  ASSERT_EQ(100, image_crop->zoomed_crop().crop().width());
  ASSERT_EQ(100, image_crop->zoomed_crop().crop().height());
  ASSERT_EQ(0, image_crop->zoomed_crop().crop().rotation_z());
  ASSERT_EQ(lens::CoordinateType::IMAGE,
            image_crop->zoomed_crop().crop().coordinate_type());
  ASSERT_EQ(expected_output, image_crop->image().image_content());
  ASSERT_EQ(
      1,
      ref_counted_logs->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(280, ref_counted_logs->client_logs()
                     .phase_latencies_metadata()
                     .phase(0)
                     .image_encode_data()
                     .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest,
       GetCenterRotatedBoxFromTabViewAndImageBounds) {
  gfx::Rect tab_bounds(50, 50, 400, 200);
  gfx::Rect view_bounds(100, 150, 200, 100);
  gfx::Rect image_bounds(125, 25, 50, 50);

  auto result = GetCenterRotatedBoxFromTabViewAndImageBounds(
      tab_bounds, view_bounds, image_bounds);

  ASSERT_EQ(0.5, result->box.x());
  ASSERT_EQ(0.75, result->box.y());
  ASSERT_EQ(0.125, result->box.width());
  ASSERT_EQ(0.25, result->box.height());
  ASSERT_EQ(lens::mojom::CenterRotatedBox_CoordinateType::kNormalized,
            result->coordinate_type);
}

TEST_F(LensOverlayImageHelperTest,
       GetCenterRotatedBoxFromTabViewAndImageBoundsClipsWhenImageOutOfView) {
  gfx::Rect tab_bounds(0, 0, 400, 200);
  gfx::Rect view_bounds(0, 0, 200, 300);
  gfx::Rect image_bounds(100, 100, 200, 200);

  auto result = GetCenterRotatedBoxFromTabViewAndImageBounds(
      tab_bounds, view_bounds, image_bounds);

  ASSERT_EQ(0.375, result->box.x());
  ASSERT_EQ(0.75, result->box.y());
  ASSERT_EQ(0.25, result->box.width());
  ASSERT_EQ(0.5, result->box.height());
  ASSERT_EQ(lens::mojom::CenterRotatedBox_CoordinateType::kNormalized,
            result->coordinate_type);
}

TEST_F(LensOverlayImageHelperTest, ExtractVibrantOrDominantColorFromImage) {
  SkBitmap bitmap;
  // Larger than sample limit of ~10K pixels
  bitmap.allocN32Pixels(200, 200);
  // muted green for the whole image, #80C080
  bitmap.eraseColor(SkColorSetRGB(128, 192, 128));
  // vibrant green for 80x80, which is 16%
  bitmap.erase(SK_ColorGREEN, {40, 40, 120, 120});

  std::vector<color_utils::ColorProfile> profiles;
  // vibrant color profile
  profiles.emplace_back(color_utils::LumaRange::ANY,
                        color_utils::SaturationRange::VIBRANT);
  // any color profile
  profiles.emplace_back(color_utils::LumaRange::ANY,
                        color_utils::SaturationRange::ANY);

  auto vibrantAndDominantColors = color_utils::CalculateProminentColorsOfBitmap(
      bitmap, profiles, /*region=*/nullptr, color_utils::ColorSwatchFilter());

  EXPECT_EQ(SK_ColorGREEN, vibrantAndDominantColors[0].color);
  EXPECT_NEAR(0.16,
              static_cast<float>(vibrantAndDominantColors[0].population) /
                  color_utils::kMaxConsideredPixelsForSwatches,
              0.001);
  EXPECT_EQ(SkColorSetRGB(128, 192, 128), vibrantAndDominantColors[1].color);
  EXPECT_NEAR(0.84,
              static_cast<float>(vibrantAndDominantColors[1].population) /
                  color_utils::kMaxConsideredPixelsForSwatches,
              0.001);

  // Happy path, green.
  {
    SkColor color = ExtractVibrantOrDominantColorFromImage(bitmap, 0.15f);
    EXPECT_EQ(SK_ColorGREEN, color);
  }

  // Not enough pixels for green, muted green.
  {
    SkColor color = ExtractVibrantOrDominantColorFromImage(bitmap, 0.2f);
    EXPECT_EQ(SkColorSetRGB(128, 192, 128) /* Muted green */, color);
  }

  // Not enough pixels for green, background dark gray is
  // not colorful enough, extraction fails and returns
  // transparent.
  {
    // dark gray for the whole image
    bitmap.eraseColor(SK_ColorDKGRAY);
    // vibrant green for 40x40, which is 16%
    bitmap.erase(SK_ColorGREEN, {40, 40, 120, 120});

    SkColor color = ExtractVibrantOrDominantColorFromImage(bitmap, 0.17f);
    EXPECT_EQ(SK_ColorTRANSPARENT, color);
  }

  // No colors qualify for vibrant.
  {
    // Muted green for the whole image, #80C080, not vibrant, dominant.
    bitmap.eraseColor(SkColorSetRGB(128, 192, 128));
    // #73904b, HSL S value < 35%, not vibrant
    bitmap.erase(SkColorSetRGB(115, 144, 75), {40, 40, 120, 120});

    SkColor color = ExtractVibrantOrDominantColorFromImage(bitmap, 0.15f);
    EXPECT_EQ(SkColorSetRGB(128, 192, 128), color);
  }

  // Colors qualify for vibrant.
  {
    // Muted green for the whole image, #80C080, not vibrant, dominant.
    bitmap.eraseColor(SkColorSetRGB(128, 192, 128));
    // #73a54b, HSL S value > 35%, considered vibrant
    bitmap.erase(SkColorSetRGB(115, 165, 75), {40, 40, 120, 120});

    SkColor color = ExtractVibrantOrDominantColorFromImage(bitmap, 0.15f);
    EXPECT_EQ(SkColorSetRGB(115, 165, 75), color);
  }

  // Test small bitmap, fewer than sample limit of ~10K pixels
  {
    SkBitmap small_bitmap;
    small_bitmap.allocN32Pixels(50, 50);
    // muted green for the whole image, #80C080
    small_bitmap.eraseColor(SkColorSetRGB(128, 192, 128));
    // vibrant green for 80x80, which is 16%
    small_bitmap.erase(SK_ColorGREEN, {10, 10, 30, 30});

    SkColor color = ExtractVibrantOrDominantColorFromImage(small_bitmap, 0.15f);
    EXPECT_EQ(SK_ColorGREEN, color);

    color = ExtractVibrantOrDominantColorFromImage(small_bitmap, 0.17f);
    EXPECT_EQ(SkColorSetRGB(128, 192, 128) /* Muted green */, color);
  }
}

TEST_F(LensOverlayImageHelperTest, ConvertColorToLab) {
  SkColor input_rgb[] = {SK_ColorBLACK, SK_ColorWHITE,  SK_ColorRED,
                         SK_ColorGREEN, SK_ColorBLUE,   SK_ColorYELLOW,
                         SK_ColorCYAN,  SK_ColorMAGENTA};

  // Conversion values from
  // https://colorjs.io/apps/convert/?color=magenta&precision=4
  std::tuple<float, float, float> output_lab[] = {
      {0.0, 0.0, 0.0},         {100.0, 0.0, 0.0},       {54.29, 80.80, 69.89},
      {87.82, -79.27, 80.99},  {29.57, 68.30, -112.03}, {97.61, -15.75, 93.39},
      {90.67, -50.66, -14.96}, {60.17, 93.54, -60.50}};

  int index = 0;
  for (auto rgb : input_rgb) {
    auto [l, a, b] = ConvertColorToLab(rgb);
    auto [expected_l, expected_a, expected_b] = output_lab[index];
    EXPECT_NEAR(expected_l, l, 0.05);
    EXPECT_NEAR(expected_a, a, 0.05);
    EXPECT_NEAR(expected_b, b, 0.05);
    index++;
  }
}

TEST_F(LensOverlayImageHelperTest, ColorUtilityFunctions) {
  EXPECT_NEAR(0.0, CalculateChroma(ConvertColorToLab(SK_ColorDKGRAY)), 1.0);
  EXPECT_NEAR(111.4, CalculateChroma(ConvertColorToLab(SK_ColorMAGENTA)), 1.0);
  EXPECT_NEAR(72.0,
              CalculateChroma(ConvertColorToLab(SkColorSetRGB(80, 200, 80))),
              1.0);

  EXPECT_NEAR(0.71, CalculateHueAngle(ConvertColorToLab(SK_ColorRED)).value(),
              0.01);
  EXPECT_NEAR(-1.023,
              CalculateHueAngle(ConvertColorToLab(SK_ColorBLUE)).value(), 0.01);
  EXPECT_NEAR(-0.574,
              CalculateHueAngle(ConvertColorToLab(SK_ColorMAGENTA)).value(),
              0.01);

  EXPECT_FALSE(CalculateHueAngle({100, 0, 0}).has_value());

  EXPECT_FALSE(CalculateHueAngleDistance(ConvertColorToLab(SK_ColorRED),
                                         {29.0f, 0.0f, 0.0f})
                   .has_value());
  EXPECT_FALSE(CalculateHueAngleDistance({29.0f, 0.0f, 0.0f},
                                         ConvertColorToLab(SK_ColorCYAN))
                   .has_value());
  EXPECT_NEAR(1.028,
              CalculateHueAngleDistance(ConvertColorToLab(SK_ColorRED),
                                        ConvertColorToLab(SK_ColorYELLOW))
                  .value(),
              0.01);
}

TEST_F(LensOverlayImageHelperTest, FindBestMatchedColorOrTransparent) {
  std::vector<SkColor> colors;
  for (const auto& pair : kPalettes) {
    colors.emplace_back(pair.first);
  }
  // No match for close to grayscale colors
  EXPECT_EQ(SK_ColorTRANSPARENT,
            FindBestMatchedColorOrTransparent(colors, SK_ColorWHITE, 3.0f));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            FindBestMatchedColorOrTransparent(colors, SK_ColorGRAY, 3.0f));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            FindBestMatchedColorOrTransparent(colors, SK_ColorBLACK, 3.0f));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            FindBestMatchedColorOrTransparent(
                colors, SkColorSetRGB(0x43, 0x46, 0x44), 3.0f));
  // Closest matching colors.
  EXPECT_EQ(kColorGrapePrimary,
            FindBestMatchedColorOrTransparent(
                colors, SkColorSetRGB(0x50, 0x12, 0xC4), 3.0f));
  EXPECT_EQ(kColorTurquoisePrimary,
            FindBestMatchedColorOrTransparent(colors, SK_ColorCYAN, 3.0f));
  EXPECT_EQ(kColorTangerinePrimary,
            FindBestMatchedColorOrTransparent(colors, SK_ColorRED, 3.0f));
  EXPECT_EQ(kColorCactusPrimary,
            FindBestMatchedColorOrTransparent(colors, SK_ColorGREEN, 3.0f));
  EXPECT_EQ(kColorSchoolbusPrimary,
            FindBestMatchedColorOrTransparent(
                colors, SkColorSetRGB(0x48, 0x39, 0x12), 3.0f));
}

TEST_F(LensOverlayImageHelperTest, TieredDownscalingTier3) {
  EnableTieredDownscaling();

  int image_scale = 2;
  int ui_scale = 1;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidthTier3 * image_scale, kImageMaxHeightTier3);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_logs_scale1 =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, ui_scale, ref_logs_scale1);

  SkBitmap expected_bitmap = CreateNonEmptyBitmap(
      kImageMaxWidthTier3, kImageMaxHeightTier3 / image_scale);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // Downscales to Tier 3 when UI scale is less than finch defined UI scaling
  // factor threshold (kImageDownscaleUIScalingFactor).
  ASSERT_EQ(kImageMaxWidthTier3, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeightTier3 / image_scale,
            image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      2,
      ref_logs_scale1->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidthTier3 * kImageMaxHeightTier3 * image_scale,
            ref_logs_scale1->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(kImageMaxWidthTier3 * kImageMaxHeightTier3 / image_scale,
            ref_logs_scale1->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(26793, ref_logs_scale1->client_logs()
                       .phase_latencies_metadata()
                       .phase(1)
                       .image_encode_data()
                       .encoded_image_size_bytes());

  ui_scale = 2;
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_logs_scale2 =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, ui_scale, ref_logs_scale2);

  expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth, kImageMaxHeight / image_scale);
  expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // Downscales to Tier 1.5 when UI scale is more than finch defined UI scaling
  // factor threshold (kImageDownscaleUIScalingFactor).
  ASSERT_EQ(kImageMaxWidth, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight / image_scale,
            image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      2,
      ref_logs_scale2->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidthTier3 * kImageMaxHeightTier3 * image_scale,
            ref_logs_scale2->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight / image_scale,
            ref_logs_scale2->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(3309, ref_logs_scale2->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest, TieredDownscalingTier2) {
  EnableTieredDownscaling();

  int image_scale = 2;
  int ui_scale = 1;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidthTier2, kImageMaxHeightTier2 * image_scale);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_logs_scale1 =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, ui_scale, ref_logs_scale1);

  SkBitmap expected_bitmap = CreateNonEmptyBitmap(
      kImageMaxWidthTier2 / image_scale, kImageMaxHeightTier2);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // Downscales to Tier 2 when UI scale is less than finch defined UI scaling
  // factor threshold (kImageDownscaleUIScalingFactor).
  ASSERT_EQ(kImageMaxWidthTier2 / image_scale,
            image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeightTier2, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      2,
      ref_logs_scale1->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidthTier2 * kImageMaxHeightTier2 * image_scale,
            ref_logs_scale1->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(kImageMaxWidthTier2 * kImageMaxHeightTier2 / image_scale,
            ref_logs_scale1->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(6912, ref_logs_scale1->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());

  ui_scale = 2;
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_logs_scale2 =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, ui_scale, ref_logs_scale2);

  expected_bitmap =
      CreateNonEmptyBitmap(kImageMaxWidth / image_scale, kImageMaxHeight);
  expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // Downscales to Tier 1.5 when UI scale is more than finch defined UI scaling
  // factor threshold (kImageDownscaleUIScalingFactor).
  ASSERT_EQ(kImageMaxWidth / image_scale, image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeight, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      2,
      ref_logs_scale2->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidthTier2 * kImageMaxHeightTier2 * image_scale,
            ref_logs_scale2->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(kImageMaxWidth * kImageMaxHeight / image_scale,
            ref_logs_scale2->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(3309, ref_logs_scale2->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest, TieredDownscalingTier1) {
  EnableTieredDownscaling();

  int image_scale = 2;
  int ui_scale = 1;
  const SkBitmap bitmap = CreateNonEmptyBitmap(
      kImageMaxWidthTier1, kImageMaxHeightTier1 * image_scale);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_logs_scale1 =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, ui_scale, ref_logs_scale1);

  SkBitmap expected_bitmap = CreateNonEmptyBitmap(
      kImageMaxWidthTier1 / image_scale, kImageMaxHeightTier1);
  std::string expected_output = GetJpegBytesForBitmap(expected_bitmap);

  // Downscales to Tier 1 when UI scale is less than finch defined UI scaling
  // factor threshold (kImageDownscaleUIScalingFactor).
  ASSERT_EQ(kImageMaxWidthTier1 / image_scale,
            image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeightTier1, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      2,
      ref_logs_scale1->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidthTier1 * kImageMaxHeightTier1 * image_scale,
            ref_logs_scale1->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(kImageMaxWidthTier1 * kImageMaxHeightTier1 / image_scale,
            ref_logs_scale1->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(1053, ref_logs_scale1->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());

  ui_scale = 2;
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_logs_scale2 =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, ui_scale, ref_logs_scale2);

  // Downscales to Tier 1 when UI scale is less than finch defined UI scaling
  // factor threshold (kImageDownscaleUIScalingFactor). Essentially verify that
  // ui_scale is ignored at Tier 1.
  ASSERT_EQ(kImageMaxWidthTier1 / image_scale,
            image_data.image_metadata().width());
  ASSERT_EQ(kImageMaxHeightTier1, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      2,
      ref_logs_scale2->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(kImageMaxWidthTier1 * kImageMaxHeightTier1 * image_scale,
            ref_logs_scale2->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .original_image_size());
  ASSERT_EQ(kImageMaxWidthTier1 * kImageMaxHeightTier1 / image_scale,
            ref_logs_scale2->client_logs()
                .phase_latencies_metadata()
                .phase(0)
                .image_downscale_data()
                .downscaled_image_size());
  ASSERT_EQ(1053, ref_logs_scale2->client_logs()
                      .phase_latencies_metadata()
                      .phase(1)
                      .image_encode_data()
                      .encoded_image_size_bytes());
}

TEST_F(LensOverlayImageHelperTest, TieredDownscalingNoCompression) {
  EnableTieredDownscaling();

  int ui_scale = 1;
  const SkBitmap bitmap = CreateNonEmptyBitmap(/*width=*/100, /*height=*/100);
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_logs_scale1 =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  lens::ImageData image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, ui_scale, ref_logs_scale1);

  std::string expected_output = GetJpegBytesForBitmap(bitmap);

  // No downscaling when image is less than Tier 1.
  ASSERT_EQ(100, image_data.image_metadata().width());
  ASSERT_EQ(100, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      1,
      ref_logs_scale1->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(359, ref_logs_scale1->client_logs()
                     .phase_latencies_metadata()
                     .phase(0)
                     .image_encode_data()
                     .encoded_image_size_bytes());

  ui_scale = 2;
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_logs_scale2 =
      base::MakeRefCounted<lens::RefCountedLensOverlayClientLogs>();
  image_data =
      lens::DownscaleAndEncodeBitmap(bitmap, ui_scale, ref_logs_scale2);

  // Verify that UI Scale does not change no compression flow
  ASSERT_EQ(100, image_data.image_metadata().width());
  ASSERT_EQ(100, image_data.image_metadata().height());
  ASSERT_EQ(expected_output, image_data.payload().image_bytes());
  ASSERT_EQ(
      1,
      ref_logs_scale2->client_logs().phase_latencies_metadata().phase_size());
  ASSERT_EQ(359, ref_logs_scale2->client_logs()
                     .phase_latencies_metadata()
                     .phase(0)
                     .image_encode_data()
                     .encoded_image_size_bytes());
}
}  // namespace lens
