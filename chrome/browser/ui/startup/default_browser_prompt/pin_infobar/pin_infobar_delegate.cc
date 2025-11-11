// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_delegate.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace default_browser {

namespace {

void RecordUserInteractionHistogram(PinInfoBarUserInteraction interaction) {
  base::UmaHistogramEnumeration("DefaultBrowser.PinInfoBar.UserInteraction",
                                interaction);
}

}  // namespace

// static
infobars::InfoBar* PinInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  CHECK(infobar_manager);
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<PinInfoBarDelegate>()));
}

PinInfoBarDelegate::~PinInfoBarDelegate() {
  if (!action_taken_) {
    RecordUserInteractionHistogram(PinInfoBarUserInteraction::kIgnored);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier PinInfoBarDelegate::GetIdentifier()
    const {
  return PIN_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& PinInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
                     : vector_icons::kProductRefreshIcon;
}

std::u16string PinInfoBarDelegate::GetMessageText() const {
#if BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_TEXT);
#elif BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_DOCK_TEXT);
#endif
}

std::u16string PinInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
#if BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_BUTTON);
#elif BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_DOCK_BUTTON);
#endif
}

int PinInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

bool PinInfoBarDelegate::Accept() {
  action_taken_ = true;
  RecordUserInteractionHistogram(PinInfoBarUserInteraction::kAccepted);

  // Pin Chrome to taskbar.
#if BUILDFLAG(IS_WIN)
  browser_util::PinAppToTaskbar(
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
      browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
      base::DoNothing());
#elif BUILDFLAG(IS_MAC)
  PinChromeToDock();
#endif

  return ConfirmInfoBarDelegate::Accept();
}

void PinInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  RecordUserInteractionHistogram(PinInfoBarUserInteraction::kDismissed);
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

}  // namespace default_browser
