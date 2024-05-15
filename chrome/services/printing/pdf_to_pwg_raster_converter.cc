// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/pdf_to_pwg_raster_converter.h"

#include <limits>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/pwg_encoder/bitmap_image.h"
#include "components/pwg_encoder/pwg_encoder.h"
#include "pdf/pdf.h"
#include "printing/mojom/print.mojom.h"
#include "printing/pdf_render_settings.h"

namespace printing {

namespace {

base::ReadOnlySharedMemoryRegion RenderPdfPagesToPwgRaster(
    base::ReadOnlySharedMemoryRegion pdf_region,
    const PdfRenderSettings& settings,
    const PwgRasterSettings& bitmap_settings,
    uint32_t* page_count) {
  base::ReadOnlySharedMemoryRegion invalid_pwg_region;
  base::ReadOnlySharedMemoryMapping pdf_mapping = pdf_region.Map();
  if (!pdf_mapping.IsValid())
    return invalid_pwg_region;

  auto pdf_data = pdf_mapping.GetMemoryAsSpan<const uint8_t>();

  // Get the page count and reserve 64 KB per page in |pwg_data| below.
  static constexpr size_t kEstimatedSizePerPage = 64 * 1024;
  static constexpr size_t kMaxPageCount =
      std::numeric_limits<size_t>::max() / kEstimatedSizePerPage;
  int total_page_count = 0;
  if (!chrome_pdf::GetPDFDocInfo(pdf_data, &total_page_count, nullptr) ||
      total_page_count <= 0 ||
      static_cast<size_t>(total_page_count) >= kMaxPageCount) {
    return invalid_pwg_region;
  }

  std::string pwg_data;
  pwg_data.reserve(total_page_count * kEstimatedSizePerPage);
  pwg_data = pwg_encoder::PwgEncoder::GetDocumentHeader();
  pwg_encoder::BitmapImage image(settings.area.size(),
                                 pwg_encoder::BitmapImage::BGRA);
  const chrome_pdf::RenderOptions options = {
      .stretch_to_bounds = false,
      .keep_aspect_ratio = true,
      .autorotate = settings.autorotate,
      .use_color = settings.use_color,
      .render_device_type = chrome_pdf::RenderDeviceType::kPrinter,
  };
  for (int i = 0; i < total_page_count; ++i) {
    int page_number = i;

    if (bitmap_settings.reverse_page_order)
      page_number = total_page_count - 1 - page_number;

    if (!chrome_pdf::RenderPDFPageToBitmap(pdf_data, page_number,
                                           image.pixel_data(), image.size(),
                                           settings.dpi, options)) {
      return invalid_pwg_region;
    }

    pwg_encoder::PwgHeaderInfo header_info;
    header_info.dpi = settings.dpi;
    header_info.total_pages = total_page_count;
    header_info.color_space = bitmap_settings.use_color
                                  ? pwg_encoder::PwgHeaderInfo::SRGB
                                  : pwg_encoder::PwgHeaderInfo::SGRAY;

    switch (bitmap_settings.duplex_mode) {
      case mojom::DuplexMode::kUnknownDuplexMode:
        NOTREACHED_IN_MIGRATION();
        break;
      case mojom::DuplexMode::kSimplex:
        // Already defaults to false/false.
        break;
      case mojom::DuplexMode::kLongEdge:
        header_info.duplex = true;
        break;
      case mojom::DuplexMode::kShortEdge:
        header_info.duplex = true;
        header_info.tumble = true;
        break;
    }

    // Transform odd pages.
    if (page_number % 2) {
      switch (bitmap_settings.odd_page_transform) {
        case TRANSFORM_NORMAL:
          break;
        case TRANSFORM_ROTATE_180:
          header_info.flipx = true;
          header_info.flipy = true;
          break;
        case TRANSFORM_FLIP_HORIZONTAL:
          header_info.flipx = true;
          break;
        case TRANSFORM_FLIP_VERTICAL:
          header_info.flipy = true;
          break;
      }
    }

    if (bitmap_settings.rotate_all_pages) {
      header_info.flipx = !header_info.flipx;
      header_info.flipy = !header_info.flipy;
    }

    std::string pwg_page =
        pwg_encoder::PwgEncoder::EncodePage(image, header_info);
    if (pwg_page.empty())
      return invalid_pwg_region;
    pwg_data += pwg_page;
  }

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(pwg_data.size());
  if (!region_mapping.IsValid())
    return invalid_pwg_region;

  *page_count = total_page_count;
  memcpy(region_mapping.mapping.memory(), pwg_data.data(), pwg_data.size());
  return std::move(region_mapping.region);
}

}  // namespace

PdfToPwgRasterConverter::PdfToPwgRasterConverter() = default;

PdfToPwgRasterConverter::~PdfToPwgRasterConverter() {}

void PdfToPwgRasterConverter::Convert(
    base::ReadOnlySharedMemoryRegion pdf_region,
    const PdfRenderSettings& pdf_settings,
    const PwgRasterSettings& pwg_raster_settings,
    ConvertCallback callback) {
  uint32_t page_count = 0;
  base::ReadOnlySharedMemoryRegion region = RenderPdfPagesToPwgRaster(
      std::move(pdf_region), pdf_settings, pwg_raster_settings, &page_count);
  std::move(callback).Run(std::move(region), page_count);
}

void PdfToPwgRasterConverter::SetUseSkiaRendererPolicy(bool use_skia) {
  chrome_pdf::SetUseSkiaRendererPolicy(use_skia);
}

}  // namespace printing
