// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PDF_PDF_PROGRESSIVE_SEARCHIFIER_H_
#define CHROME_SERVICES_PDF_PDF_PROGRESSIVE_SEARCHIFIER_H_

#include <memory>

#include "chrome/services/pdf/public/mojom/pdf_progressive_searchifier.mojom.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"

class SkBitmap;

namespace chrome_pdf {
class PdfProgressiveSearchifier;
}  // namespace chrome_pdf

namespace pdf {

// PdfProgressiveSearchifier creates a PDF and provides operations to add and
// delete pages, and save the searchified PDF. The service will crash if any of
// the operations experienced an error.
class PdfProgressiveSearchifier : public mojom::PdfProgressiveSearchifier {
 public:
  explicit PdfProgressiveSearchifier(mojo::PendingRemote<mojom::Ocr> Ocr);
  PdfProgressiveSearchifier(const PdfProgressiveSearchifier&) = delete;
  PdfProgressiveSearchifier& operator=(const PdfProgressiveSearchifier&) =
      delete;
  ~PdfProgressiveSearchifier() override;

  // mojom::PdfProgressiveSearchifier
  void AddPage(const SkBitmap& bitmap, uint32_t index) override;
  void DeletePage(uint32_t index) override;
  void Save(SaveCallback callback) override;

 private:
  screen_ai::mojom::VisualAnnotationPtr PerformOcr(const SkBitmap& bitmap);

  mojo::Remote<mojom::Ocr> ocr_remote_;
  std::unique_ptr<chrome_pdf::PdfProgressiveSearchifier>
      progressive_searchifier_;
};

}  // namespace pdf

#endif  // CHROME_SERVICES_PDF_PDF_PROGRESSIVE_SEARCHIFIER_H_
