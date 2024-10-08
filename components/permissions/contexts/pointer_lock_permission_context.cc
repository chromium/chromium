// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/pointer_lock_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

PointerLockPermissionContext::PointerLockPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::POINTER_LOCK,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

void PointerLockPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  if (is_one_time) {
    // When `persist` is true, one-time permissions persist for a short period
    // of time (managed by OneTimePermissionProvider), which we do not want for
    // pointer lock. If another request is made (e.g. after a page navigation),
    // the request must obtain a user permission again.
    persist = false;
  }
  permissions::PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting, is_one_time, is_final_decision);
}

#if !BUILDFLAG(IS_ANDROID)
ContentSetting PointerLockPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (base::FeatureList::IsEnabled(features::kKeyboardAndPointerLockPrompt)) {
    return PermissionsClient::Get()
        ->GetSettingsMap(browser_context())
        ->GetContentSetting(requesting_origin, embedding_origin,
                            ContentSettingsType::POINTER_LOCK);
  }
  return CONTENT_SETTING_ALLOW;
}
#endif

}  // namespace permissions
