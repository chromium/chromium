// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/clipboard_read_write_permission_context.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace permissions {

ClipboardReadWritePermissionContext::ClipboardReadWritePermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::CLIPBOARD_READ_WRITE,
          blink::mojom::PermissionsPolicyFeature::kClipboardRead) {}

ClipboardReadWritePermissionContext::~ClipboardReadWritePermissionContext() =
    default;

void ClipboardReadWritePermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.global_render_frame_host_id());
  if (!content_settings)
    return;

  if (allowed) {
    content_settings->OnContentAllowed(
        ContentSettingsType::CLIPBOARD_READ_WRITE);
  } else {
    content_settings->OnContentBlocked(
        ContentSettingsType::CLIPBOARD_READ_WRITE);
  }
}

}  // namespace permissions
