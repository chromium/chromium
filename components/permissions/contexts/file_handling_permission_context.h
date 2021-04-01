// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_FILE_HANDLING_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_FILE_HANDLING_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class FileHandlingPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit FileHandlingPermissionContext(
      content::BrowserContext* browser_context);
  ~FileHandlingPermissionContext() override;

  FileHandlingPermissionContext(const FileHandlingPermissionContext&) = delete;
  FileHandlingPermissionContext& operator=(
      const FileHandlingPermissionContext&) = delete;

 protected:
  // PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override;
};

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_FILE_HANDLING_PERMISSION_CONTEXT_H_
