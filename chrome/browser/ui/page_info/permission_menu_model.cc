// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/permission_menu_model.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/origin_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

PermissionMenuModel::PermissionMenuModel(Profile* profile,
                                         const GURL& url,
                                         const PageInfoUI::PermissionInfo& info,
                                         const ChangeCallback& callback)
    : ui::SimpleMenuModel(this),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile)),
      permission_(info),
      callback_(callback) {
  DCHECK(!callback_.is_null());
  base::string16 label;

  DCHECK_NE(permission_.default_setting, CONTENT_SETTING_NUM_SETTINGS);

  // The Material UI for site settings uses comboboxes instead of menubuttons,
  // which means the elements of the menu themselves have to be shorter, instead
  // of simply setting a shorter label on the menubutton.
  label = PageInfoUI::PermissionActionToUIString(
      profile, permission_.type, CONTENT_SETTING_DEFAULT,
      permission_.default_setting, permission_.source);

  AddCheckItem(CONTENT_SETTING_DEFAULT, label);

  // Retrieve the string to show for allowing the permission.
  if (ShouldShowAllow(url)) {
    label = PageInfoUI::PermissionActionToUIString(
        profile, permission_.type, CONTENT_SETTING_ALLOW,
        permission_.default_setting, permission_.source);
    AddCheckItem(CONTENT_SETTING_ALLOW, label);
  }

  // Retrieve the string to show for blocking the permission.
  label = PageInfoUI::PermissionActionToUIString(
      profile, info.type, CONTENT_SETTING_BLOCK, permission_.default_setting,
      info.source);
  AddCheckItem(CONTENT_SETTING_BLOCK, label);

  // Retrieve the string to show for allowing the user to be asked about the
  // permission.
  if (ShouldShowAsk(url)) {
    label = PageInfoUI::PermissionActionToUIString(
        profile, info.type, CONTENT_SETTING_ASK, permission_.default_setting,
        info.source);
    AddCheckItem(CONTENT_SETTING_ASK, label);
  }
}

PermissionMenuModel::~PermissionMenuModel() {}

bool PermissionMenuModel::IsCommandIdChecked(int command_id) const {
  return permission_.setting == command_id;
}

bool PermissionMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void PermissionMenuModel::ExecuteCommand(int command_id, int event_flags) {
  permission_.setting = static_cast<ContentSetting>(command_id);
  callback_.Run(permission_);
}

bool PermissionMenuModel::ShouldShowAllow(const GURL& url) {
  switch (permission_.type) {
    // Notifications does not support CONTENT_SETTING_ALLOW in incognito.
    case ContentSettingsType::NOTIFICATIONS:
      return !permission_.is_incognito;
    // Media only supports CONTENT_SETTING_ALLOW for secure origins.
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return content::IsOriginSecure(url);
    // Chooser permissions do not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::USB_GUARD:
    // Bluetooth scanning does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::BLUETOOTH_SCANNING:
    // Native file system write does not support CONTENT_SETTING_ALLOW.
    case ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD:
      return false;
    default:
      return true;
  }
}

bool PermissionMenuModel::ShouldShowAsk(const GURL& url) {
  switch (permission_.type) {
    case ContentSettingsType::USB_GUARD:
    case ContentSettingsType::SERIAL_GUARD:
    case ContentSettingsType::BLUETOOTH_SCANNING:
    case ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD:
      return true;
    default:
      return false;
  }
}
