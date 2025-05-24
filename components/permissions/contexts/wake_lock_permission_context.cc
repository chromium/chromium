// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/wake_lock_permission_context.h"

#include "base/check.h"
#include "base/notreached.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

WakeLockPermissionContext::WakeLockPermissionContext(
    content::BrowserContext* browser_context,
    ContentSettingsType content_settings_type)
    : PermissionContextBase(
          browser_context,
          content_settings_type,
          content_settings_type == ContentSettingsType::WAKE_LOCK_SCREEN
              ? network::mojom::PermissionsPolicyFeature::kScreenWakeLock
              : network::mojom::PermissionsPolicyFeature::kNotFound) {
  DCHECK(content_settings_type == ContentSettingsType::WAKE_LOCK_SCREEN ||
         content_settings_type == ContentSettingsType::WAKE_LOCK_SYSTEM);
}

WakeLockPermissionContext::~WakeLockPermissionContext() = default;

ContentSetting WakeLockPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  switch (content_settings_type()) {
    case ContentSettingsType::WAKE_LOCK_SCREEN:
      return CONTENT_SETTING_ALLOW;
    case ContentSettingsType::WAKE_LOCK_SYSTEM:
      return CONTENT_SETTING_BLOCK;
    default:
      NOTREACHED();
  }
}

}  // namespace permissions
