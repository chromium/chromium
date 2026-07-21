// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PWG_ENCODER_BITMAP_IMAGE_H_
#define COMPONENTS_PWG_ENCODER_BITMAP_IMAGE_H_

#include <stdint.h>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace pwg_encoder {

class BitmapImage {
 public:
  explicit BitmapImage(const gfx::Size& size);

  BitmapImage(const BitmapImage&) = delete;
  BitmapImage& operator=(const BitmapImage&) = delete;

  ~BitmapImage();

  static constexpr uint8_t channels() { return 4u; }
  const gfx::Size& size() const { return size_; }

  base::span<uint32_t> pixels();

  base::span<const uint32_t> GetRow(size_t row, bool flip_y) const;

 private:
  gfx::Size size_;
  base::HeapArray<uint32_t> data_;
};

}  // namespace pwg_encoder

#endif  // COMPONENTS_PWG_ENCODER_BITMAP_IMAGE_H_
