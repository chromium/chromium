// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pwg_encoder/bitmap_image.h"

#include "base/containers/span.h"

namespace pwg_encoder {

BitmapImage::BitmapImage(const gfx::Size& size, Colorspace colorspace)
    : size_(size),
      colorspace_(colorspace),
      data_(base::HeapArray<uint32_t>::Uninit(size.GetArea())) {}

BitmapImage::~BitmapImage() = default;

base::span<uint32_t> BitmapImage::pixels() {
  return data_;
}

base::span<const uint32_t> BitmapImage::GetRow(size_t row, bool flip_y) const {
  size_t actual_row = flip_y ? size_.height() - 1 - row : row;
  return data_.subspan(size_.width() * actual_row, size_.width());
}

static_assert(BitmapImage::channels() * sizeof(uint8_t) == sizeof(uint32_t),
              "Underlying storage type of `BitmapImage` is conceptually tied "
              "to the raster being 4 channels");

}  // namespace pwg_encoder
