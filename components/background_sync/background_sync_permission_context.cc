// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_sync/background_sync_permission_context.h"

#include "base/notreached.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

BackgroundSyncPermissionContext::BackgroundSyncPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::BACKGROUND_SYNC,
          network::mojom::PermissionsPolicyFeature::kNotFound) {}

void BackgroundSyncPermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_Data,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize background sync.
  NOTREACHED();
}
