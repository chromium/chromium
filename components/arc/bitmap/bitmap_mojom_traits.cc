// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bitmap/bitmap_mojom_traits.h"

namespace mojo {

bool StructTraits<arc::mojom::ArcBitmapDataView, SkBitmap>::Read(
    arc::mojom::ArcBitmapDataView data,
    SkBitmap* out) {
  mojo::ArrayDataView<uint8_t> pixel_data;
  data.GetPixelDataDataView(&pixel_data);

  SkImageInfo info = SkImageInfo::Make(
      data.width(), data.height(), kRGBA_8888_SkColorType, kPremul_SkAlphaType);
  if (info.computeByteSize(info.minRowBytes()) > pixel_data.size()) {
    // Insufficient buffer size.
    return false;
  }

  // Create the SkBitmap object which wraps the arc bitmap pixels. This
  // doesn't copy and |data| and |bitmap| share the buffer.
  SkBitmap bitmap;
  if (!bitmap.installPixels(info, const_cast<uint8_t*>(pixel_data.data()),
                            info.minRowBytes())) {
    // Error in installing pixels.
    return false;
  }

  // Copy the pixels with converting color type.
  SkImageInfo image_info = info.makeColorType(kN32_SkColorType);
  return out->tryAllocPixels(image_info) &&
         bitmap.readPixels(image_info, out->getPixels(), out->rowBytes(), 0, 0);
}

}  // namespace mojo
