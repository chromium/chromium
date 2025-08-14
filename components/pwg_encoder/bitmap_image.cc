// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pwg_encoder/bitmap_image.h"

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"

namespace pwg_encoder {

BitmapImage::BitmapImage(const gfx::Size& size, Colorspace colorspace)
    : size_(size),
      colorspace_(colorspace),
      data_(base::HeapArray<uint32_t>::Uninit(size.GetArea())) {}

BitmapImage::~BitmapImage() = default;

uint8_t* BitmapImage::pixel_data() {
  return base::as_writable_bytes(data_.as_span()).data();
}

const uint8_t* BitmapImage::pixel_data() const {
  return base::as_bytes(data_.as_span()).data();
}

base::span<uint32_t> BitmapImage::pixels() {
  return data_;
}

const uint8_t* BitmapImage::GetPixel(const gfx::Point& point) const {
  DCHECK_LT(point.x(), size_.width());
  DCHECK_LT(point.y(), size_.height());
  return UNSAFE_TODO(pixel_data() +
                     (point.y() * size_.width() + point.x()) * channels());
}

static_assert(BitmapImage::channels() * sizeof(uint8_t) == sizeof(uint32_t),
              "Underlying storage type of `BitmapImage` is conceptually tied "
              "to the raster being 4 channels");

}  // namespace pwg_encoder
