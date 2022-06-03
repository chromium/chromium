// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/font_access_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

FontAccessPermissionContext::FontAccessPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::FONT_ACCESS,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

FontAccessPermissionContext::~FontAccessPermissionContext() = default;

bool FontAccessPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
