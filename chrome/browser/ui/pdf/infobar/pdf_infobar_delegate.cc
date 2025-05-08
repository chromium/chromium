// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_delegate.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/shell_util.h"
#include "ui/aura/window.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

void RecordUserInteractionHistogram(PdfInfoBarUserInteraction interaction) {
  base::UmaHistogramEnumeration("PDF.InfoBar.UserInteraction", interaction);
}

}  // namespace

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
    RecordUserInteractionHistogram(PdfInfoBarUserInteraction::kIgnored);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier PdfInfoBarDelegate::GetIdentifier()
    const {
  return PDF_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& PdfInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
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
  RecordUserInteractionHistogram(PdfInfoBarUserInteraction::kAccepted);

#if BUILDFLAG(IS_MAC)
  shell_integration::SetAsDefaultHandlerForUTType("com.adobe.pdf");
#elif BUILDFLAG(IS_WIN)
  auto* window =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar())
          ->GetTopLevelNativeWindow();
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(base::IgnoreResult(
                         &ShellUtil::ShowSetDefaultForFileExtensionSystemUI),
                     base::PathService::CheckedGet(base::FILE_EXE),
                     base::wcstring_view(L".pdf"),
                     views::HWNDForNativeWindow(window)));
#else
#error PdfInfoBarDelegate should only be created on Windows or MacOS
#endif
  // TODO(crbug.com/396202897): record metrics when the user accepts, dismisses,
  // or ignores the infobar, and whether opening the system UI succeeds.
  return ConfirmInfoBarDelegate::Accept();
}

void PdfInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  RecordUserInteractionHistogram(PdfInfoBarUserInteraction::kDismissed);
  ConfirmInfoBarDelegate::InfoBarDismissed();
}
