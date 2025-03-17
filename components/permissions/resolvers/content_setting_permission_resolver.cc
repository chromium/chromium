// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/content_setting_permission_resolver.h"

#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_util.h"

namespace permissions {

ContentSettingPermissionResolver::ContentSettingPermissionResolver(
    ContentSettingsType content_settings_type)
    : PermissionResolver(content_settings_type),
      default_value_(content_settings::ContentSettingsRegistry::GetInstance()
                         ->Get(content_settings_type)
                         ->GetInitialDefaultSetting()) {}

blink::mojom::PermissionStatus
ContentSettingPermissionResolver::DeterminePermissionStatus(
    PermissionSetting setting) {
  CHECK(setting.options.is_none());
  return PermissionUtil::ContentSettingToPermissionStatus(
      setting.content_setting == CONTENT_SETTING_DEFAULT
          ? default_value_
          : setting.content_setting);
}

ContentSettingPermissionResolver::PermissionSetting
ContentSettingPermissionResolver::ComputePermissionDecisionResult(
    PermissionSetting previous_setting,
    ContentSetting decision,
    std::optional<base::Value> prompt_options) {
  // Pure content settings don't have or set any options
  CHECK(previous_setting.options.is_none());
  CHECK(!prompt_options.has_value() || prompt_options->is_none());
  return PermissionSetting(
      decision == CONTENT_SETTING_DEFAULT ? default_value_ : decision,
      base::Value());
}

}  // namespace permissions
