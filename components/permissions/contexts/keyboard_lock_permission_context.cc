// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/keyboard_lock_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

KeyboardLockPermissionContext::KeyboardLockPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::KEYBOARD_LOCK,
          network::mojom::PermissionsPolicyFeature::kNotFound) {}

#if !BUILDFLAG(IS_ANDROID)
ContentSetting KeyboardLockPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (base::FeatureList::IsEnabled(features::kKeyboardLockPrompt)) {
    return ContentSettingPermissionContextBase::GetContentSettingStatusInternal(
        render_frame_host, requesting_origin, embedding_origin);
  }
  return CONTENT_SETTING_ALLOW;
}
#endif

}  // namespace permissions
