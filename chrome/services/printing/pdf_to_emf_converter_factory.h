// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_H_
#define CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_H_

#include "chrome/services/printing/public/mojom/pdf_to_emf_converter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace printing {

class PdfToEmfConverterFactory : public mojom::PdfToEmfConverterFactory {
 public:
  PdfToEmfConverterFactory();

  PdfToEmfConverterFactory(const PdfToEmfConverterFactory&) = delete;
  PdfToEmfConverterFactory& operator=(const PdfToEmfConverterFactory&) = delete;

  ~PdfToEmfConverterFactory() override;

  static void Create(
      mojo::PendingReceiver<mojom::PdfToEmfConverterFactory> receiver);

 private:
  // mojom::PdfToEmfConverterFactory implementation.
  void CreateConverter(base::ReadOnlySharedMemoryRegion pdf_region,
                       const PdfRenderSettings& render_settings,
                       CreateConverterCallback callback) override;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_H_
