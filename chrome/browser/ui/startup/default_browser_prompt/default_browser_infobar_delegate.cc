// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"

#include <memory>

#include "base/types/pass_key.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* DefaultBrowserInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    Profile* profile,
    bool can_pin_to_taskbar) {
  return infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::make_unique<DefaultBrowserInfoBarDelegate>(
          base::PassKey<DefaultBrowserInfoBarDelegate>(), profile,
          can_pin_to_taskbar)));
}

DefaultBrowserInfoBarDelegate::DefaultBrowserInfoBarDelegate(
    base::PassKey<DefaultBrowserInfoBarDelegate>,
    Profile* profile,
    bool can_pin_to_taskbar)
    : profile_(profile), can_pin_to_taskbar_(can_pin_to_taskbar) {}

DefaultBrowserInfoBarDelegate::~DefaultBrowserInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
DefaultBrowserInfoBarDelegate::GetIdentifier() const {
  return DEFAULT_BROWSER_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& DefaultBrowserInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
                     : vector_icons::kProductRefreshIcon;
}

bool DefaultBrowserInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

void DefaultBrowserInfoBarDelegate::InfoBarDismissed() {
  // |profile_| may be null in tests.
  if (profile_) {
    chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(profile_);
  }

  ConfirmInfoBarDelegate::InfoBarDismissed();
}

std::u16string DefaultBrowserInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(can_pin_to_taskbar_
                                       ? IDS_DEFAULT_BROWSER_PIN_INFOBAR_TEXT
                                       : IDS_DEFAULT_BROWSER_INFOBAR_TEXT);
}

int DefaultBrowserInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string DefaultBrowserInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL);
}

bool DefaultBrowserInfoBarDelegate::Accept() {
  // |profile_| may be null in tests.
  if (profile_) {
    chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(profile_);
  }

  return ConfirmInfoBarDelegate::Accept();
}

bool DefaultBrowserInfoBarDelegate::ShouldHideInFullscreen() const {
  return true;
}
