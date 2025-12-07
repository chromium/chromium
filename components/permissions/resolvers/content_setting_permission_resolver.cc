// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/content_setting_permission_resolver.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/permission_prompt_options.h"

namespace permissions {

ContentSettingPermissionResolver::ContentSettingPermissionResolver(
    ContentSettingsType content_settings_type)
    : PermissionResolver(content_settings_type) {
  auto* info = content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_settings_type);
  if (info) {
    default_value_ = info->GetInitialDefaultSetting();
  }
}

ContentSettingPermissionResolver::ContentSettingPermissionResolver(
    RequestType request_type)
    : PermissionResolver(request_type) {}

blink::mojom::PermissionStatus
ContentSettingPermissionResolver::DeterminePermissionStatus(
    const PermissionSetting& setting) const {
  ContentSetting content_setting = std::get<ContentSetting>(setting);
  content_setting = content_setting == CONTENT_SETTING_DEFAULT
                        ? default_value_
                        : content_setting;
  return PermissionUtil::ContentSettingToPermissionStatus(content_setting);
}

PermissionSetting
ContentSettingPermissionResolver::ComputePermissionDecisionResult(
    const PermissionSetting& previous_setting,
    PermissionDecision decision,
    PromptOptions prompt_options) const {
  return PermissionUtil::PermissionDecisionToContentSetting(decision);
}

ContentSettingPermissionResolver::PromptParameters
ContentSettingPermissionResolver::GetPromptParameters(
    const PermissionSetting& current_setting_state) const {
  // TODO(crbug.com/417916654): Migrate PermissionRequest prompt parameters into
  // PermissionResolvers.
  NOTIMPLEMENTED();
  return PromptParameters();
}

}  // namespace permissions
