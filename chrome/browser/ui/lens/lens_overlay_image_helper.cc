// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"

#include <numbers>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "chrome/browser/ui/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/lens/lens_features.h"
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

bool ShouldDownscaleSize(const gfx::Size& size,
                         int max_area,
                         int max_width,
                         int max_height) {
  // This returns true if the area is larger than the max area AND one of the
  // width OR height exceeds the configured max values.
  return size.GetArea() > max_area &&
         (size.width() > max_width || size.height() > max_height);
}

bool ShouldDownscaleSizeWithUiScaling(const gfx::Size& size,
                                      int max_area,
                                      int max_width,
                                      int max_height,
                                      int ui_scale_factor) {
  if (ui_scale_factor <= 0) {
    return ShouldDownscaleSize(size, max_area, max_width, max_height);
  }
  return ui_scale_factor <
             lens::features::
                 GetLensOverlayImageDownscaleUiScalingFactorThreshold() &&
         ShouldDownscaleSize(size, max_area, max_width, max_height);
}

double GetPreferredScale(const gfx::Size& original_size,
                         int target_width,
                         int target_height) {
  return std::min(
      base::ClampDiv(static_cast<double>(target_width), original_size.width()),
      base::ClampDiv(static_cast<double>(target_height),
                     original_size.height()));
}

gfx::Size GetPreferredSize(const gfx::Size& original_size,
                           int target_width,
                           int target_height) {
  double scale = GetPreferredScale(original_size, target_width, target_height);
  int width = std::clamp<int>(scale * original_size.width(), 1, target_width);
  int height =
      std::clamp<int>(scale * original_size.height(), 1, target_height);
  return gfx::Size(width, height);
}

void AddClientLogsForDownscale(
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
    const SkBitmap& original_image,
    const SkBitmap& downscaled_image) {
  auto* downscale_phase = client_logs->client_logs()
                              .mutable_phase_latencies_metadata()
                              ->add_phase();
  downscale_phase->mutable_image_downscale_data()->set_original_image_size(
      original_image.width() * original_image.height());
  downscale_phase->mutable_image_downscale_data()->set_downscaled_image_size(
      downscaled_image.width() * downscaled_image.height());
}

void AddClientLogsForEncode(
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
    scoped_refptr<base::RefCountedBytes> output_bytes) {
  auto* encode_phase = client_logs->client_logs()
                           .mutable_phase_latencies_metadata()
                           ->add_phase();
  encode_phase->mutable_image_encode_data()->set_encoded_image_size_bytes(
      output_bytes->as_vector().size());
}

SkBitmap DownscaleImage(
    const SkBitmap& image,
    int target_width,
    int target_height,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  auto size = gfx::Size(image.width(), image.height());
  auto preferred_size = GetPreferredSize(size, target_width, target_height);
  SkBitmap downscaled_image = skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, preferred_size.width(),
      preferred_size.height());
  AddClientLogsForDownscale(client_logs, image, downscaled_image);
  return downscaled_image;
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
  } else if (ShouldDownscaleSize(
                 size, lens::features::GetLensOverlayImageMaxAreaTier2(),
                 lens::features::GetLensOverlayImageMaxWidthTier2(),
                 lens::features::GetLensOverlayImageMaxHeightTier2())) {
    return DownscaleImage(image, lens::features::GetLensOverlayImageMaxWidth(),
                          lens::features::GetLensOverlayImageMaxHeight(),
                          client_logs);
    // Tier 1 Downscaling.
  } else if (ShouldDownscaleSize(
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
  if (ShouldDownscaleSize(size, lens::features::GetLensOverlayImageMaxArea(),
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
  if (ShouldDownscaleSize(region_size,
                          lens::features::GetLensOverlayImageMaxArea(),
                          target_width, target_height)) {
    double scale = GetPreferredScale(region_size, target_width, target_height);
    auto downscaled_region_size =
        GetPreferredSize(region_size, target_width, target_height);
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
  AddClientLogsForDownscale(client_logs, image, output);
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

bool EncodeImage(
    const SkBitmap& image,
    int compression_quality,
    scoped_refptr<base::RefCountedBytes> output,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  if (gfx::JPEGCodec::Encode(image, compression_quality,
                             &output->as_vector())) {
    AddClientLogsForEncode(client_logs, output);
    return true;
  }
  return false;
}

bool EncodeImageMaybeWithTransparency(
    const SkBitmap& image,
    int compression_quality,
    scoped_refptr<base::RefCountedBytes> output,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  if (image.isOpaque()) {
    return EncodeImage(image, compression_quality, output, client_logs);
  }
  if (gfx::WebpCodec::Encode(image, compression_quality,
                             &output->as_vector())) {
    AddClientLogsForEncode(client_logs, output);
    return true;
  }
  return false;
}

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

std::optional<lens::ImageCrop> DownscaleAndEncodeBitmapRegionIfNeeded(
    const SkBitmap& image,
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<SkBitmap> region_bytes,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs) {
  if (!region) {
    return std::nullopt;
  }

  gfx::Rect region_rect = GetRectForRegion(image, region);

  lens::ImageCrop image_crop;
  SkBitmap region_bitmap;
  scoped_refptr<base::RefCountedBytes> data =
      base::MakeRefCounted<base::RefCountedBytes>();
  ;
  if (region_bytes.has_value()) {
    region_bitmap = DownscaleImageIfNeeded(*region_bytes, /*ui_scale_factor=*/0,
                                           client_logs);
  } else {
    region_bitmap =
        CropAndDownscaleImageIfNeeded(image, region_rect, client_logs);
  }
  if (EncodeImageMaybeWithTransparency(
          region_bitmap,
          lens::features::GetLensOverlayImageCompressionQuality(), data,
          client_logs)) {
    auto* mutable_zoomed_crop = image_crop.mutable_zoomed_crop();
    mutable_zoomed_crop->set_parent_height(image.height());
    mutable_zoomed_crop->set_parent_width(image.width());
    double scale = static_cast<double>(region_bitmap.width()) /
                   static_cast<double>(region_rect.width());
    mutable_zoomed_crop->set_zoom(scale);
    mutable_zoomed_crop->mutable_crop()->set_center_x(
        region_rect.CenterPoint().x());
    mutable_zoomed_crop->mutable_crop()->set_center_y(
        region_rect.CenterPoint().y());
    mutable_zoomed_crop->mutable_crop()->set_width(region_rect.width());
    mutable_zoomed_crop->mutable_crop()->set_height(region_rect.height());
    mutable_zoomed_crop->mutable_crop()->set_coordinate_type(
        lens::CoordinateType::IMAGE);

    image_crop.mutable_image()->mutable_image_content()->assign(data->begin(),
                                                                data->end());
  }
  return image_crop;
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

SkColor ExtractVibrantOrDominantColorFromImage(const SkBitmap& image,
                                               float min_population_pct) {
  if (image.empty() || image.isNull()) {
    return SK_ColorTRANSPARENT;
  }

  min_population_pct = std::clamp(min_population_pct, 0.0f, 1.0f);

  std::vector<color_utils::ColorProfile> profiles;
  // vibrant color profile
  profiles.emplace_back(color_utils::LumaRange::ANY,
                        color_utils::SaturationRange::VIBRANT);
  // any color profile
  profiles.emplace_back(color_utils::LumaRange::ANY,
                        color_utils::SaturationRange::ANY);

  auto vibrantAndDominantColors = color_utils::CalculateProminentColorsOfBitmap(
      image, profiles, /*region=*/nullptr, color_utils::ColorSwatchFilter());

  for (const auto& swatch : vibrantAndDominantColors) {
    // Valid color. Extraction failure returns 0 alpha channel.
    // Population Threshold.
    if (SkColorGetA(swatch.color) != SK_AlphaTRANSPARENT &&
        static_cast<float>(swatch.population) >=
            static_cast<float>(
                std::min(image.width() * image.height(),
                         color_utils::kMaxConsideredPixelsForSwatches)) *
                min_population_pct) {
      return swatch.color;
    }
  }
  return SK_ColorTRANSPARENT;
}

std::optional<float> CalculateHueAngle(
    const std::tuple<float, float, float>& lab_color) {
  float a = std::get<1>(lab_color);
  float b = std::get<2>(lab_color);
  if (a == 0) {
    return std::nullopt;
  }
  return atan2(b, a);
}

float CalculateChroma(const std::tuple<float, float, float>& lab_color) {
  return hypotf(std::get<1>(lab_color), std::get<2>(lab_color));
}

std::optional<float> CalculateHueAngleDistance(
    const std::tuple<float, float, float>& lab_color1,
    const std::tuple<float, float, float>& lab_color2) {
  auto angle1 = CalculateHueAngle(lab_color1);
  auto angle2 = CalculateHueAngle(lab_color2);
  if (!angle1.has_value() || !angle2.has_value()) {
    return std::nullopt;
  }
  float distance = std::abs(angle1.value() - angle2.value());
  return std::min(distance, (float)(std::numbers::pi * 2.0 - distance));
}

// This conversion goes from legacy int based RGB to sRGB floats to
// XYZD50 to Lab, leveraging gfx conver_conversion functions.
std::tuple<float, float, float> ConvertColorToLab(SkColor color) {
  // Legacy RGB -> float sRGB -> XYZD50 -> LAB.
  auto [r, g, b] = gfx::SRGBLegacyToSRGB((float)SkColorGetR(color),
                                         (float)SkColorGetG(color),
                                         (float)SkColorGetB(color));
  auto [x, y, z] = gfx::SRGBToXYZD50(r, g, b);
  return gfx::XYZD50ToLab(x, y, z);
}

SkColor FindBestMatchedColorOrTransparent(
    const std::vector<SkColor>& candidate_colors,
    SkColor seed_color,
    float min_chroma) {
  if (SkColorGetA(seed_color) == SK_AlphaTRANSPARENT) {
    return SK_ColorTRANSPARENT;
  }
  if (candidate_colors.empty()) {
    return SK_ColorTRANSPARENT;
  }

  const auto& seed_lab = ConvertColorToLab(seed_color);
  // Check seed has enough chroma, calculated as hypot of a & b channels.
  if (CalculateChroma(seed_lab) < min_chroma) {
    return SK_ColorTRANSPARENT;
  }

  auto closest_color = std::min_element(
      candidate_colors.begin(), candidate_colors.end(),
      [&seed_lab](const auto& color1, const auto& color2) -> bool {
        const auto& theme1_lab = ConvertColorToLab(color1);
        const auto& theme2_lab = ConvertColorToLab(color2);
        auto angle1 = CalculateHueAngleDistance(theme1_lab, seed_lab);
        auto angle2 = CalculateHueAngleDistance(theme2_lab, seed_lab);
        return angle1.has_value() && angle2.has_value() &&
               angle1.value() < angle2.value();
      });
  if (closest_color == candidate_colors.end()) {
    return SK_ColorTRANSPARENT;
  }
  return *closest_color;
}
}  // namespace lens
