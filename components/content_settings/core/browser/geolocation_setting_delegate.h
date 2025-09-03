// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_GEOLOCATION_SETTING_DELEGATE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_GEOLOCATION_SETTING_DELEGATE_H_

#include <optional>

#include "base/values.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

class GeolocationSettingDelegate
    : public content_settings::PermissionSettingsInfo::Delegate {
 public:
  bool IsValid(const PermissionSetting& setting) const override;
  bool IsDefaultSettingValid(const PermissionSetting& setting) const override;

  PermissionSetting InheritInIncognito(
      const PermissionSetting& setting) const override;

  bool IsAnyPermissionAllowed(const PermissionSetting& setting) const override;
  bool IsUndecided(const PermissionSetting& setting) const override;
  bool CanTrackLastVisit() const override;

  bool ShouldCoalesceEphemeralState() const override;
  PermissionSetting CoalesceEphemeralState(
      const PermissionSetting& persistent_permission_setting,
      const PermissionSetting& ephemeral_permission_setting) const override;
  PermissionSetting ApplyPermissionEmbargo(
      const PermissionSetting& setting) const override;

  base::Value ToValue(const PermissionSetting& setting) const override;
  std::optional<PermissionSetting> FromValue(
      const base::Value& value) const override;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_GEOLOCATION_SETTING_DELEGATE_H_
