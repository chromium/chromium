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
          // Permission policy check is special-cased, see
          // PermissionAllowedByPermissionsPolicy() below.
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

// If something is looking for the local-network-access permission policy,
// delegate to the local-network or loopback-network permission policy. This is
// unlikely to be needed as when
// network::features::kLocalNetworkAccessChecksSplitPermissions is enabled
// (which is the only time this class should be used), local-network or
// loopback-network should be checked in the Chromium implementation, but is
// done 1) as a defense-in-depth measure, and 2) to ensure web-facing backward
// compatibility so that the result of permission.query({name:
// 'local-network-access'}) is mostly correctly feature-policy gated.
//
// Note that
// services/network/public/cpp/permissions_policy/permissions_policy.cc also
// delegates as well.
bool LocalNetworkAccessCompatPermissionContext::
    PermissionAllowedByPermissionsPolicy(content::RenderFrameHost* rfh) const {
  return rfh->IsFeatureEnabled(
             network::mojom::PermissionsPolicyFeature::kLocalNetwork) ||
         rfh->IsFeatureEnabled(
             network::mojom::PermissionsPolicyFeature::kLoopbackNetwork);
}

// Split permissions mode never has ContentSettingsType::LOCAL_NETWORK_ACCESS
// change, but instead has ContentSettingsType::LOCAL_NETWORK and
// ContentSettingsType::LOOPBACK_NETWORK change; trigger obsevers when those
// change instead.
void LocalNetworkAccessCompatPermissionContext::NotifyObservers(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) const {
  if (content_type_set.Contains(ContentSettingsType::LOCAL_NETWORK) ||
      content_type_set.Contains(ContentSettingsType::LOOPBACK_NETWORK)) {
    PermissionContextBase::NotifyObservers(
        primary_pattern, secondary_pattern,
        ContentSettingsTypeSet(ContentSettingsType::LOCAL_NETWORK_ACCESS));
  }
}

}  // namespace permissions
