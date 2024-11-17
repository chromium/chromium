// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/pwg_encoder/bitmap_image.h"

#include "base/check_op.h"

namespace pwg_encoder {

namespace {
const uint8_t kCurrentlySupportedNumberOfChannels = 4;
}

BitmapImage::BitmapImage(const gfx::Size& size, Colorspace colorspace)
    : size_(size),
      colorspace_(colorspace),
      data_(new uint8_t[size.GetArea() * channels()]) {}

BitmapImage::~BitmapImage() = default;

uint8_t BitmapImage::channels() const {
  return kCurrentlySupportedNumberOfChannels;
}

const uint8_t* BitmapImage::GetPixel(const gfx::Point& point) const {
  DCHECK_LT(point.x(), size_.width());
  DCHECK_LT(point.y(), size_.height());
  return data_.get() + (point.y() * size_.width() + point.x()) * channels();
}

}  // namespace pwg_encoder
