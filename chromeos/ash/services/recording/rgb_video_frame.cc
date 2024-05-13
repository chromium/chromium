// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/rgb_video_frame.h"

#include <cstring>

#include "base/check_op.h"
#include "third_party/skia/include/core/SkColorType.h"

namespace recording {

namespace {

// Wraps the given `video_frame` pixels in a bitmap and returns it. Note that
// this does not copy the pixels bytes from `video_frame` to the returned
// bitmap, and hence the bitmap is safe to access as long as the `video_frame`
// is alive.
SkBitmap WrapVideoFrameInBitmap(const media::VideoFrame& video_frame) {
  const gfx::Size visible_size = video_frame.visible_rect().size();
  const SkImageInfo image_info = SkImageInfo::MakeN32(
      visible_size.width(), visible_size.height(), kPremul_SkAlphaType,
      video_frame.ColorSpace().ToSkColorSpace());

  SkBitmap bitmap;
  const uint8_t* pixels =
      video_frame.visible_data(media::VideoFrame::Plane::kARGB);
  bitmap.installPixels(
      SkPixmap(image_info, pixels,
               video_frame.row_bytes(media::VideoFrame::Plane::kARGB)));
  return bitmap;
}

}  // namespace

RgbVideoFrame::RgbVideoFrame(const media::VideoFrame& video_frame)
    : RgbVideoFrame(WrapVideoFrameInBitmap(video_frame),
                    video_frame.metadata().reference_time.value_or(
                        base::TimeTicks::Now())) {}

RgbVideoFrame::RgbVideoFrame(const SkBitmap& bitmap, base::TimeTicks frame_time)
    : width_(bitmap.width()),
      height_(bitmap.height()),
      frame_time_(frame_time),
      data_(new RgbColor[width_ * height_]) {
  DCHECK_EQ(kN32_SkColorType, bitmap.colorType());

  const size_t bytes_per_pixel = bitmap.bytesPerPixel();
  DCHECK_EQ(bytes_per_pixel, sizeof(RgbColor));

  // Some bitmaps may contain padding pixels at the end of each row. In this
  // case the returned value of `rowBytesAsPixels()` will be larger than
  // `width_`. If there are no padding pixels, then we can copy the whole buffer
  // in a 1-shot `memcpy()` call.
  if (width_ == bitmap.rowBytesAsPixels()) {
    const size_t num_bytes = num_pixels() * bytes_per_pixel;
    std::memcpy(&data_[0], bitmap.getPixels(), num_bytes);
    return;
  }

  // Otherwise, we have to copy it row by row.
  const size_t num_pixels_per_row = width_;
  const size_t bytes_per_row = num_pixels_per_row * bytes_per_pixel;

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
