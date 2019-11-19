// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/printing_service.h"

#include "build/build_config.h"
#include "chrome/services/printing/pdf_nup_converter.h"
#include "chrome/services/printing/pdf_to_pwg_raster_converter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(OS_CHROMEOS)
#include "chrome/services/printing/pdf_flattener.h"
#endif

#if defined(OS_WIN)
#include "chrome/services/printing/pdf_to_emf_converter.h"
#include "chrome/services/printing/pdf_to_emf_converter_factory.h"
#endif

namespace printing {

PrintingService::PrintingService(
    mojo::PendingReceiver<mojom::PrintingService> receiver)
    : receiver_(this, std::move(receiver)) {}

PrintingService::~PrintingService() = default;

void PrintingService::BindPdfNupConverter(
    mojo::PendingReceiver<mojom::PdfNupConverter> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<printing::PdfNupConverter>(),
                              std::move(receiver));
}

void PrintingService::BindPdfToPwgRasterConverter(
    mojo::PendingReceiver<mojom::PdfToPwgRasterConverter> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<printing::PdfToPwgRasterConverter>(),
      std::move(receiver));
}

#if defined(OS_CHROMEOS)
void PrintingService::BindPdfFlattener(
    mojo::PendingReceiver<mojom::PdfFlattener> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<printing::PdfFlattener>(),
                              std::move(receiver));
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
void PrintingService::BindPdfToEmfConverterFactory(
    mojo::PendingReceiver<mojom::PdfToEmfConverterFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<printing::PdfToEmfConverterFactory>(),
      std::move(receiver));
}
#endif

}  // namespace printing
