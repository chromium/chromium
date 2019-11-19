// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/content_settings/popup_blocked_infobar_delegate.h"

#include <stddef.h>
#include <utility>

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

// static
void PopupBlockedInfoBarDelegate::Create(content::WebContents* web_contents,
                                         int num_popups) {
  const GURL& url = web_contents->GetURL();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  std::unique_ptr<infobars::InfoBar> infobar(
      infobar_service->CreateConfirmInfoBar(
          std::unique_ptr<ConfirmInfoBarDelegate>(
              new PopupBlockedInfoBarDelegate(
                  num_popups, url,
                  HostContentSettingsMapFactory::GetForProfile(profile)))));

  // See if there is an existing popup infobar already.
  // TODO(dfalcantara) When triggering more than one popup the infobar
  // will be shown once, then hide then be shown again.
  // This will be fixed once we have an in place replace infobar mechanism.
  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* existing_infobar = infobar_service->infobar_at(i);
    if (existing_infobar->delegate()->AsPopupBlockedInfoBarDelegate()) {
      infobar_service->ReplaceInfoBar(existing_infobar, std::move(infobar));
      return;
    }
  }

  infobar_service->AddInfoBar(std::move(infobar));

  content_settings::RecordPopupsAction(
      content_settings::POPUPS_ACTION_DISPLAYED_INFOBAR_ON_MOBILE);
}

PopupBlockedInfoBarDelegate::~PopupBlockedInfoBarDelegate() {
}

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
    HostContentSettingsMap* map)
    : ConfirmInfoBarDelegate(), num_popups_(num_popups), url_(url), map_(map) {
  content_settings::SettingInfo setting_info;
  std::unique_ptr<base::Value> setting = map->GetWebsiteSetting(
      url, url, ContentSettingsType::POPUPS, std::string(), &setting_info);
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
      InfoBarService::WebContentsFromInfoBar(infobar());
  PopupBlockerTabHelper* popup_blocker_helper =
      PopupBlockerTabHelper::FromWebContents(web_contents);
  DCHECK(popup_blocker_helper);
  PopupBlockerTabHelper::PopupIdMap blocked_popups =
      popup_blocker_helper->GetBlockedPopupRequests();
  for (PopupBlockerTabHelper::PopupIdMap::iterator it = blocked_popups.begin();
      it != blocked_popups.end(); ++it)
    popup_blocker_helper->ShowBlockedPopup(it->first,
                                           WindowOpenDisposition::CURRENT_TAB);

  content_settings::RecordPopupsAction(
      content_settings::POPUPS_ACTION_CLICKED_ALWAYS_SHOW_ON_MOBILE);
  return true;
}
