// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_IMAGE_HELPER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_IMAGE_HELPER_H_

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"

class SkBitmap;

namespace lens {

// Encodes the SkBitmap into JPEG placing the bytes into |output|. Returns false
// if encoding fails.
bool EncodeImage(const SkBitmap& image,
                 int compression_quality,
                 scoped_refptr<base::RefCountedBytes>* output);

// Downscales and encodes the provided bitmap and then stores it in a
// lens::ImageData object. Returns an empty object if encoding fails.
// Downscaling only occurs if the bitmap dimensions exceed configured flag
// values.
lens::ImageData DownscaleAndEncodeBitmap(const SkBitmap& image);

// Downscales and encodes the provided bitmap region and then stores it in a
// lens::ImageCrop object if needed. Returns a nullopt if the region is not
// set. Downscaling only occurs if the region dimensions exceed configured
// flag values.
std::optional<lens::ImageCrop> DownscaleAndEncodeBitmapRegionIfNeeded(
    const SkBitmap& image,
    lens::mojom::CenterRotatedBoxPtr region);
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_IMAGE_HELPER_H_
