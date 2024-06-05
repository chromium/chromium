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

void KeyboardLockPermissionContext::NotifyPermissionSet(
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
    // keyboard lock. If another request is made (e.g. after a page navigation),
    // the request must obtain a user permission again.
    persist = false;
  }
  permissions::PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting, is_one_time, is_final_decision);
}

}  // namespace permissions
