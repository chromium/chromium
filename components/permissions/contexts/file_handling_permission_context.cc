// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/file_handling_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

FileHandlingPermissionContext::FileHandlingPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::FILE_HANDLING,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

FileHandlingPermissionContext::~FileHandlingPermissionContext() = default;

bool FileHandlingPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
