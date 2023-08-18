// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/pdf_to_emf_converter_factory.h"

#include <memory>
#include <utility>

#include "chrome/services/printing/pdf_to_emf_converter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace printing {

PdfToEmfConverterFactory::PdfToEmfConverterFactory() = default;

PdfToEmfConverterFactory::~PdfToEmfConverterFactory() = default;

void PdfToEmfConverterFactory::CreateConverter(
    base::ReadOnlySharedMemoryRegion pdf_region,
    const PdfRenderSettings& render_settings,
    CreateConverterCallback callback) {
  auto converter = std::make_unique<PdfToEmfConverter>(std::move(pdf_region),
                                                       render_settings);
  uint32_t page_count = converter->total_page_count();
  mojo::PendingRemote<mojom::PdfToEmfConverter> converter_remote;
  mojo::MakeSelfOwnedReceiver(
      std::move(converter), converter_remote.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(std::move(converter_remote), page_count);
}

// static
void PdfToEmfConverterFactory::Create(
    mojo::PendingReceiver<mojom::PdfToEmfConverterFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<PdfToEmfConverterFactory>(),
                              std::move(receiver));
}

}  // namespace printing
