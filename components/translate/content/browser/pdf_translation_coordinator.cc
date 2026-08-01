// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/pdf_translation_coordinator.h"

#include <utility>

#include "base/barrier_callback.h"
#include "base/functional/callback.h"
#include "base/functional/bind.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace translate {

DOCUMENT_USER_DATA_KEY_IMPL(PDFTranslationCoordinator);

PDFTranslationCoordinator::PDFTranslationCoordinator(content::RenderFrameHost* rfh)
    : content::DocumentUserData<PDFTranslationCoordinator>(rfh) {}

PDFTranslationCoordinator::~PDFTranslationCoordinator() = default;

void PDFTranslationCoordinator::RunIfPdfIsTranslatable(
    base::OnceClosure callback) {
  if (status_ == TranslatabilityStatus::kTranslatable) {
    std::move(callback).Run();
    return;
  }
  if (status_ == TranslatabilityStatus::kUntranslatable) {
    return;
  }

  pending_callbacks_.push_back(std::move(callback));
  if (pending_callbacks_.size() > 1) {
    return;
  }

  // Get the PDFDocumentHelper from the current document.
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::GetForCurrentDocument(&this->render_frame_host());
  if (!pdf_helper) {
    OnTranslatabilityDetermined(false);
    return;
  }

  pdf_helper->RegisterForDocumentLoadComplete(
      base::BindOnce(&PDFTranslationCoordinator::StartTranslatabilityCheck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PDFTranslationCoordinator::StartTranslatabilityCheck() {
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::GetForCurrentDocument(&this->render_frame_host());
  if (!pdf_helper) {
    OnTranslatabilityDetermined(false);
    return;
  }

  enum class PdfCheckType { kMeaningfulText, kJavaScript, kPasswordProtected };
  using PdfCheckResult = std::pair<PdfCheckType, bool>;

  auto completion_callback = base::BindOnce(
      &PDFTranslationCoordinator::OnTranslatabilityDetermined,
      weak_ptr_factory_.GetWeakPtr());

  auto pdf_checks_barrier = base::BarrierCallback<PdfCheckResult>(
      3, base::BindOnce(
             [](base::OnceCallback<void(bool)> completion_callback,
                std::vector<PdfCheckResult> results) {
               bool has_meaningful_text = false;
               bool has_javascript = true;
               bool is_password_protected = true;
               for (const auto& [type, value] : results) {
                 switch (type) {
                   case PdfCheckType::kMeaningfulText:
                     has_meaningful_text = value;
                     break;
                   case PdfCheckType::kJavaScript:
                     has_javascript = value;
                     break;
                   case PdfCheckType::kPasswordProtected:
                     is_password_protected = value;
                     break;
                 }
               }
               std::move(completion_callback)
                   .Run(has_meaningful_text && !has_javascript &&
                        !is_password_protected);
             },
             mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                 std::move(completion_callback), false)));

  pdf_helper->HasMeaningfulText(base::BindOnce(
      [](base::RepeatingCallback<void(PdfCheckResult)> barrier, bool result) {
        barrier.Run({PdfCheckType::kMeaningfulText, result});
      },
      pdf_checks_barrier));

  pdf_helper->HasJavaScript(base::BindOnce(
      [](base::RepeatingCallback<void(PdfCheckResult)> barrier, bool result) {
        barrier.Run({PdfCheckType::kJavaScript, result});
      },
      pdf_checks_barrier));

  pdf_helper->IsPasswordProtected(base::BindOnce(
      [](base::RepeatingCallback<void(PdfCheckResult)> barrier, bool result) {
        barrier.Run({PdfCheckType::kPasswordProtected, result});
      },
      pdf_checks_barrier));
}

void PDFTranslationCoordinator::OnTranslatabilityDetermined(
    bool is_translatable) {
  status_ = is_translatable ? TranslatabilityStatus::kTranslatable
                            : TranslatabilityStatus::kUntranslatable;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&this->render_frame_host());
  if (web_contents) {
    ContentTranslateDriver* driver =
        ContentTranslateDriver::FromWebContents(web_contents);
    if (driver && driver->translate_manager()) {
      translate::LanguageState* language_state = driver->translate_manager()->GetLanguageState();
      language_state->set_pdf_translatability_status(
          is_translatable
              ? LanguageState::PdfTranslatabilityStatus::kTranslatable
              : LanguageState::PdfTranslatabilityStatus::kUntranslatable);
      if (!is_translatable) {
        language_state->SetPageLevelTranslationCriteriaMet(false);
      }
    }
  }

  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(pending_callbacks_);
  if (is_translatable) {
    for (auto& callback : callbacks) {
      std::move(callback).Run();
    }
  }
}

}  // namespace translate
