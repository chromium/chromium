// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/core_favicon_service.h"

#include "base/containers/contains.h"
#include "components/favicon_base/favicon_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace favicon {

// static
std::vector<int> CoreFaviconService::GetPixelSizesForFaviconScales(
    int size_in_dip) {
  // NOTE: GetFaviconScales() always returns 1x on android.
  std::vector<float> scales = favicon_base::GetFaviconScales();
  std::vector<int> sizes_in_pixel;
  for (float scale : scales)
    sizes_in_pixel.push_back(std::ceil(size_in_dip * scale));
  return sizes_in_pixel;
}

// static
std::vector<SkBitmap> CoreFaviconService::ExtractSkBitmapsToStore(
    const gfx::Image& image) {
  gfx::ImageSkia image_skia = image.AsImageSkia();
  image_skia.EnsureRepsForSupportedScales();
  std::vector<SkBitmap> bitmaps;
  const std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  for (const gfx::ImageSkiaRep& rep : image_skia.image_reps()) {
    // Only save images with a supported sale.
    if (base::Contains(favicon_scales, rep.scale()))
      bitmaps.push_back(rep.GetBitmap());
  }
  return bitmaps;
}

}  // namespace favicon
