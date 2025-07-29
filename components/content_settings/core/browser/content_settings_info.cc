// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_info.h"

#include <optional>
#include <variant>

#include "base/containers/contains.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace content_settings {

ContentSettingsInfo::ContentSettingsInfo(
    const PermissionSettingsInfo* permission_settings_info,
    Delegate* delegate,
    const std::set<ContentSetting>& valid_settings,
    IncognitoBehavior incognito_behavior)
    : permission_settings_info_(permission_settings_info),
      delegate_(delegate),
      valid_settings_(valid_settings),
      incognito_behavior_(incognito_behavior) {
  delegate->set_content_settings_info(this);
}

ContentSettingsInfo::~ContentSettingsInfo() {
  delegate_->set_content_settings_info(nullptr);
}

ContentSetting ContentSettingsInfo::GetInitialDefaultSetting() const {
  const base::Value& initial_default =
      website_settings_info()->initial_default_value();
  DCHECK(initial_default.is_int());
  return ValueToContentSetting(initial_default);
}

bool ContentSettingsInfo::IsSettingValid(ContentSetting setting) const {
  return base::Contains(valid_settings_, setting);
}

// TODO(raymes): Find a better way to deal with the special-casing in
// IsDefaultSettingValid.
bool ContentSettingsInfo::IsDefaultSettingValid(ContentSetting setting) const {
  ContentSettingsType type = website_settings_info()->type();
  // Don't support ALLOW for the default media settings.
  if ((type == ContentSettingsType::MEDIASTREAM_CAMERA ||
       type == ContentSettingsType::MEDIASTREAM_MIC) &&
      setting == CONTENT_SETTING_ALLOW) {
    return false;
  }

  // Don't support ALLOW for the file system settings.
  if ((type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD ||
       type == ContentSettingsType::FILE_SYSTEM_READ_GUARD) &&
      setting == CONTENT_SETTING_ALLOW) {
    return false;
  }

  return base::Contains(valid_settings_, setting);
}

bool ContentSettingsInfo::Delegate::IsValid(
    const PermissionSetting& setting) const {
  auto* content_setting = std::get_if<ContentSetting>(&setting);
  if (!content_setting) {
    return false;
  }
  return info_->IsSettingValid(*content_setting);
}

PermissionSetting ContentSettingsInfo::Delegate::InheritInIncognito(
    const PermissionSetting& setting) const {
  ContentSetting content_setting = std::get<ContentSetting>(setting);
  switch (info_->incognito_behavior()) {
    case ContentSettingsInfo::INHERIT_IN_INCOGNITO:
      return content_setting;
    case ContentSettingsInfo::DONT_INHERIT_IN_INCOGNITO:
      return ValueToContentSetting(
          info_->website_settings_info()->initial_default_value());
    case ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE:
      ContentSetting initial_setting = ValueToContentSetting(
          info_->website_settings_info()->initial_default_value());
      if (IsMorePermissive(content_setting, initial_setting)) {
        return initial_setting;
      }
      return content_setting;
  }
}

bool ContentSettingsInfo::Delegate::ShouldCoalesceEphemeralState() const {
  return false;
}

bool ContentSettingsInfo::Delegate::IsAnyPermissionAllowed(
    PermissionSetting setting) const {
  return std::get<ContentSetting>(setting) == CONTENT_SETTING_ALLOW;
}

bool ContentSettingsInfo::Delegate::IsUndecided(
    PermissionSetting setting) const {
  return std::get<ContentSetting>(setting) == CONTENT_SETTING_ASK;
}

bool ContentSettingsInfo::Delegate::CanTrackLastVisit() const {
  ContentSettingsType type = info_->website_settings_info()->type();

  // Last visit is not tracked for notification permission as it shouldn't be
  // auto-revoked.
  if (type == ContentSettingsType::NOTIFICATIONS) {
    return false;
  }

  // Protocol handler don't actually use their content setting and don't have
  // a valid "initial default" value.
  if (type == ContentSettingsType::PROTOCOL_HANDLERS) {
    return false;
  }
  return info_->GetInitialDefaultSetting() == CONTENT_SETTING_ASK;
}

base::Value ContentSettingsInfo::Delegate::ToValue(
    const PermissionSetting& setting) const {
  return ContentSettingToValue((std::get<ContentSetting>(setting)));
}

std::optional<PermissionSetting> ContentSettingsInfo::Delegate::FromValue(
    const base::Value& value) const {
  return ParseContentSettingValue(value);
}

}  // namespace content_settings
