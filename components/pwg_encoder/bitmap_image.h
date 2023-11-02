// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PWG_ENCODER_BITMAP_IMAGE_H_
#define COMPONENTS_PWG_ENCODER_BITMAP_IMAGE_H_

#include <stdint.h>

#include <memory>

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace pwg_encoder {

class BitmapImage {
 public:
  enum Colorspace {
    // These are the only types PWGEncoder currently supports.
    RGBA,
    BGRA
  };

  BitmapImage(const gfx::Size& size, Colorspace colorspace);

  BitmapImage(const BitmapImage&) = delete;
  BitmapImage& operator=(const BitmapImage&) = delete;

  ~BitmapImage();

  uint8_t channels() const;
  const gfx::Size& size() const { return size_; }
  Colorspace colorspace() const { return colorspace_; }

  const uint8_t* pixel_data() const { return data_.get(); }
  uint8_t* pixel_data() { return data_.get(); }

  const uint8_t* GetPixel(const gfx::Point& point) const;

 private:
  gfx::Size size_;
  Colorspace colorspace_;
  std::unique_ptr<uint8_t[]> data_;
};

}  // namespace pwg_encoder

#endif  // COMPONENTS_PWG_ENCODER_BITMAP_IMAGE_H_
