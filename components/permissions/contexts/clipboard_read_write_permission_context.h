// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/contexts/clipboard_permission_context_delegate.h"
#include "components/permissions/permission_request_data.h"

namespace permissions {

// Manages Clipboard API user permissions, including unsanitized read and write,
// as well as sanitized read.
class ClipboardReadWritePermissionContext
    : public ContentSettingPermissionContextBase {
 public:
  explicit ClipboardReadWritePermissionContext(
      content::BrowserContext* browser_context,
      std::unique_ptr<ClipboardPermissionContextDelegate> delegate);
  ~ClipboardReadWritePermissionContext() override;

  ClipboardReadWritePermissionContext(
      const ClipboardReadWritePermissionContext&) = delete;
  ClipboardReadWritePermissionContext& operator=(
      const ClipboardReadWritePermissionContext&) = delete;

 private:
  // PermissionContextBase:
  void DecidePermission(std::unique_ptr<PermissionRequestData> request_data,
                        BrowserPermissionCallback callback) override;

  // PermissionContextBase:
  void UpdateTabContext(const PermissionRequestData& request_data,
                        bool allowed) override;

  // ContentSettingPermissionContextBase:
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  std::unique_ptr<ClipboardPermissionContextDelegate> delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_
