// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_BITMAP_PROCESSING_H_
#define COMPONENTS_LENS_LENS_BITMAP_PROCESSING_H_

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace lens {

// Returns true if the area is larger than the max area AND one of the width OR
// height exceeds the configured max values.
bool ShouldDownscaleSize(const gfx::Size& size,
                         int max_area,
                         int max_width,
                         int max_height);

// Returns the preferred scale for the given original size and target width and
// height.
double GetPreferredScale(const gfx::Size& original_size,
                         int target_width,
                         int target_height);

// Returns the preferred size for the given original size and target width and
// height. The preferred size is the original size scaled down to the target
// width and height.
gfx::Size GetPreferredSize(const gfx::Size& original_size,
                           int target_width,
                           int target_height);

// Outputs image processing data to the client logs for the downscale phase,
// including the original and downscaled image sizes.
void AddClientLogsForDownscale(
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
    const SkBitmap& original_image,
    const SkBitmap& downscaled_image);

// Outputs image processing data to the client logs for the encode phase,
// including the encoded image size.
void AddClientLogsForEncode(
  scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs,
  scoped_refptr<base::RefCountedBytes> output_bytes);

// Downscales the image to the target width and height. Outputs image processing
// data to the client logs.
SkBitmap DownscaleImage(
    const SkBitmap& image,
    int target_width,
    int target_height,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

// Encodes the image using JPEG. Outputs image processing data to the client
// logs.
bool EncodeImage(
    const SkBitmap& image,
    int compression_quality,
    scoped_refptr<base::RefCountedBytes> output,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

// Encodes the image using JPEG if it is opaque, otherwise uses WebP. Outputs
// image processing data to the client logs.
bool EncodeImageMaybeWithTransparency(
    const SkBitmap& image,
    int compression_quality,
    scoped_refptr<base::RefCountedBytes> output,
    scoped_refptr<lens::RefCountedLensOverlayClientLogs> client_logs);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_BITMAP_PROCESSING_H_
