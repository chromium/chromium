// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/local_network_access_compat_permission_context.h"

#include "base/logging.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

LocalNetworkAccessCompatPermissionContext::
    LocalNetworkAccessCompatPermissionContext(
        content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::LOCAL_NETWORK_ACCESS,
          // TODO(crbug.com/465491626): figure out if we should specify
          // kLocalNetworkAccess here.
          network::mojom::PermissionsPolicyFeature::kNotFound) {}

LocalNetworkAccessCompatPermissionContext::
    ~LocalNetworkAccessCompatPermissionContext() = default;

ContentSetting
LocalNetworkAccessCompatPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  ContentSetting local_network_setting =
      PermissionsClient::Get()
          ->GetSettingsMap(browser_context())
          ->GetContentSetting(requesting_origin, embedding_origin,
                              ContentSettingsType::LOCAL_NETWORK);

  ContentSetting loopback_network_setting =
      PermissionsClient::Get()
          ->GetSettingsMap(browser_context())
          ->GetContentSetting(requesting_origin, embedding_origin,
                              ContentSettingsType::LOOPBACK_NETWORK);

  DCHECK(local_network_setting == CONTENT_SETTING_ALLOW ||
         local_network_setting == CONTENT_SETTING_BLOCK ||
         local_network_setting == CONTENT_SETTING_ASK);

  DCHECK(loopback_network_setting == CONTENT_SETTING_ALLOW ||
         loopback_network_setting == CONTENT_SETTING_BLOCK ||
         loopback_network_setting == CONTENT_SETTING_ASK);

  // If only one of the two has a non-ask setting, return that.
  if ((local_network_setting == CONTENT_SETTING_ALLOW ||
       local_network_setting == CONTENT_SETTING_BLOCK) &&
      loopback_network_setting == CONTENT_SETTING_ASK) {
    return local_network_setting;
  }

  if ((loopback_network_setting == CONTENT_SETTING_ALLOW ||
       loopback_network_setting == CONTENT_SETTING_BLOCK) &&
      local_network_setting == CONTENT_SETTING_ASK) {
    return loopback_network_setting;
  }

  // If one is blocked, return blocked.
  if (loopback_network_setting == CONTENT_SETTING_BLOCK ||
      local_network_setting == CONTENT_SETTING_BLOCK) {
    return CONTENT_SETTING_BLOCK;
  }

  // At this point, both are ALLOW or both are ASK. Return whatever they are.
  return local_network_setting;
}

// Nothing should be setting or requesting this permission this when split
// permissions are enabled.
void LocalNetworkAccessCompatPermissionContext::UpdateContentSetting(
    const permissions::PermissionRequestData& request_data,
    ContentSetting content_setting,
    bool is_one_time) {
  NOTREACHED();
}

void LocalNetworkAccessCompatPermissionContext::RequestPermission(
    std::unique_ptr<PermissionRequestData> request_data,
    BrowserPermissionCallback callback) {
  NOTREACHED();
}

}  // namespace permissions
