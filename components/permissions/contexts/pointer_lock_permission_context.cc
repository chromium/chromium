// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/pointer_lock_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permissions_client.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

PointerLockPermissionContext::PointerLockPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::POINTER_LOCK,
          network::mojom::PermissionsPolicyFeature::kNotFound) {}

void PointerLockPermissionContext::NotifyPermissionSet(
    const PermissionRequestData& request_data,
    BrowserPermissionCallback callback,
    bool persist,
    PermissionDecision decision,
    bool is_final_decision) {
  if (decision == PermissionDecision::kAllowThisTime) {
    // When `persist` is true, one-time permissions persist for a short period
    // of time (managed by OneTimePermissionProvider), which we do not want for
    // pointer lock. If another request is made (e.g. after a page navigation),
    // the request must obtain a user permission again.
    persist = false;
  }
  permissions::ContentSettingPermissionContextBase::NotifyPermissionSet(
      request_data, std::move(callback), persist, decision, is_final_decision);
}

#if !BUILDFLAG(IS_ANDROID)
ContentSetting PointerLockPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_ALLOW;
}
#endif

}  // namespace permissions
