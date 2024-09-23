// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/android/popup_blocked_infobar_delegate.h"

#include <stddef.h>
#include <utility>

#include "components/blocked_content/android/popup_blocked_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace blocked_content {

// static
bool PopupBlockedInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    int num_popups,
    HostContentSettingsMap* settings_map,
    base::OnceClosure on_accept_callback) {
  const GURL& url = infobar_manager->web_contents()->GetURL();
  auto infobar = std::make_unique<infobars::ConfirmInfoBar>(
      base::WrapUnique<PopupBlockedInfoBarDelegate>(
          new PopupBlockedInfoBarDelegate(num_popups, url, settings_map,
                                          std::move(on_accept_callback))));

  // See if there is an existing popup infobar already.
  // TODO(dfalcantara) When triggering more than one popup the infobar
  // will be shown once, then hide then be shown again.
  // This will be fixed once we have an in place replace infobar mechanism.
  for (infobars::InfoBar* existing_infobar : infobar_manager->infobars()) {
    if (existing_infobar->delegate()->AsPopupBlockedInfoBarDelegate()) {
      infobar_manager->ReplaceInfoBar(existing_infobar, std::move(infobar));
      return false;
    }
  }

  infobar_manager->AddInfoBar(std::move(infobar));

  return true;
}

PopupBlockedInfoBarDelegate::~PopupBlockedInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
PopupBlockedInfoBarDelegate::GetIdentifier() const {
  return POPUP_BLOCKED_INFOBAR_DELEGATE_MOBILE;
}

int PopupBlockedInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_BLOCKED_POPUPS;
}

PopupBlockedInfoBarDelegate*
PopupBlockedInfoBarDelegate::AsPopupBlockedInfoBarDelegate() {
  return this;
}

PopupBlockedInfoBarDelegate::PopupBlockedInfoBarDelegate(
    int num_popups,
    const GURL& url,
    HostContentSettingsMap* map,
    base::OnceClosure on_accept_callback)
    : ConfirmInfoBarDelegate(),
      num_popups_(num_popups),
      url_(url),
      map_(map),
      on_accept_callback_(std::move(on_accept_callback)) {
  can_show_popups_ = !PopupSettingManagedByPolicy(map, url);
}

std::u16string PopupBlockedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetPluralStringFUTF16(IDS_POPUPS_BLOCKED_INFOBAR_TEXT,
                                          num_popups_);
}

int PopupBlockedInfoBarDelegate::GetButtons() const {
  if (!can_show_popups_)
    return 0;

  int buttons = BUTTON_OK;

  return buttons;
}

std::u16string PopupBlockedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  switch (button) {
    case BUTTON_OK:
      return l10n_util::GetStringUTF16(IDS_POPUPS_BLOCKED_INFOBAR_BUTTON_SHOW);
    case BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return std::u16string();
}

bool PopupBlockedInfoBarDelegate::Accept() {
  DCHECK(can_show_popups_);

  // Create exceptions.
  map_->SetNarrowestContentSetting(url_, url_, ContentSettingsType::POPUPS,
                                   CONTENT_SETTING_ALLOW);

  // Launch popups.
  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar());
  ShowBlockedPopups(web_contents);

  if (on_accept_callback_)
    std::move(on_accept_callback_).Run();
  return true;
}

}  // namespace blocked_content
