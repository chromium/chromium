// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_delegate.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
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
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/shell_util.h"
#include "ui/aura/window.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace pdf::infobar {
namespace {

#if BUILDFLAG(IS_WIN)
// Potential results of showing the Windows settings UI to set Chrome as the
// default PDF viewer.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PdfInfoBarSettingsResult)
enum class PdfInfoBarSettingsResult {
  // The settings UI was purposely not shown. This is unexpected, and indicates
  // that the infobar was shown when setting Chrome as default was inappropriate
  // (e.g., when Chrome is already the default).
  kNotShown = 0,
  // The settings UI was shown and Chrome was set as default PDF viewer.
  kSuccess = 1,
  // The settings UI was shown, but Chrome wasn't set as default.
  kSuccessNoChange = 2,
  // The fallback settings UI was shown and Chrome was set as default.
  kFallback = 3,
  // The fallback settings UI was shown, but Chrome wasn't set as default.
  kFallbackNoChange = 4,
  // The settings UI failed to open.
  kError = 5,
  kMaxValue = kError
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PdfInfoBarSettingsResult)

void RecordSettingsResultHistogram(PdfInfoBarSettingsResult result) {
  base::UmaHistogramEnumeration("PDF.InfoBar.SettingsResult", result);
}

// Emit a histogram recording the result of opening the Windows settings UI. If
// it was opened successfully, check whether Chrome has been set as the default
// PDF viewer and emit that to the histogram.
void RecordSettingsResult(ShellUtil::ShowSystemUIResult result) {
  auto record_settings_result = base::BindOnce(
      [](ShellUtil::ShowSystemUIResult result) {
        switch (result) {
          case ShellUtil::ShowSystemUIResult::kNotShown: {
            RecordSettingsResultHistogram(PdfInfoBarSettingsResult::kNotShown);
            break;
          }
          case ShellUtil::ShowSystemUIResult::kError: {
            RecordSettingsResultHistogram(PdfInfoBarSettingsResult::kError);
            break;
          }
          case ShellUtil::ShowSystemUIResult::kSuccess: {
            RecordSettingsResultHistogram(
                shell_integration::IsDefaultHandlerForFileExtension(".pdf")
                    ? PdfInfoBarSettingsResult::kSuccess
                    : PdfInfoBarSettingsResult::kSuccessNoChange);
            break;
          }
          case ShellUtil::ShowSystemUIResult::kFallback: {
            RecordSettingsResultHistogram(
                shell_integration::IsDefaultHandlerForFileExtension(".pdf")
                    ? PdfInfoBarSettingsResult::kFallback
                    : PdfInfoBarSettingsResult::kFallbackNoChange);
          }
        }
      },
      result);
  // Check whether Chrome has been set as default after a short delay, to wait
  // for the user to interact with the settings UI (while the "Select a default
  // app for .pdf files" pop-up only returns after it closes, the fallback
  // "Default apps" page returns right after opening).
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(record_settings_result), base::Seconds(30));
}
#endif  // BUILDFLAG(IS_WIN)

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
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ShellUtil::ShowSetDefaultForFileExtensionSystemUI,
                     base::PathService::CheckedGet(base::FILE_EXE),
                     base::wcstring_view(L".pdf"),
                     views::HWNDForNativeWindow(window)),
      base::BindOnce(&RecordSettingsResult));
#else
#error PdfInfoBarDelegate should only be created on Windows or MacOS
#endif
  return ConfirmInfoBarDelegate::Accept();
}

void PdfInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  RecordUserInteractionHistogram(PdfInfoBarUserInteraction::kDismissed);
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

}  // namespace pdf::infobar
