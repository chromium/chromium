// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_info.h"

#include "base/containers/contains.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace content_settings {

ContentSettingsInfo::ContentSettingsInfo(
    const WebsiteSettingsInfo* website_settings_info,
    const std::vector<std::string>& allowlisted_primary_schemes,
    const std::set<ContentSetting>& valid_settings,
    IncognitoBehavior incognito_behavior,
    OriginRestriction origin_restriction)
    : website_settings_info_(website_settings_info),
      allowlisted_primary_schemes_(allowlisted_primary_schemes),
      valid_settings_(valid_settings),
      incognito_behavior_(incognito_behavior),
      origin_restriction_(origin_restriction) {}

ContentSettingsInfo::~ContentSettingsInfo() = default;

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
  ContentSettingsType type = website_settings_info_->type();
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

}  // namespace content_settings
