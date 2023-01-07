// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_SANITIZED_WRITE_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_SANITIZED_WRITE_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

// Manages Clipboard API user permissions, for sanitized write only.
class ClipboardSanitizedWritePermissionContext : public PermissionContextBase {
 public:
  explicit ClipboardSanitizedWritePermissionContext(
      content::BrowserContext* browser_context);
  ~ClipboardSanitizedWritePermissionContext() override;

  ClipboardSanitizedWritePermissionContext(
      const ClipboardSanitizedWritePermissionContext&) = delete;
  ClipboardSanitizedWritePermissionContext& operator=(
      const ClipboardSanitizedWritePermissionContext&) = delete;

 private:
  // PermissionContextBase:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_SANITIZED_WRITE_PERMISSION_CONTEXT_H_
