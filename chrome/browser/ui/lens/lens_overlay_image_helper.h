// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_IMAGE_HELPER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_IMAGE_HELPER_H_

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/ui/lens/ref_counted_lens_overlay_client_logs.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/skia/include/core/SkColor.h"

class SkBitmap;

namespace lens {

// Encodes the SkBitmap into JPEG placing the bytes into |output|. Returns false
// if encoding fails. Outputs image processing data to the client logs.
bool EncodeImage(
    const SkBitmap& image,
    int compression_quality,
    scoped_refptr<base::RefCountedBytes> output,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

// Downscales and encodes the provided bitmap and then stores it in a
// lens::ImageData object. Returns an empty object if encoding fails.
// Downscaling only occurs if the bitmap dimensions mixed with the
// ui_scale_factor exceed configured flag values. ui_scale_factor will be
// ignored if tiered downscaling is disabled. Outputs image processing
// data to the client logs.
lens::ImageData DownscaleAndEncodeBitmap(
    const SkBitmap& image,
    int ui_scale_factor,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

// Adds the significant regions to the lens::ImageData object.
void AddSignificantRegions(
    lens::ImageData& image_data,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes);

// Crops the given bitmap to the given region.
SkBitmap CropBitmapToRegion(const SkBitmap& image,
                            lens::mojom::CenterRotatedBoxPtr region);

// Downscales and encodes the provided bitmap region and then stores it in a
// lens::ImageCrop object if needed. Returns a nullopt if the region is not
// set. Downscaling only occurs if the region dimensions exceed configured
// flag values. Providing region_bytes will use those bytes instead of cropping
// the region from the full page bytes. Outputs image processing data to the
// client logs.
std::optional<lens::ImageCrop> DownscaleAndEncodeBitmapRegionIfNeeded(
    const SkBitmap& image,
    lens::mojom::CenterRotatedBoxPtr region,
    std::optional<SkBitmap> region_bytes,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

// Returns a normalized bounding box from the given tab, view, and image
// bounds, clipping if the image bounds go outside the tab or view bounds.
lens::mojom::CenterRotatedBoxPtr GetCenterRotatedBoxFromTabViewAndImageBounds(
    const gfx::Rect& tab_bounds,
    const gfx::Rect& view_bounds,
    gfx::Rect image_bounds);

// Returns the extracted color from the given image. If the extraction fails,
// the returned color is transparent. The returned color should be
// representative of min_population_pct [0.0, 1.0] of the total number of pixels
// in the image.
SkColor ExtractVibrantOrDominantColorFromImage(const SkBitmap& image,
                                               float min_population_pct);

// Returns the hue angle of the given color in LAB space, in radians [-π, π],
// or null_opt if achromatic.
std::optional<float> CalculateHueAngle(
    const std::tuple<float, float, float>& lab_color);

// Returns the chroma distance (from 0) of the given color in LAB space.
float CalculateChroma(const std::tuple<float, float, float>& lab_color);

// Returns the hue angle distance of the 2 given colors [0, 2π).
std::optional<float> CalculateHueAngleDistance(
    const std::tuple<float, float, float>& lab_color1,
    const std::tuple<float, float, float>& lab_color2);

// Returns std::tuple<int> of given color in CIELAB color space.
std::tuple<float, float, float> ConvertColorToLab(SkColor color);

// Returns the best matched color theme item in the given
// map of candidates, matching on the closest in hue angle distance to the
// given seed color. If seed_color is invalid, or does not have
// enough chroma, given by min_chroma, or no good matches can be found,
// returns SK_ColorTransparent.
SkColor FindBestMatchedColorOrTransparent(
    const std::vector<SkColor>& candidate_colors,
    SkColor seed_color,
    float min_chroma);
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_IMAGE_HELPER_H_
