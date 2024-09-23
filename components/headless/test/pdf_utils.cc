// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/pdf_utils.h"

#include <optional>

#include "base/logging.h"
#include "components/headless/test/bitmap_utils.h"
#include "pdf/pdf.h"
#include "printing/units.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace headless {

PDFPageBitmap::PDFPageBitmap() = default;
PDFPageBitmap::~PDFPageBitmap() = default;

bool PDFPageBitmap::Render(base::span<const uint8_t> pdf_data, int page_index) {
  std::optional<gfx::SizeF> page_size_in_points =
      chrome_pdf::GetPDFPageSizeByIndex(pdf_data, page_index);
  if (!page_size_in_points) {
    return false;
  }

  gfx::SizeF page_size_in_pixels =
      gfx::ScaleSize(page_size_in_points.value(),
                     static_cast<float>(kDpi) / printing::kPointsPerInch);

  gfx::Rect page_rect(gfx::ToCeiledSize(page_size_in_pixels));

  constexpr chrome_pdf::RenderOptions options = {
      .stretch_to_bounds = false,
      .keep_aspect_ratio = true,
      .autorotate = true,
      .use_color = true,
      .render_device_type = chrome_pdf::RenderDeviceType::kPrinter,
  };

  bitmap_size_ = page_rect.size();
  bitmap_data_.resize(kColorChannels * bitmap_size_.GetArea());
  return chrome_pdf::RenderPDFPageToBitmap(pdf_data, page_index,
                                           bitmap_data_.data(), bitmap_size_,
                                           gfx::Size(kDpi, kDpi), options);
}

uint32_t PDFPageBitmap::GetPixelRGB(const gfx::Point& pt) const {
  return GetPixelRGB(pt.x(), pt.y());
}

uint32_t PDFPageBitmap::GetPixelRGB(int x, int y) const {
  CHECK_LT(x, bitmap_size_.width());
  CHECK_LT(y, bitmap_size_.height());

  int pixel_index =
      bitmap_size_.width() * y * kColorChannels + x * kColorChannels;
  return bitmap_data_[pixel_index + 0]             // B
         | (bitmap_data_[pixel_index + 1] << 8)    // G
         | (bitmap_data_[pixel_index + 2] << 16);  // R
}

bool PDFPageBitmap::CheckColoredRect(SkColor rect_color,
                                     SkColor bkgr_color,
                                     int margins) {
  SkBitmap bitmap;
  SkImageInfo info = SkImageInfo::MakeN32(
      bitmap_size_.width(), bitmap_size_.height(), kOpaque_SkAlphaType);
  size_t rowBytes = bitmap_size_.width() * kColorChannels;
  CHECK(bitmap.installPixels(info, bitmap_data_.data(), rowBytes));

  return headless::CheckColoredRect(bitmap, rect_color, bkgr_color, margins);
}

}  // namespace headless
