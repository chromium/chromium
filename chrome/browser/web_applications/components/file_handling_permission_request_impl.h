// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_REQUEST_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_REQUEST_IMPL_H_

#include <string>
#include "components/permissions/permission_request_impl.h"

namespace content {
class WebContents;
}

namespace permissions {

// Provides a custom message text fragment that differs based on file handlers
// requested by a web app.
class FileHandlingPermissionRequestImpl : public PermissionRequestImpl {
 public:
  FileHandlingPermissionRequestImpl(
      const GURL& request_origin,
      ContentSettingsType content_settings_type,
      bool has_gesture,
      content::WebContents* web_contents,
      PermissionDecidedCallback permission_decided_callback,
      base::OnceClosure delete_callback);
  ~FileHandlingPermissionRequestImpl() override;

 private:
  std::u16string GetMessageTextFragment() const override;
  std::u16string message_text_fragment_;
};
}  // namespace permissions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_FILE_HANDLING_PERMISSION_REQUEST_IMPL_H_
