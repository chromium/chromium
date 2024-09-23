// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PRINTING_SERVICE_H_
#define CHROME_SERVICES_PRINTING_PRINTING_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/printing/public/mojom/printing_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/buildflags/buildflags.h"

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace printing {

class PrintingService : public mojom::PrintingService {
 public:
  explicit PrintingService(
      mojo::PendingReceiver<mojom::PrintingService> receiver);

  PrintingService(const PrintingService&) = delete;
  PrintingService& operator=(const PrintingService&) = delete;

  ~PrintingService() override;

 private:
  // mojom::PrintingService implementation:
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void BindPdfNupConverter(
      mojo::PendingReceiver<mojom::PdfNupConverter> receiver) override;
  void BindPdfToPwgRasterConverter(
      mojo::PendingReceiver<mojom::PdfToPwgRasterConverter> receiver) override;
#endif
#if BUILDFLAG(IS_CHROMEOS)
  void BindPdfFlattener(
      mojo::PendingReceiver<mojom::PdfFlattener> receiver) override;
#endif
#if BUILDFLAG(IS_WIN)
  void BindPdfToEmfConverterFactory(
      mojo::PendingReceiver<mojom::PdfToEmfConverterFactory> receiver) override;
#endif

  scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
  mojo::Receiver<mojom::PrintingService> receiver_;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PRINTING_SERVICE_H_
