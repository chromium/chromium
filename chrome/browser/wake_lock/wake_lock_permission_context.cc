// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wake_lock/wake_lock_permission_context.h"

#include "base/logging.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

WakeLockPermissionContext::WakeLockPermissionContext(
    Profile* profile,
    ContentSettingsType content_settings_type)
    : PermissionContextBase(profile,
                            content_settings_type,
                            blink::mojom::FeaturePolicyFeature::kWakeLock),
      content_settings_type_(content_settings_type) {
  DCHECK(content_settings_type == ContentSettingsType::WAKE_LOCK_SCREEN ||
         content_settings_type == ContentSettingsType::WAKE_LOCK_SYSTEM);
}

WakeLockPermissionContext::~WakeLockPermissionContext() {}

ContentSetting WakeLockPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  switch (content_settings_type_) {
    case ContentSettingsType::WAKE_LOCK_SCREEN:
      return CONTENT_SETTING_ALLOW;
    case ContentSettingsType::WAKE_LOCK_SYSTEM:
      return CONTENT_SETTING_BLOCK;
    default:
      NOTREACHED();
      return CONTENT_SETTING_BLOCK;
  }
}

bool WakeLockPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
