// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PDF_PDF_SEARCHIFIER_H_
#define CHROME_SERVICES_PDF_PDF_SEARCHIFIER_H_

#include <cstdint>
#include <vector>

#include "chrome/services/pdf/public/mojom/pdf_searchifier.mojom.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"

class SkBitmap;

namespace pdf {

// PdfSearchifier converts inaccessible PDFs to searchable PDFs. OCR remotes
// passed to PdfSearchifier may be called multiple times.
class PdfSearchifier : public mojom::PdfSearchifier {
 public:
  explicit PdfSearchifier(mojo::PendingRemote<mojom::Ocr> Ocr);
  PdfSearchifier(const PdfSearchifier&) = delete;
  PdfSearchifier& operator=(const PdfSearchifier&) = delete;
  ~PdfSearchifier() override;

  // mojom::PdfSearchifier
  void Searchify(const std::vector<uint8_t>& pdf,
                 SearchifyCallback searchified_callback) override;

 private:
  screen_ai::mojom::VisualAnnotationPtr PerformOcr(const SkBitmap& bitmap);

  mojo::Remote<mojom::Ocr> ocr_remote_;
};

}  // namespace pdf

#endif  // CHROME_SERVICES_PDF_PDF_SEARCHIFIER_H_
