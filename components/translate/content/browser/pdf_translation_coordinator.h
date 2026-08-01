// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_PDF_PDF_TRANSLATION_COORDINATOR_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_PDF_PDF_TRANSLATION_COORDINATOR_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_user_data.h"

namespace translate {

// PDFTranslationCoordinator orchestrates the PDF-specific translation
// lifecycle, including asynchronous translatability checks.
// It is a Per-Document manager created when checking a PDF frame.
class PDFTranslationCoordinator
    : public content::DocumentUserData<PDFTranslationCoordinator> {
 public:
  enum class TranslatabilityStatus {
    kNotChecked,      // Initial state on page load
    kTranslatable,    // Check completed: PDF is translatable
    kUntranslatable,  // Check completed: PDF is NOT translatable
  };

  ~PDFTranslationCoordinator() override;

  // Determines whether the current PDF is translatable. The callback will
  // receive the translatability result (true/false).
  void RunIfPdfIsTranslatable(base::OnceClosure callback);

  TranslatabilityStatus status() const { return status_; }
  bool is_translatable() const {
    return status_ == TranslatabilityStatus::kTranslatable;
  }

 private:
  friend class content::DocumentUserData<PDFTranslationCoordinator>;
  friend class PDFTranslationCoordinatorTest;

  explicit PDFTranslationCoordinator(content::RenderFrameHost* rfh);

  // Called when the translatability check completes.
  void OnTranslatabilityDetermined(bool is_translatable);

  void StartTranslatabilityCheck();

  TranslatabilityStatus status_ = TranslatabilityStatus::kNotChecked;
  std::vector<base::OnceClosure> pending_callbacks_;

  base::WeakPtrFactory<PDFTranslationCoordinator> weak_ptr_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_PDF_PDF_TRANSLATION_COORDINATOR_H_
