// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/automatic_fullscreen_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

AutomaticFullscreenPermissionContext::AutomaticFullscreenPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::AUTOMATIC_FULLSCREEN,
          blink::mojom::PermissionsPolicyFeature::kFullscreen) {}

}  // namespace permissions
