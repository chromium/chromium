// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_H_
#define CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "chrome/services/printing/public/mojom/pdf_to_emf_converter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace printing {

class PdfToEmfConverterFactory : public mojom::PdfToEmfConverterFactory {
 public:
  PdfToEmfConverterFactory();
  ~PdfToEmfConverterFactory() override;

  static void Create(
      mojo::PendingReceiver<mojom::PdfToEmfConverterFactory> receiver);

 private:
  // mojom::PdfToEmfConverterFactory implementation.
  void CreateConverter(
      base::ReadOnlySharedMemoryRegion pdf_region,
      const PdfRenderSettings& render_settings,
      mojo::PendingRemote<mojom::PdfToEmfConverterClient> client,
      CreateConverterCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(PdfToEmfConverterFactory);
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PDF_TO_EMF_CONVERTER_FACTORY_H_
