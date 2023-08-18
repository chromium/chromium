// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_H_
#define CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/services/printing/public/mojom/pdf_to_emf_converter.mojom.h"
#include "printing/pdf_render_settings.h"

namespace printing {

class PdfToEmfConverter : public mojom::PdfToEmfConverter {
 public:
  PdfToEmfConverter(base::ReadOnlySharedMemoryRegion pdf_region,
                    const PdfRenderSettings& render_settings);

  PdfToEmfConverter(const PdfToEmfConverter&) = delete;
  PdfToEmfConverter& operator=(const PdfToEmfConverter&) = delete;

  ~PdfToEmfConverter() override;

  int total_page_count() const { return total_page_count_; }

 private:
  // mojom::PdfToEmfConverter implementation.
  void ConvertPage(uint32_t page_index, ConvertPageCallback callback) override;
  void SetWebContentsURL(const GURL& url) override;
  void SetUseSkiaRendererPolicy(bool use_skia) override;

  void SetPrintMode();
  void LoadPdf(base::ReadOnlySharedMemoryRegion pdf_region);
  base::ReadOnlySharedMemoryRegion RenderPdfPageToMetafile(int page_number,
                                                           bool postscript,
                                                           float* scale_factor);

  uint32_t total_page_count_ = 0;
  PdfRenderSettings pdf_render_settings_;
  base::ReadOnlySharedMemoryMapping pdf_mapping_;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_H_
