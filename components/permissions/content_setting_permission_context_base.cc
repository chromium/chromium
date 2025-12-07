// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/content_setting_permission_context_base.h"

#include "components/content_settings/core/browser/content_settings_registry.h"
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

void ContentSettingPermissionContextBase::UpdateContentSetting(
    const PermissionRequestData& request_data,
    ContentSetting content_setting,
    bool is_one_time) {
  DCHECK_EQ(request_data.requesting_origin,
            request_data.requesting_origin.DeprecatedGetOriginAsURL());
  DCHECK_EQ(request_data.embedding_origin,
            request_data.embedding_origin.DeprecatedGetOriginAsURL());

  content_settings::ContentSettingConstraints constraints;
  constraints.set_session_model(
      is_one_time ? content_settings::mojom::SessionModel::ONE_TIME
                  : content_settings::mojom::SessionModel::DURABLE);

  // The Permissions module in Safety check will revoke permissions after
  // a finite amount of time if the permission can be revoked.
  if (content_settings::CanBeAutoRevokedAsUnusedPermission(
          content_settings_type(),
          content_settings::ContentSettingToValue(content_setting),
          is_one_time)) {
    constraints.set_track_last_visit_for_autoexpiration(true);
  }

  if (is_one_time) {
    if (content_settings::ShouldTypeExpireActively(content_settings_type())) {
      constraints.set_lifetime(kOneTimePermissionMaximumLifetime);
    }
  }

  PermissionsClient::Get()
      ->GetSettingsMap(browser_context())
      ->SetContentSettingDefaultScope(
          request_data.requesting_origin, request_data.embedding_origin,
          content_settings_type(), content_setting, constraints);
}

void ContentSettingPermissionContextBase::UpdateSetting(
    const PermissionRequestData& request_data,
    PermissionSetting setting,
    bool is_one_time) {
  auto content_setting = std::get<ContentSetting>(setting);
  CHECK(content_setting == CONTENT_SETTING_ALLOW ||
        content_setting == CONTENT_SETTING_BLOCK);
  UpdateContentSetting(request_data, content_setting, is_one_time);
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

}  // namespace permissions
