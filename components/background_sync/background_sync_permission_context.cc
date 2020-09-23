// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_sync/background_sync_permission_context.h"

#include "base/notreached.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

BackgroundSyncPermissionContext::BackgroundSyncPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::BACKGROUND_SYNC,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

void BackgroundSyncPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize background sync.
  NOTREACHED();
}

bool BackgroundSyncPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
