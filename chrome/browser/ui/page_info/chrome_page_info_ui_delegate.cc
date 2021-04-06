// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"

ChromePageInfoUiDelegate::ChromePageInfoUiDelegate(Profile* profile,
                                                   const GURL& site_url)
    : profile_(profile), site_url_(site_url) {}

permissions::PermissionResult ChromePageInfoUiDelegate::GetPermissionStatus(
    ContentSettingsType type) {
  return PermissionManagerFactory::GetForProfile(profile_)->GetPermissionStatus(
      type, site_url_, site_url_);
}

#if !defined(OS_ANDROID)

bool ChromePageInfoUiDelegate::IsBlockAutoPlayEnabled() {
  return profile_->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled);
}
#endif

bool ChromePageInfoUiDelegate::ShouldShowAllow(ContentSettingsType type) {
  switch (type) {
    // Notifications and idle detection do not support CONTENT_SETTING_ALLOW in
    // incognito.
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::IDLE_DETECTION:
      return !profile_->IsOffTheRecord();
    // Media only supports CONTENT_SETTING_ALLOW for secure origins.
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return network::IsUrlPotentiallyTrustworthy(site_url_);
    // Chooser permissions do not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::BLUETOOTH_GUARD:
    case ContentSettingsType::HID_GUARD:
    // Bluetooth scanning does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::BLUETOOTH_SCANNING:
    // File system write does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      return false;
    default:
      return true;
  }
}

bool ChromePageInfoUiDelegate::ShouldShowAsk(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::BLUETOOTH_GUARD:
    case ContentSettingsType::BLUETOOTH_SCANNING:
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
    case ContentSettingsType::HID_GUARD:
      return true;
    default:
      return false;
  }
}
