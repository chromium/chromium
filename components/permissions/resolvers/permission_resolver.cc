// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/permission_resolver.h"

#include "components/permissions/permission_util.h"

namespace permissions {

PermissionResolver::PermissionSetting::PermissionSetting(
    ContentSetting permission_content_setting,
    base::Value permission_options)
    : content_setting(permission_content_setting),
      options(std::move(permission_options)) {}

PermissionResolver::PermissionSetting::PermissionSetting(
    PermissionResolver::PermissionSetting& other) {
  content_setting = other.content_setting;
  options = other.options.Clone();
}

bool PermissionResolver::PermissionSetting::operator==(
    const PermissionResolver::PermissionSetting& other) const {
  return content_setting == other.content_setting && options == other.options;
}

PermissionResolver::PermissionResolver(
    ContentSettingsType content_settings_type)
    : content_settings_type_(content_settings_type) {}

}  // namespace permissions
