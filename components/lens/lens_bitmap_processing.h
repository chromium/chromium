// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_BITMAP_PROCESSING_H_
#define COMPONENTS_LENS_LENS_BITMAP_PROCESSING_H_

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/lens/ref_counted_lens_overlay_client_logs.h"
#include "ui/gfx/geometry/size.h"

#if !BUILDFLAG(IS_IOS)
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // !BUILDFLAG(IS_IOS)

namespace lens {

class ImageData;

// Image encoding options for an uploaded image.
struct ImageEncodingOptions {
  bool enable_webp_encoding;
  int max_size;
  int max_height;
  int max_width;
  int compression_quality;
};

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

// Outputs image processing data to the client logs for the encode phase,
// including the encoded image size.
void AddClientLogsForEncode(
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    scoped_refptr<base::RefCountedBytes> output_bytes);

// Outputs image processing data to the client logs for the downscale phase,
// including the original and downscaled image sizes.
void AddClientLogsForDownscale(
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    const gfx::Size& original_pixel_size,
    const gfx::Size& downscaled_pixel_size);

#if !BUILDFLAG(IS_IOS)

// Outputs image processing data to the client logs for the downscale phase,
// including the original and downscaled image sizes.
void AddClientLogsForDownscale(
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    const SkBitmap& original_image,
    const SkBitmap& downscaled_image);

// Downscales the image to the target width and height. Outputs image processing
// data to the client logs.
SkBitmap DownscaleImage(
    const SkBitmap& image,
    int target_width,
    int target_height,
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs);

// Encodes the image using JPEG. Outputs image processing data to the client
// logs.
bool EncodeImage(const SkBitmap& image,
                 int compression_quality,
                 scoped_refptr<base::RefCountedBytes> output,
                 scoped_refptr<RefCountedLensOverlayClientLogs> client_logs);

// Encodes the image using JPEG if it is opaque, otherwise uses WebP. Outputs
// image processing data to the client logs.
bool EncodeImageMaybeWithTransparency(
    const SkBitmap& image,
    int compression_quality,
    scoped_refptr<base::RefCountedBytes> output,
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs);

// Downscales and returns the provided bitmap if the bitmap dimensions exceed
// configured flag values.
SkBitmap DownscaleBitmap(
    const SkBitmap& image,
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    const ImageEncodingOptions& image_options);

// Downscales and encodes the provided bitmap and then stores it in a
// ImageData object. Returns an empty object if encoding fails.
// Downscaling only occurs if the bitmap dimensions exceed configured flag
// values.
ImageData DownscaleAndEncodeBitmap(
    const SkBitmap& image,
    scoped_refptr<RefCountedLensOverlayClientLogs> client_logs,
    const ImageEncodingOptions& image_options);

#endif  // !BUILDFLAG(IS_IOS)

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_BITMAP_PROCESSING_H_
