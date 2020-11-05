// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/android/popup_blocked_infobar_delegate.h"

#include <stddef.h>
#include <utility>

#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
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
  std::unique_ptr<infobars::InfoBar> infobar(
      infobar_manager->CreateConfirmInfoBar(
          std::unique_ptr<ConfirmInfoBarDelegate>(
              new PopupBlockedInfoBarDelegate(num_popups, url, settings_map,
                                              std::move(on_accept_callback)))));

  // See if there is an existing popup infobar already.
  // TODO(dfalcantara) When triggering more than one popup the infobar
  // will be shown once, then hide then be shown again.
  // This will be fixed once we have an in place replace infobar mechanism.
  for (size_t i = 0; i < infobar_manager->infobar_count(); ++i) {
    infobars::InfoBar* existing_infobar = infobar_manager->infobar_at(i);
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
  content_settings::SettingInfo setting_info;
  std::unique_ptr<base::Value> setting = map->GetWebsiteSetting(
      url, url, ContentSettingsType::POPUPS, &setting_info);
  can_show_popups_ =
      setting_info.source != content_settings::SETTING_SOURCE_POLICY;
}

base::string16 PopupBlockedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetPluralStringFUTF16(IDS_POPUPS_BLOCKED_INFOBAR_TEXT,
                                          num_popups_);
}

int PopupBlockedInfoBarDelegate::GetButtons() const {
  if (!can_show_popups_)
    return 0;

  int buttons = BUTTON_OK;

  return buttons;
}

base::string16 PopupBlockedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  switch (button) {
    case BUTTON_OK:
      return l10n_util::GetStringUTF16(IDS_POPUPS_BLOCKED_INFOBAR_BUTTON_SHOW);
    case BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(IDS_PERMISSION_DENY);
    default:
      NOTREACHED();
      break;
  }
  return base::string16();
}

bool PopupBlockedInfoBarDelegate::Accept() {
  DCHECK(can_show_popups_);

  // Create exceptions.
  map_->SetNarrowestContentSetting(url_, url_, ContentSettingsType::POPUPS,
                                   CONTENT_SETTING_ALLOW);

  // Launch popups.
  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar());
  blocked_content::PopupBlockerTabHelper* popup_blocker_helper =
      blocked_content::PopupBlockerTabHelper::FromWebContents(web_contents);
  DCHECK(popup_blocker_helper);
  blocked_content::PopupBlockerTabHelper::PopupIdMap blocked_popups =
      popup_blocker_helper->GetBlockedPopupRequests();
  for (blocked_content::PopupBlockerTabHelper::PopupIdMap::iterator it =
           blocked_popups.begin();
       it != blocked_popups.end(); ++it) {
    popup_blocker_helper->ShowBlockedPopup(it->first,
                                           WindowOpenDisposition::CURRENT_TAB);
  }

  if (on_accept_callback_)
    std::move(on_accept_callback_).Run();
  return true;
}

}  // namespace blocked_content
