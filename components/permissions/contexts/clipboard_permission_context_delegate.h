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

  // Allows the delegate to override the context's GetPermissionStatusInternal
  // logic. If this returns non empty ContentSetting, the base context's
  // GetPermissionStatusInternal() will not be called.
  virtual std::optional<ContentSetting> GetPermissionStatus(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) const = 0;
};
}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_
