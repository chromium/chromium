// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/pdf/pdf_thumbnailer.h"

#include <string.h>
#include <utility>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "pdf/pdf.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace pdf {

namespace {

// Whether or not to rotate PDF to fit the given size; always no.
constexpr bool kAutorotate = false;

// Whether to create color thumbnails. Always yes.
constexpr bool kUseColor = true;

}  // namespace

PdfThumbnailer::PdfThumbnailer() = default;

PdfThumbnailer::~PdfThumbnailer() = default;

void PdfThumbnailer::GetThumbnail(pdf::mojom::ThumbParamsPtr params,
                                  base::ReadOnlySharedMemoryRegion pdf_region,
                                  GetThumbnailCallback callback) {
  // Vet the requested thumbnail size.
  int width_px = params->size_px.width();
  int height_px = params->size_px.height();
  if (params->size_px.IsEmpty() ||
      width_px > pdf::mojom::PdfThumbnailer::kMaxWidthPixels ||
      height_px > pdf::mojom::PdfThumbnailer::kMaxHeightPixels) {
    DLOG(ERROR) << "Invalid thumbnail size " << width_px << " x " << height_px;
    std::move(callback).Run(SkBitmap());
    return;
  }

  // Decode memory region as PDF bytes.
  base::ReadOnlySharedMemoryMapping pdf_map = pdf_region.Map();
  if (!pdf_map.IsValid()) {
    DLOG(ERROR) << "Failed to decode memory map for PDF thumbnail";
    std::move(callback).Run(SkBitmap());
    return;
  }
  auto pdf_buffer = pdf_map.GetMemoryAsSpan<const uint8_t>();

  // Allocate bitmap to which we render the thumbnail.
  SkBitmap result;
  const SkImageInfo info = SkImageInfo::Make(
      width_px, height_px, kBGRA_8888_SkColorType, kOpaque_SkAlphaType);
  if (!result.tryAllocPixels(info, info.minRowBytes())) {
    DLOG(ERROR) << "Failed to allocate bitmap pixels";
    std::move(callback).Run(SkBitmap());
    return;
  }

  // Convert PDF bytes into a bitmap thumbnail.
  chrome_pdf::RenderOptions options = {
      .stretch_to_bounds = params->stretch,
      .keep_aspect_ratio = params->keep_aspect,
      .autorotate = kAutorotate,
      .use_color = kUseColor,
      .render_device_type = chrome_pdf::RenderDeviceType::kDisplay,
  };
  if (!chrome_pdf::RenderPDFPageToBitmap(pdf_buffer, 0, result.getPixels(),
                                         params->size_px, params->dpi,
                                         options)) {
    DLOG(ERROR) << "Failed to render PDF buffer as bitmap image";
    std::move(callback).Run(SkBitmap());
    return;
  }

  DCHECK_EQ(width_px, result.width());
  DCHECK_EQ(height_px, result.height());
  std::move(callback).Run(result);
}

void PdfThumbnailer::SetUseSkiaRendererPolicy(bool use_skia) {
  chrome_pdf::SetUseSkiaRendererPolicy(use_skia);
}

}  // namespace pdf
