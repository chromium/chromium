// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_RGB_VIDEO_FRAME_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_RGB_VIDEO_FRAME_H_

#include <cstdint>
#include <memory>

#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace recording {

// Defines a type for conveniently accessing the color components of the pixels
// as they're natively stored in a `kN32_SkColorType`-color-type bitmap, and
// comparing instances while ignoring the alpha component.
struct RgbColor {
  RgbColor() = default;
  RgbColor(uint8_t red, uint8_t green, uint8_t blue)
#if SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
      : b(blue),
        g(green),
        r(red)
#elif SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
      : r(red),
        g(green),
        b(blue)
#else
#error "The color format must be either BGRA or RGBA"
#endif
  {
  }

  bool operator==(const RgbColor& rhs) const {
    return r == rhs.r && g == rhs.g && b == rhs.b;
  }

  // The order of the color components depends on the platform.
  // The alpha component is ignored and never used as it's not important for
  // GIF recording.
#if SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
  uint8_t b;
  uint8_t g;
  uint8_t r;
  uint8_t ignored_a;
#elif SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t ignored_a;
#else
#error "The color format must be either BGRA or RGBA"
#endif
};

static_assert(sizeof(RgbColor) == sizeof(SkPMColor));

// Defines a video frame that is composed of as many `RgbColor`s as there are
// number of pixels in the frame represented by the given `bitmap`. The color
// type of the given `bitmap` must be `kN32_SkColorType`, which is either
// `kBGRA_8888_SkColorType` or `kRGBA_8888_SkColorType` depending on the
// platform (see above `RgbColor` members order).
class RgbVideoFrame {
 public:
  explicit RgbVideoFrame(const media::VideoFrame& video_frame);
  RgbVideoFrame(const SkBitmap& bitmap, base::TimeTicks frame_time);
  RgbVideoFrame(RgbVideoFrame&&);
  RgbVideoFrame& operator=(const RgbVideoFrame&) = delete;
  ~RgbVideoFrame();

  int width() const { return width_; }
  int height() const { return height_; }
  base::TimeTicks frame_time() const { return frame_time_; }

  size_t num_pixels() const { return width_ * height_; }

  // Returns the color of the pixel at `row` and `column`. The non-const version
  // can be used to change the color of the pixel.
  RgbColor& pixel_color(int row, int column) {
    return data_[row * width_ + column];
  }
  const RgbColor& pixel_color(int row, int column) const {
    return const_cast<RgbVideoFrame*>(this)->pixel_color(row, column);
  }

  RgbVideoFrame Clone() const;

 private:
  // Copy constructor made private so as not to be used implicitly. `Clone()`
  // above should be used explicitly if needed.
  RgbVideoFrame(const RgbVideoFrame& other);

  // The width and height of the video frame.
  const int width_;
  const int height_;

  const base::TimeTicks frame_time_;

  // The pixel color data.
  std::unique_ptr<RgbColor[]> data_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_RGB_VIDEO_FRAME_H_
