// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/pdf_nup_converter.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "components/crash/core/common/crash_key.h"
#include "pdf/pdf.h"

namespace printing {

namespace {

// |pdf_mappings| which has the pdf page data needs to remain valid until
// ConvertPdfPagesToNupPdf completes, since base::span is a reference type.
std::vector<base::span<const uint8_t>> CreatePdfPagesVector(
    const std::vector<base::ReadOnlySharedMemoryRegion>& pdf_page_regions,
    std::vector<base::ReadOnlySharedMemoryMapping>* pdf_mappings) {
  std::vector<base::span<const uint8_t>> pdf_page_span;

  for (auto& pdf_page_region : pdf_page_regions) {
    base::ReadOnlySharedMemoryMapping pdf_mapping = pdf_page_region.Map();
    pdf_page_span.push_back(pdf_mapping.GetMemoryAsSpan<const uint8_t>());
    pdf_mappings->push_back(std::move(pdf_mapping));
  }

  return pdf_page_span;
}

template <class Callback>
void RunCallbackWithConversionResult(Callback callback,
                                     const std::vector<uint8_t>& buffer) {
  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(buffer.size());
  if (!region_mapping.IsValid()) {
    std::move(callback).Run(mojom::PdfNupConverter::Status::HANDLE_MAP_ERROR,
                            std::move(region_mapping.region));
    return;
  }

  memcpy(region_mapping.mapping.memory(), buffer.data(), buffer.size());
  std::move(callback).Run(mojom::PdfNupConverter::Status::SUCCESS,
                          std::move(region_mapping.region));
}

}  // namespace

PdfNupConverter::PdfNupConverter() = default;

PdfNupConverter::~PdfNupConverter() {}

void PdfNupConverter::NupPageConvert(
    uint32_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area,
    std::vector<base::ReadOnlySharedMemoryRegion> pdf_page_regions,
    NupPageConvertCallback callback) {
  std::vector<base::ReadOnlySharedMemoryMapping> pdf_mappings;
  std::vector<base::span<const uint8_t>> input_pdf_buffers =
      CreatePdfPagesVector(pdf_page_regions, &pdf_mappings);

  std::vector<uint8_t> output_pdf_buffer = chrome_pdf::ConvertPdfPagesToNupPdf(
      std::move(input_pdf_buffers), pages_per_sheet, page_size, printable_area);
  if (output_pdf_buffer.empty()) {
    std::move(callback).Run(mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
                            base::ReadOnlySharedMemoryRegion());
    return;
  }

  RunCallbackWithConversionResult(std::move(callback), output_pdf_buffer);
}

void PdfNupConverter::NupDocumentConvert(
    uint32_t pages_per_sheet,
    const gfx::Size& page_size,
    const gfx::Rect& printable_area,
    base::ReadOnlySharedMemoryRegion src_pdf_region,
    NupDocumentConvertCallback callback) {
  base::ReadOnlySharedMemoryMapping pdf_document_mapping = src_pdf_region.Map();
  auto input_pdf_buffer = pdf_document_mapping.GetMemoryAsSpan<const uint8_t>();

  std::vector<uint8_t> output_pdf_buffer =
      chrome_pdf::ConvertPdfDocumentToNupPdf(input_pdf_buffer, pages_per_sheet,
                                             page_size, printable_area);
  if (output_pdf_buffer.empty()) {
    std::move(callback).Run(mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
                            base::ReadOnlySharedMemoryRegion());
    return;
  }

  RunCallbackWithConversionResult(std::move(callback), output_pdf_buffer);
}

void PdfNupConverter::SetWebContentsURL(const GURL& url) {
  // Record the most recent url we tried to print. This should be sufficient
  // for users using print preview by default.
  static crash_reporter::CrashKeyString<1024> crash_key("main-frame-url");
  crash_key.Set(url.spec());
}

void PdfNupConverter::SetUseSkiaRendererPolicy(bool use_skia) {
  chrome_pdf::SetUseSkiaRendererPolicy(use_skia);
}

}  // namespace printing
