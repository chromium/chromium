// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/automatic_fullscreen_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace permissions {

AutomaticFullscreenPermissionContext::AutomaticFullscreenPermissionContext(
    content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::AUTOMATIC_FULLSCREEN,
          network::mojom::PermissionsPolicyFeature::kFullscreen) {}

}  // namespace permissions
