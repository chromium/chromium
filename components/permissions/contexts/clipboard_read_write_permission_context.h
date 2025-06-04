// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

// Manages Clipboard API user permissions, including unsanitized read and write,
// as well as sanitized read.
class ClipboardReadWritePermissionContext : public PermissionContextBase {
 public:
  explicit ClipboardReadWritePermissionContext(
      content::BrowserContext* browser_context);
  ~ClipboardReadWritePermissionContext() override;

  ClipboardReadWritePermissionContext(
      const ClipboardReadWritePermissionContext&) = delete;
  ClipboardReadWritePermissionContext& operator=(
      const ClipboardReadWritePermissionContext&) = delete;

 private:
  // PermissionContextBase:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_CLIPBOARD_READ_WRITE_PERMISSION_CONTEXT_H_
