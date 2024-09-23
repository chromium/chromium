// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/pdf/pdf_searchifier.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/services/pdf/public/mojom/pdf_searchifier.mojom.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/pdf.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace pdf {

PdfSearchifier::PdfSearchifier(mojo::PendingRemote<mojom::Ocr> ocr)
    : ocr_remote_(std::move(ocr)) {}

PdfSearchifier::~PdfSearchifier() = default;

void PdfSearchifier::Searchify(const std::vector<uint8_t>& pdf,
                               SearchifyCallback searchified_callback) {
  // `base::Unretained(this)` is used because the return type is not void. It's
  // safe since `PdfSearchifier` will outlive the entire `chrome_pdf::Searchify`
  // work.
  auto perform_ocr_callback =
      base::BindRepeating(&PdfSearchifier::PerformOcr, base::Unretained(this));
  std::vector<uint8_t> searchified_pdf =
      chrome_pdf::Searchify(pdf, std::move(perform_ocr_callback));
  std::move(searchified_callback).Run(std::move(searchified_pdf));
}

screen_ai::mojom::VisualAnnotationPtr PdfSearchifier::PerformOcr(
    const SkBitmap& bitmap) {
  screen_ai::mojom::VisualAnnotationPtr annotation;
  ocr_remote_->PerformOcr(bitmap, &annotation);
  return annotation;
}

}  // namespace pdf
