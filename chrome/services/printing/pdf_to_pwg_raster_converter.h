// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_H_
#define CHROME_SERVICES_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_H_

#include "chrome/services/printing/public/mojom/pdf_to_pwg_raster_converter.mojom.h"

namespace printing {

struct PdfRenderSettings;

class PdfToPwgRasterConverter
    : public printing::mojom::PdfToPwgRasterConverter {
 public:
  PdfToPwgRasterConverter();

  PdfToPwgRasterConverter(const PdfToPwgRasterConverter&) = delete;
  PdfToPwgRasterConverter& operator=(const PdfToPwgRasterConverter&) = delete;

  ~PdfToPwgRasterConverter() override;

 private:
  // printing::mojom::PdfToPwgRasterConverter
  void Convert(base::ReadOnlySharedMemoryRegion pdf_region,
               const PdfRenderSettings& pdf_settings,
               const PwgRasterSettings& pwg_raster_settings,
               ConvertCallback callback) override;
  void SetUseSkiaRendererPolicy(bool use_skia) override;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PDF_TO_PWG_RASTER_CONVERTER_H_
