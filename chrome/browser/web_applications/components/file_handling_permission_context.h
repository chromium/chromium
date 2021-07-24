// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_CONTEXT_H_

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
  void NotifyPermissionSet(const permissions::PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           permissions::BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time) override;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_CONTEXT_H_
