// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/permission_resolver.h"

#ifndef COMPONENTS_PERMISSIONS_RESOLVERS_CONTENT_SETTING_PERMISSION_RESOLVER_H_
#define COMPONENTS_PERMISSIONS_RESOLVERS_CONTENT_SETTING_PERMISSION_RESOLVER_H_

namespace permissions {

// PermissionResolver for basic ContentSetting permissions which do not use
// permission options.
class ContentSettingPermissionResolver : public PermissionResolver {
 public:
  explicit ContentSettingPermissionResolver(
      ContentSettingsType content_settings_type);

  blink::mojom::PermissionStatus DeterminePermissionStatus(
      PermissionSetting setting) override;

  PermissionSetting ComputePermissionDecisionResult(
      PermissionSetting previous_setting,
      ContentSetting decision,
      std::optional<base::Value> prompt_options) override;

  ContentSetting default_value_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_RESOLVERS_CONTENT_SETTING_PERMISSION_RESOLVER_H_
