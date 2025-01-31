// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/local_fonts_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

LocalFontsPermissionContext::LocalFontsPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::LOCAL_FONTS,
          blink::mojom::PermissionsPolicyFeature::kLocalFonts) {}

LocalFontsPermissionContext::~LocalFontsPermissionContext() = default;
