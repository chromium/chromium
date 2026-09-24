// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_

#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_request_data.h"

namespace permissions {

// Delegate which allows embedders to modify the logic of the clipboard
// permission context.
class ClipboardPermissionContextDelegate {
 public:
  virtual ~ClipboardPermissionContextDelegate() = default;

  // Returns true if the permission request was handled, in this case
  // the callback is consumed otherwise not.
  virtual bool DecidePermission(const PermissionRequestData& request_data,
                                BrowserPermissionCallback callback) = 0;

  // Either of these overriding the context's GetPermissionStatusInternal logic
  // means the base context's GetPermissionStatusInternal() is not called.

  // Answers for a frame, which may be inside a webview. A webview grant is not
  // saved to ContentSettings, so the stored status would otherwise still be
  // prompt.
  virtual std::optional<ContentSetting> GetPermissionStatusForWebview(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) const = 0;

  // Answers for a context with no frame, such as a service worker.
  virtual std::optional<ContentSetting> GetPermissionStatusForWorker(
      content::BrowserContext* browser_context,
      const GURL& requesting_origin) const = 0;
};
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_
