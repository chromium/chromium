// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_delegate.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/branded_strings.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace default_browser {

// static
infobars::InfoBar* PinInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  CHECK(infobar_manager);
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<PinInfoBarDelegate>()));
}

PinInfoBarDelegate::~PinInfoBarDelegate() {
  if (!action_taken_) {
    base::UmaHistogramEnumeration("DefaultBrowser.PinInfoBar.UserInteraction",
                                  PinInfoBarUserInteraction::kIgnored);
  }
}

infobars::InfoBarDelegate::InfoBarIdentifier PinInfoBarDelegate::GetIdentifier()
    const {
  return PIN_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& PinInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? features::IsRoundedIconsEnabled()
                           ? omnibox::kChromeProductIcon
                           : omnibox::kProductChromeRefreshOldIcon
                     : vector_icons::kProductRefreshIcon;
}

std::u16string PinInfoBarDelegate::GetMessageText() const {
  return PinInfoBarController::GetMessageText();
}

std::u16string PinInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  return PinInfoBarController::GetButtonLabel();
}

int PinInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

bool PinInfoBarDelegate::Accept() {
  action_taken_ = true;
  PinInfoBarController::OnAccept(/*web_contents=*/nullptr);
  return ConfirmInfoBarDelegate::Accept();
}

void PinInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  PinInfoBarController::OnDismiss(/*web_contents=*/nullptr);
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

}  // namespace default_browser
