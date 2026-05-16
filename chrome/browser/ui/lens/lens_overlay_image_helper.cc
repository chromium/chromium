// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"

#include <numbers>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/lens_features.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_phase_latencies_metadata.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

bool ShouldDownscaleSizeWithUiScaling(const gfx::Size& size,
                                      int max_area,
                                      int max_width,
                                      int max_height,
                                      int ui_scale_factor) {
  if (ui_scale_factor <= 0) {
    return lens::ShouldDownscaleSize(size, max_area, max_width, max_height);
  }
  return ui_scale_factor <
             lens::features::
                 GetLensOverlayImageDownscaleUiScalingFactorThreshold() &&
         lens::ShouldDownscaleSize(size, max_area, max_width, max_height);
}

SkBitmap DownscaleImageIfNeededWithTieredApproach(
    const SkBitmap& image,
    int ui_scale_factor,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  auto size = gfx::Size(image.width(), image.height());
  // Tier 3 Downscaling.
  if (ShouldDownscaleSizeWithUiScaling(
          size, lens::features::GetLensOverlayImageMaxAreaTier3(),
          lens::features::GetLensOverlayImageMaxWidthTier3(),
          lens::features::GetLensOverlayImageMaxHeightTier3(),
          ui_scale_factor)) {
    return DownscaleImage(
        image, lens::features::GetLensOverlayImageMaxWidthTier3(),
        lens::features::GetLensOverlayImageMaxHeightTier3(), client_logs);
    // Tier 2 Downscaling.
  } else if (ShouldDownscaleSizeWithUiScaling(
                 size, lens::features::GetLensOverlayImageMaxAreaTier2(),
                 lens::features::GetLensOverlayImageMaxWidthTier2(),
                 lens::features::GetLensOverlayImageMaxHeightTier2(),
                 ui_scale_factor)) {
    return DownscaleImage(
        image, lens::features::GetLensOverlayImageMaxWidthTier2(),
        lens::features::GetLensOverlayImageMaxHeightTier2(), client_logs);
    // Tier 1.5 Downscaling.
  } else if (lens::ShouldDownscaleSize(
                 size, lens::features::GetLensOverlayImageMaxAreaTier2(),
                 lens::features::GetLensOverlayImageMaxWidthTier2(),
                 lens::features::GetLensOverlayImageMaxHeightTier2())) {
    return DownscaleImage(image, lens::features::GetLensOverlayImageMaxWidth(),
                          lens::features::GetLensOverlayImageMaxHeight(),
                          client_logs);
    // Tier 1 Downscaling.
  } else if (lens::ShouldDownscaleSize(
                 size, lens::features::GetLensOverlayImageMaxAreaTier1(),
                 lens::features::GetLensOverlayImageMaxWidthTier1(),
                 lens::features::GetLensOverlayImageMaxHeightTier1())) {
    return DownscaleImage(
        image, lens::features::GetLensOverlayImageMaxWidthTier1(),
        lens::features::GetLensOverlayImageMaxHeightTier1(), client_logs);
  }

  // No downscaling needed.
  return image;
}

SkBitmap DownscaleImageIfNeeded(
    const SkBitmap& image,
    int ui_scale_factor,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  if (lens::features::LensOverlayUseTieredDownscaling()) {
    return DownscaleImageIfNeededWithTieredApproach(image, ui_scale_factor,
                                                    client_logs);
  }

  auto size = gfx::Size(image.width(), image.height());
  if (lens::ShouldDownscaleSize(
          size, lens::features::GetLensOverlayImageMaxArea(),
          lens::features::GetLensOverlayImageMaxWidth(),
          lens::features::GetLensOverlayImageMaxHeight())) {
    return DownscaleImage(image, lens::features::GetLensOverlayImageMaxWidth(),
                          lens::features::GetLensOverlayImageMaxHeight(),
                          client_logs);
  }
  // No downscaling needed.
  return image;
}

SkBitmap CropAndDownscaleImageIfNeeded(
    const SkBitmap& image,
    gfx::Rect region,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  SkBitmap output;
  auto full_image_size = gfx::Size(image.width(), image.height());
  auto region_size = gfx::Size(region.width(), region.height());
  auto target_width = lens::features::GetLensOverlayImageMaxWidth();
  auto target_height = lens::features::GetLensOverlayImageMaxHeight();
  if (lens::ShouldDownscaleSize(region_size,
                                lens::features::GetLensOverlayImageMaxArea(),
                                target_width, target_height)) {
    double scale =
        lens::GetPreferredScale(region_size, target_width, target_height);
    auto downscaled_region_size =
        lens::GetPreferredSize(region_size, target_width, target_height);
    int scaled_full_image_width =
        std::max<int>(scale * full_image_size.width(), 1);
    int scaled_full_image_height =
        std::max<int>(scale * full_image_size.height(), 1);
    int scaled_x = int(scale * region.x());
    int scaled_y = int(scale * region.y());

    SkIRect dest_subset = {scaled_x, scaled_y,
                           scaled_x + downscaled_region_size.width(),
                           scaled_y + downscaled_region_size.height()};
    output = skia::ImageOperations::Resize(
        image, skia::ImageOperations::RESIZE_BEST, scaled_full_image_width,
        scaled_full_image_height, dest_subset);
  } else {
    SkIRect dest_subset = {region.x(), region.y(), region.x() + region.width(),
                           region.y() + region.height()};
    output = skia::ImageOperations::Resize(
        image, skia::ImageOperations::RESIZE_BEST, image.width(),
        image.height(), dest_subset);
  }

  // Since we are cropping the image from a screenshot, we are assuming there
  // cannot be transparent pixels. This allows encoding logic to choose the
  // correct image format to represent the crop.
  output.setAlphaType(kOpaque_SkAlphaType);
  lens::AddClientLogsForDownscale(client_logs, image, output);
  return output;
}

gfx::Rect GetRectForRegion(const SkBitmap& image,
                           const lens::mojom::CenterRotatedBoxPtr& region) {
  bool use_normalized_coordinates =
      region->coordinate_type ==
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  double x_scale = use_normalized_coordinates ? image.width() : 1;
  double y_scale = use_normalized_coordinates ? image.height() : 1;
  return gfx::Rect(
      base::ClampFloor((region->box.x() - 0.5 * region->box.width()) * x_scale),
      base::ClampFloor((region->box.y() - 0.5 * region->box.height()) *
                       y_scale),
      std::max(1, base::ClampFloor(region->box.width() * x_scale)),
      std::max(1, base::ClampFloor(region->box.height() * y_scale)));
}

}  // namespace

namespace lens {

lens::ImageData DownscaleAndEncodeBitmap(
    const SkBitmap& image,
    int ui_scale_factor,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  lens::ImageData image_data;
  scoped_refptr<base::RefCountedBytes> data =
      base::MakeRefCounted<base::RefCountedBytes>();

  auto resized_bitmap =
      DownscaleImageIfNeeded(image, ui_scale_factor, client_logs);
  if (EncodeImage(resized_bitmap,
                  lens::features::GetLensOverlayImageCompressionQuality(), data,
                  client_logs)) {
    image_data.mutable_image_metadata()->set_height(resized_bitmap.height());
    image_data.mutable_image_metadata()->set_width(resized_bitmap.width());

    image_data.mutable_payload()->mutable_image_bytes()->assign(data->begin(),
                                                                data->end());
  }
  return image_data;
}

void AddSignificantRegions(
    lens::ImageData& image_data,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes) {
  for (auto& bounding_box : significant_region_boxes) {
    auto* region = image_data.add_significant_regions();
    auto box = bounding_box->box;
    region->mutable_bounding_box()->set_center_x(box.x());
    region->mutable_bounding_box()->set_center_y(box.y());
    region->mutable_bounding_box()->set_width(box.width());
    region->mutable_bounding_box()->set_height(box.height());
    region->mutable_bounding_box()->set_coordinate_type(
        lens::CoordinateType::NORMALIZED);
  }
}

SkBitmap CropBitmapToRegion(const SkBitmap& image,
                            lens::mojom::CenterRotatedBoxPtr region) {
  gfx::Rect region_rect = GetRectForRegion(image, region);
  return SkBitmapOperations::CreateTiledBitmap(
      image, region_rect.x(), region_rect.y(), region_rect.width(),
      region_rect.height());
}

std::optional<lens::ImageCropAndBitmap> DownscaleAndEncodeBitmapRegionIfNeeded(
    const SkBitmap& image,
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<SkBitmap> region_bytes,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  if (!region) {
    return std::nullopt;
  }

  gfx::Rect region_rect = GetRectForRegion(image, region);

  lens::ImageCropAndBitmap image_crop_and_bitmap;
  scoped_refptr<base::RefCountedBytes> data =
      base::MakeRefCounted<base::RefCountedBytes>();
  if (region_bytes.has_value()) {
    image_crop_and_bitmap.region_bitmap = DownscaleImageIfNeeded(*region_bytes, /*ui_scale_factor=*/0,
                                           client_logs);
  } else {
    image_crop_and_bitmap.region_bitmap =
        CropAndDownscaleImageIfNeeded(image, region_rect, client_logs);
  }

  const auto& region_bitmap = image_crop_and_bitmap.region_bitmap;
  auto& image_crop = image_crop_and_bitmap.image_crop;

  // Populate the zoomed_crop spatial context independently of byte encoding
  // success. This ensures region geometry is preserved for the backend even if
  // compression fails. Valid dimension boundaries are enforced to prevent
  // division by zero.
  if (region_rect.width() > 0 && image.width() > 0 && image.height() > 0) {
    auto* mutable_zoomed_crop = image_crop.mutable_zoomed_crop();
    mutable_zoomed_crop->set_parent_height(image.height());
    mutable_zoomed_crop->set_parent_width(image.width());
    double scale = static_cast<double>(region_bitmap.width()) /
                   static_cast<double>(region_rect.width());
    mutable_zoomed_crop->set_zoom(scale);
    mutable_zoomed_crop->mutable_crop()->set_center_x(
        static_cast<double>(region_rect.CenterPoint().x()) /
        static_cast<double>(image.width()));
    mutable_zoomed_crop->mutable_crop()->set_center_y(
        static_cast<double>(region_rect.CenterPoint().y()) /
        static_cast<double>(image.height()));
    mutable_zoomed_crop->mutable_crop()->set_width(
        static_cast<double>(region_rect.width()) /
        static_cast<double>(image.width()));
    mutable_zoomed_crop->mutable_crop()->set_height(
        static_cast<double>(region_rect.height()) /
        static_cast<double>(image.height()));
    mutable_zoomed_crop->mutable_crop()->set_coordinate_type(
        lens::CoordinateType::NORMALIZED);
  }

  if (EncodeImageMaybeWithTransparency(
          region_bitmap,
          lens::features::GetLensOverlayImageCompressionQuality(), data,
          client_logs)) {
    image_crop.mutable_image()->mutable_image_content()->assign(data->begin(),
                                                                data->end());
  }
  return std::move(image_crop_and_bitmap);
}

lens::mojom::CenterRotatedBoxPtr GetCenterRotatedBoxFromTabViewAndImageBounds(
    const gfx::Rect& tab_bounds,
    const gfx::Rect& view_bounds,
    gfx::Rect image_bounds) {
  // Image bounds are relative to view bounds, so create a copy of the view
  // bounds with the offset removed. Use this to clip the image bounds.
  auto view_bounds_for_clipping = gfx::Rect(view_bounds.size());
  image_bounds.Intersect(view_bounds_for_clipping);

  float left =
      static_cast<float>(view_bounds.x() + image_bounds.x() - tab_bounds.x()) /
      tab_bounds.width();
  float right = static_cast<float>(view_bounds.x() + image_bounds.x() +
                                   image_bounds.width() - tab_bounds.x()) /
                tab_bounds.width();
  float top =
      static_cast<float>(view_bounds.y() + image_bounds.y() - tab_bounds.y()) /
      tab_bounds.height();
  float bottom = static_cast<float>(view_bounds.y() + image_bounds.y() +
                                    image_bounds.height() - tab_bounds.y()) /
                 tab_bounds.height();

  // Clip to remain inside tab bounds.
  if (left < 0) {
    left = 0;
  }
  if (right > 1) {
    right = 1;
  }
  if (top < 0) {
    top = 0;
  }
  if (bottom > 1) {
    bottom = 1;
  }

  float width = right - left;
  float height = bottom - top;
  float x = (left + right) / 2;
  float y = (top + bottom) / 2;

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(x, y, width, height);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  return region;
}

}  // namespace lens
