// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace content {
class WebContents;
}  // namespace content

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
  std::unique_ptr<permissions::PermissionRequest> CreatePermissionRequest(
      const GURL& request_origin,
      ContentSettingsType content_settings_type,
      bool has_gesture,
      content::WebContents* web_contents,
      permissions::PermissionRequestImpl::PermissionDecidedCallback
          permission_decided_callback,
      base::OnceClosure delete_callback) const override;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_CONTEXT_H_
