// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/rgb_video_frame.h"

#include <cstring>

#include "base/check_op.h"
#include "third_party/skia/include/core/SkColorType.h"

namespace recording {

RgbVideoFrame::RgbVideoFrame(const SkBitmap& bitmap)
    : width_(bitmap.width()),
      height_(bitmap.height()),
      data_(new RgbColor[width_ * height_]) {
  DCHECK_EQ(kN32_SkColorType, bitmap.colorType());

  // Note that we don't use `bitmap.rowBytesAsPixels()` or `bitmap.rowBytes()`
  // since the values returned from these can contain padding at the end of each
  // row. We're only interested in the real pixel data.
  const size_t num_pixels_per_row = width_;
  const size_t bytes_per_pixel = bitmap.bytesPerPixel();
  const size_t bytes_per_row = num_pixels_per_row * bytes_per_pixel;

  DCHECK_EQ(bytes_per_pixel, sizeof(RgbColor));
  DCHECK_EQ(num_pixels() * sizeof(RgbColor), bytes_per_row * height_);
  DCHECK_EQ(width_ * sizeof(RgbColor), bytes_per_row);

  for (int row = 0; row < height_; ++row) {
    std::memcpy(&data_[row * width_], bitmap.getAddr(0, row), bytes_per_row);
  }
}

RgbVideoFrame::RgbVideoFrame(RgbVideoFrame&&) = default;

RgbVideoFrame::~RgbVideoFrame() = default;

RgbVideoFrame RgbVideoFrame::Clone() const {
  return RgbVideoFrame(*this);
}

RgbVideoFrame::RgbVideoFrame(const RgbVideoFrame& other)
    : width_(other.width_),
      height_(other.height_),
      data_(new RgbColor[width_ * height_]) {
  std::memcpy(&data_[0], &other.data_[0], num_pixels() * sizeof(RgbColor));
}

}  // namespace recording
