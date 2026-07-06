// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_delegate.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/native_ui_types.h"

namespace pdf::infobar {

// static
infobars::InfoBar* PdfInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  base::UmaHistogramBoolean("PDF.InfoBar.Shown", true);

  CHECK(infobar_manager);
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<PdfInfoBarDelegate>()));
}

PdfInfoBarDelegate::~PdfInfoBarDelegate() {
  if (!action_taken_) {
    PdfInfoBarController::RecordUserInteractionHistogram(
        PdfInfoBarUserInteraction::kIgnored);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier PdfInfoBarDelegate::GetIdentifier()
    const {
  return PDF_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& PdfInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? features::IsRoundedIconsEnabled()
                           ? omnibox::kChromeProductIcon
                           : omnibox::kProductChromeRefreshOldIcon
                     : vector_icons::kProductRefreshIcon;
}

std::u16string PdfInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PDF_INFOBAR_TEXT);
}

int PdfInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string PdfInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL);
}

bool PdfInfoBarDelegate::Accept() {
  action_taken_ = true;
  PdfInfoBarController::RecordUserInteractionHistogram(
      PdfInfoBarUserInteraction::kAccepted);

  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar());
  PdfInfoBarController::SetAsDefaultPdfHandler(web_contents);

  return ConfirmInfoBarDelegate::Accept();
}

void PdfInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  PdfInfoBarController::RecordUserInteractionHistogram(
      PdfInfoBarUserInteraction::kDismissed);
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

}  // namespace pdf::infobar
