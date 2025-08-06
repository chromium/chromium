// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/permission_settings_info.h"

#include "base/containers/contains.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"

namespace content_settings {

PermissionSettingsInfo::PermissionSettingsInfo(
    const WebsiteSettingsInfo* website_settings_info,
    const std::vector<std::string>& allowlisted_primary_schemes,
    OriginRestriction origin_restriction,
    std::unique_ptr<Delegate> delegate)
    : website_settings_info_(website_settings_info),
      allowlisted_primary_schemes_(allowlisted_primary_schemes),
      origin_restriction_(origin_restriction),
      delegate_(std::move(delegate)) {
  CHECK(delegate_);
}

PermissionSettingsInfo::~PermissionSettingsInfo() = default;

PermissionSetting PermissionSettingsInfo::GetInitialDefaultSetting() const {
  const base::Value& initial_default =
      website_settings_info()->initial_default_value();
  std::optional<PermissionSetting> default_setting =
      delegate_->FromValue(initial_default);
  DCHECK(default_setting);
  DCHECK(delegate_->IsValid(*default_setting));
  return *default_setting;
}

bool PermissionSettingsInfo::Delegate::IsBlocked(
    const PermissionSetting& setting) const {
  return !IsAnyPermissionAllowed(setting) && !IsUndecided(setting);
}

PermissionSetting PermissionSettingsInfo::Delegate::CoalesceEphemeralState(
    const PermissionSetting& persistent_permission_setting,
    const PermissionSetting& ephemeral_permission_setting) const {
  NOTREACHED();
}

}  // namespace content_settings
