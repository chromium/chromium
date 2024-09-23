// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PDF_PDF_SERVICE_H_
#define CHROME_SERVICES_PDF_PDF_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ChromeOS ash-chrome only");

namespace discardable_memory {
class ClientDiscardableSharedMemoryManager;
}

namespace pdf {

class PdfService : public mojom::PdfService {
 public:
  explicit PdfService(mojo::PendingReceiver<mojom::PdfService> receiver);

  PdfService(const PdfService&) = delete;
  PdfService& operator=(const PdfService&) = delete;

  ~PdfService() override;

 private:
  // mojom::PdfService:
  void BindPdfProgressiveSearchifier(
      mojo::PendingReceiver<mojom::PdfProgressiveSearchifier> receiver,
      mojo::PendingRemote<mojom::Ocr> ocr) override;
  void BindPdfSearchifier(mojo::PendingReceiver<mojom::PdfSearchifier> receiver,
                          mojo::PendingRemote<mojom::Ocr> ocr) override;
  void BindPdfThumbnailer(
      mojo::PendingReceiver<mojom::PdfThumbnailer> receiver) override;

  scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
  mojo::Receiver<mojom::PdfService> receiver_;
};

}  // namespace pdf

#endif  // CHROME_SERVICES_PDF_PDF_SERVICE_H_
