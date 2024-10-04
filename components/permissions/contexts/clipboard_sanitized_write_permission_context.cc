// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/clipboard_sanitized_write_permission_context.h"

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/gurl.h"

namespace permissions {

ClipboardSanitizedWritePermissionContext::
    ClipboardSanitizedWritePermissionContext(
        content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::CLIPBOARD_SANITIZED_WRITE,
          blink::mojom::PermissionsPolicyFeature::kClipboardWrite) {}

ClipboardSanitizedWritePermissionContext::
    ~ClipboardSanitizedWritePermissionContext() = default;

ContentSetting
ClipboardSanitizedWritePermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_ALLOW;
}

}  // namespace permissions
