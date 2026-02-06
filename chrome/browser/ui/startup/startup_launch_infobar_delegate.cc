// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_launch_infobar_delegate.h"

#include <memory>

#include "base/types/pass_key.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/startup/startup_launch_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"

using InfoBarType = StartupLaunchInfoBarManager::InfoBarType;

// static
std::unique_ptr<StartupLaunchInfoBarDelegate>
StartupLaunchInfoBarDelegate::Create(Profile* profile,
                                     InfoBarType infobar_type) {
  return std::make_unique<StartupLaunchInfoBarDelegate>(
      base::PassKey<StartupLaunchInfoBarDelegate>(), profile, infobar_type);
}

StartupLaunchInfoBarDelegate::StartupLaunchInfoBarDelegate(
    base::PassKey<StartupLaunchInfoBarDelegate>,
    Profile* profile,
    InfoBarType infobar_type)
    : profile_(profile), infobar_type_(infobar_type) {}

StartupLaunchInfoBarDelegate::~StartupLaunchInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
StartupLaunchInfoBarDelegate::GetIdentifier() const {
  return STARTUP_LAUNCH_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& StartupLaunchInfoBarDelegate::GetVectorIcon() const {
  return dark_mode() ? omnibox::kProductChromeRefreshIcon
                     : vector_icons::kProductRefreshIcon;
}

bool StartupLaunchInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

std::u16string StartupLaunchInfoBarDelegate::GetMessageText() const {
  switch (infobar_type_) {
    case InfoBarType::kForegroundOptIn:
      return l10n_util::GetStringUTF16(IDS_STARTUP_LAUNCH_INFOBAR_OPT_IN_TITLE);
    case InfoBarType::kForegroundOptOut:
      return l10n_util::GetStringUTF16(
          IDS_STARTUP_LAUNCH_INFOBAR_OPT_OUT_TITLE);
  }
}

int StartupLaunchInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::optional<ui::ButtonStyle> StartupLaunchInfoBarDelegate::GetButtonStyle(
    ConfirmInfoBarDelegate::InfoBarButton /*button*/) const {
  switch (infobar_type_) {
    case InfoBarType::kForegroundOptIn:
      return ui::ButtonStyle::kProminent;
    case InfoBarType::kForegroundOptOut:
      return ui::ButtonStyle::kTonal;
  }
}

bool StartupLaunchInfoBarDelegate::Accept() {
  // Notify observers but don't close the infobar. Closing infobar will occur
  // when the corresponding pref is changed.
  ConfirmInfoBarDelegate::Accept();
  return false;
}

std::u16string StartupLaunchInfoBarDelegate::GetButtonLabel(
    InfoBarButton /*button*/) const {
  switch (infobar_type_) {
    case InfoBarType::kForegroundOptIn:
      return l10n_util::GetStringUTF16(IDS_STARTUP_LAUNCH_INFOBAR_ALLOW_BUTTON);
    case InfoBarType::kForegroundOptOut:
      return l10n_util::GetStringUTF16(
          IDS_STARTUP_LAUNCH_INFOBAR_SETTINGS_BUTTON);
  }
}

bool StartupLaunchInfoBarDelegate::ShouldHideInFullscreen() const {
  return true;
}
