// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_TEST_PDF_UTILS_H_
#define COMPONENTS_HEADLESS_TEST_PDF_UTILS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

// Utility class to render PDF page into a bitmap and inspect its pixels.
class PDFPageBitmap {
 public:
  static constexpr int kDpi = 300;
  static constexpr int kColorChannels = 4;

  PDFPageBitmap();
  ~PDFPageBitmap();

  bool Render(base::span<const uint8_t> pdf_data, int page_index);

  uint32_t GetPixelRGB(const gfx::Point& pt) const;
  uint32_t GetPixelRGB(int x, int y) const;

  bool CheckColoredRect(SkColor rect_color, SkColor bkgr_color, int margins);
  bool CheckColoredRect(SkColor rect_color, SkColor bkgr_color) {
    return CheckColoredRect(rect_color, bkgr_color, /*margins=*/0);
  }

  int width() const { return bitmap_size_.width(); }
  int height() const { return bitmap_size_.height(); }
  gfx::Size size() const { return bitmap_size_; }

 private:
  std::vector<uint8_t> bitmap_data_;
  gfx::Size bitmap_size_;
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_TEST_PDF_UTILS_H_
