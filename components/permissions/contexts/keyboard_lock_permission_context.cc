// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/keyboard_lock_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

KeyboardLockPermissionContext::KeyboardLockPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::KEYBOARD_LOCK,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

}  // namespace permissions
