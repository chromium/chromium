// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PDF_NUP_CONVERTER_H_
#define CHROME_SERVICES_PRINTING_PDF_NUP_CONVERTER_H_

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/services/printing/public/mojom/pdf_nup_converter.mojom.h"

namespace printing {

class PdfNupConverter : public printing::mojom::PdfNupConverter {
 public:
  PdfNupConverter();

  PdfNupConverter(const PdfNupConverter&) = delete;
  PdfNupConverter& operator=(const PdfNupConverter&) = delete;

  ~PdfNupConverter() override;

  // printing::mojom::PdfNupConverter
  void NupPageConvert(
      uint32_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area,
      std::vector<base::ReadOnlySharedMemoryRegion> pdf_page_regions,
      NupPageConvertCallback callback) override;
  void NupDocumentConvert(uint32_t pages_per_sheet,
                          const gfx::Size& page_size,
                          const gfx::Rect& printable_area,
                          base::ReadOnlySharedMemoryRegion src_pdf_region,
                          NupDocumentConvertCallback callback) override;
  void SetWebContentsURL(const GURL& url) override;
  void SetUseSkiaRendererPolicy(bool use_skia) override;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PDF_NUP_CONVERTER_H_
