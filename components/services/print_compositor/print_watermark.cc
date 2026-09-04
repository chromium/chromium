// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/print_compositor/print_watermark.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/enterprise/watermarking/mojom/watermark.mojom.h"
#include "components/enterprise/watermarking/watermark.h"
#include "pdf/pdf.h"
#include "pdf/pdf_watermark_overlayer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skia_span_util.h"

namespace printing {

// static
std::unique_ptr<PrintWatermark> PrintWatermark::Create(
    watermark::mojom::WatermarkBlockPtr watermark_block) {
  if (!watermark_block) {
    return nullptr;
  }

  base::ReadOnlySharedMemoryMapping mapping =
      watermark_block->serialized_skpicture.Map();
  if (!mapping.IsValid()) {
    VLOG(1) << "Failed to map shared memory for watermark block received "
               "from browser";
    return nullptr;
  }
  auto skpicture_span = mapping.GetMemoryAsSpan<uint8_t>();
  SkMemoryStream stream(gfx::MakeSkDataFromSpanWithoutCopy(skpicture_span));
  sk_sp<SkPicture> picture = SkPicture::MakeFromStream(&stream);
  if (!picture) {
    VLOG(1)
        << "Failed to deserialize the watermark picture received from browser";
    return nullptr;
  }

  return base::WrapUnique(
      new PrintWatermark(std::move(watermark_block), std::move(picture)));
}

PrintWatermark::PrintWatermark(
    watermark::mojom::WatermarkBlockPtr watermark_block,
    sk_sp<SkPicture> picture)
    : watermark_block_(std::move(watermark_block)),
      cached_picture_(std::move(picture)) {
  CHECK(watermark_block_);
  CHECK(cached_picture_);
}

PrintWatermark::~PrintWatermark() = default;

void PrintWatermark::OnDrawPage(SkCanvas* canvas, const SkSize& size) {
  enterprise_watermark::DrawWatermark(canvas, cached_picture_.get(),
                                      watermark_block_->width,
                                      watermark_block_->height, size);
}

base::ReadOnlySharedMemoryRegion PrintWatermark::OnOverlayPdf(
    base::ReadOnlySharedMemoryRegion pdf_region) {
  // TODO(b/518763216): Add UMA histogram tracking for success/failure.
  return OnOverlayPdfImpl(std::move(pdf_region));
}

base::ReadOnlySharedMemoryRegion PrintWatermark::OnOverlayPdfImpl(
    base::ReadOnlySharedMemoryRegion pdf_region) {
  base::ReadOnlySharedMemoryMapping pdf_mapping = pdf_region.Map();
  if (!pdf_mapping.IsValid()) {
    VLOG(1) << "Failed to map input PDF shared memory region.";
    return base::ReadOnlySharedMemoryRegion();
  }

  // Creates the overlayer, which validates the PDF structure and page count.
  // Returns nullptr if the input PDF is invalid or corrupt.
  std::unique_ptr<chrome_pdf::PdfWatermarkOverlayer> overlayer =
      chrome_pdf::CreatePdfWatermarkOverlayer(
          pdf_mapping.GetMemoryAsSpan<const uint8_t>());
  if (!overlayer) {
    VLOG(1) << "Failed to create PdfWatermarkOverlayer.";
    return base::ReadOnlySharedMemoryRegion();
  }

  std::vector<gfx::SizeF> page_sizes = overlayer->GetPageSizes();
  CHECK(!page_sizes.empty());

  SkDynamicMemoryWStream stream;
  sk_sp<SkDocument> doc = SkPDF::MakeDocument(&stream);

  for (const auto& size : page_sizes) {
    SkCanvas* canvas = doc->beginPage(size.width(), size.height());
    if (!canvas) {
      VLOG(1) << "Failed to begin Skia PDF page.";
      return base::ReadOnlySharedMemoryRegion();
    }
    OnDrawPage(canvas, SkSize::Make(size.width(), size.height()));
    doc->endPage();
  }
  doc->close();

  sk_sp<SkData> overlay_data = stream.detachAsData();
  if (overlay_data->isEmpty()) {
    VLOG(1) << "Failed to generate overlay PDF data.";
    return base::ReadOnlySharedMemoryRegion();
  }

  std::vector<uint8_t> output_pdf =
      overlayer->OverlayPages(gfx::SkDataToSpan(overlay_data));
  if (output_pdf.empty()) {
    VLOG(1) << "Failed to overlay PDF pages.";
    return base::ReadOnlySharedMemoryRegion();
  }

  base::MappedReadOnlyRegion output_region =
      base::ReadOnlySharedMemoryRegion::Create(output_pdf.size());
  if (!output_region.IsValid()) {
    VLOG(1) << "Failed to allocate output shared memory region.";
    return base::ReadOnlySharedMemoryRegion();
  }

  output_region.mapping.GetMemoryAsSpan<uint8_t>()
      .first(output_pdf.size())
      .copy_from(output_pdf);

  return std::move(output_region.region);
}

}  // namespace printing
