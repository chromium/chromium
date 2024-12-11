// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_IMAGE_HELPER_H_
#define CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_IMAGE_HELPER_H_

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/ui/lens/ref_counted_lens_overlay_client_logs.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"

namespace gfx {
class Rect;
}  // namespace gfx

class SkBitmap;

// Encodes the SkBitmap into JPEG placing the bytes into `output`. Returns false
// if encoding fails. Outputs image processing data to the client logs.
bool EncodeImage(
    const SkBitmap& image,
    int compression_quality,
    scoped_refptr<base::RefCountedBytes> output,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

// Downscales and encodes the provided bitmap and then stores it in a
// lens::ImageData object. Returns an empty object if encoding fails.
// Downscaling only occurs if the bitmap dimensions mixed with the
// `ui_scale_factor` exceed configured flag values. `ui_scale_factor` will be
// ignored if tiered downscaling is disabled. Outputs image processing
// data to the client logs.
lens::ImageData DownscaleAndEncodeBitmap(
    const SkBitmap& image,
    int ui_scale_factor,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

// Adds the significant regions to the lens::ImageData object.
void AddSignificantRegions(
    lens::ImageData& image_data,
    const std::vector<lens::CenterRotatedBox>& significant_region_boxes);

// Crops the given bitmap to the given region.
SkBitmap CropBitmapToRegion(const SkBitmap& image,
                            lens::CenterRotatedBox region);

// Downscales and encodes the provided bitmap region and then stores it in a
// lens::ImageCrop object if needed. Returns a nullopt if the region is not
// set. Downscaling only occurs if the region dimensions exceed configured
// flag values. Providing region_bytes will use those bytes instead of cropping
// the region from the full page bytes. Outputs image processing data to the
// client logs.
std::optional<lens::ImageCrop> DownscaleAndEncodeBitmapRegionIfNeeded(
    const SkBitmap& image,
    lens::CenterRotatedBox region,
    std::optional<SkBitmap> region_bytes,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

// Returns a normalized bounding box from the given tab, view, and image
// bounds, clipping if the image bounds go outside the tab or view bounds.
lens::CenterRotatedBox GetCenterRotatedBoxFromTabViewAndImageBounds(
    const gfx::Rect& tab_bounds,
    const gfx::Rect& view_bounds,
    gfx::Rect image_bounds);

#endif  // CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_IMAGE_HELPER_H_
