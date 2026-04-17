// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/content_setting_permission_context_base.h"

#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permissions_client.h"

namespace permissions {
ContentSettingPermissionContextBase::ContentSettingPermissionContextBase(
    content::BrowserContext* browser_context,
    ContentSettingsType content_settings_type,
    network::mojom::PermissionsPolicyFeature permissions_policy_feature)
    : PermissionContextBase(browser_context,
                            content_settings_type,
                            permissions_policy_feature) {}

void ContentSettingPermissionContextBase::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_settings_type())) {
    return;
  }

  PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->SetContentSettingDefaultScope(requesting_origin, embedding_origin,
                                      content_settings_type(),
                                      CONTENT_SETTING_DEFAULT);
}

ContentSetting
ContentSettingPermissionContextBase::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->GetContentSetting(requesting_origin, embedding_origin,
                          content_settings_type());
}

PermissionSetting
ContentSettingPermissionContextBase::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return GetContentSettingStatusInternal(render_frame_host, requesting_origin,
                                         embedding_origin);
}

void ContentSettingPermissionContextBase::UpdateSetting(
    const PermissionRequestData& request_data,
    const PermissionSetting& setting,
    bool is_one_time) {
  CHECK(std::holds_alternative<ContentSetting>(setting));
  ContentSetting content_setting = std::get<ContentSetting>(setting);
  CHECK(content_setting == CONTENT_SETTING_ALLOW ||
        content_setting == CONTENT_SETTING_BLOCK);
  PermissionContextBase::UpdateSetting(request_data, setting, is_one_time);
}

}  // namespace permissions
