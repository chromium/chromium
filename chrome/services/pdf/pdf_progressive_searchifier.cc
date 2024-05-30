// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/pdf/pdf_progressive_searchifier.h"

#include <utility>

#include "chrome/services/pdf/public/mojom/pdf_progressive_searchifier.mojom.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/pdf.h"
#include "pdf/pdf_progressive_searchifier.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace pdf {

PdfProgressiveSearchifier::PdfProgressiveSearchifier(
    mojo::PendingRemote<mojom::Ocr> ocr)
    : ocr_remote_(std::move(ocr)),
      progressive_searchifier_(chrome_pdf::CreateProgressiveSearchifier()) {}

PdfProgressiveSearchifier::~PdfProgressiveSearchifier() = default;

screen_ai::mojom::VisualAnnotationPtr PdfProgressiveSearchifier::PerformOcr(
    const SkBitmap& bitmap) {
  screen_ai::mojom::VisualAnnotationPtr annotation;
  ocr_remote_->PerformOcr(bitmap, &annotation);
  return annotation;
}

void PdfProgressiveSearchifier::AddPage(const SkBitmap& bitmap,
                                        uint32_t index) {
  progressive_searchifier_->AddPage(bitmap, index, PerformOcr(bitmap));
}

void PdfProgressiveSearchifier::DeletePage(uint32_t index) {
  progressive_searchifier_->DeletePage(index);
}

void PdfProgressiveSearchifier::Save(SaveCallback callback) {
  std::move(callback).Run(progressive_searchifier_->Save());
}

}  // namespace pdf
