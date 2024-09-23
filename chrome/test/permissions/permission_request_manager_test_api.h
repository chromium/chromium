// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PERMISSIONS_PERMISSION_REQUEST_MANAGER_TEST_API_H_
#define CHROME_TEST_PERMISSIONS_PERMISSION_REQUEST_MANAGER_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"

class Browser;

namespace content {
class RenderFrameHost;
}

namespace views {
class Widget;
}  // namespace views

namespace test {

class PermissionRequestManagerTestApi {
 public:
  explicit PermissionRequestManagerTestApi(
      permissions::PermissionRequestManager* manager);

  // Wraps the PermissionRequestManager for the active tab in |browser|.
  explicit PermissionRequestManagerTestApi(Browser* browser);

  PermissionRequestManagerTestApi(const PermissionRequestManagerTestApi&) =
      delete;
  PermissionRequestManagerTestApi& operator=(
      const PermissionRequestManagerTestApi&) = delete;

  permissions::PermissionRequestManager* manager() { return manager_; }

  // Add a "simple" permission request originating from the given frame. One
  // that uses base PermissionRequest, such as for RequestType kMidiSysex,
  // kNotifications, or kGeolocation.
  void AddSimpleRequest(content::RenderFrameHost* source_frame,
                        permissions::RequestType type);

  void SetOrigin(const GURL& permission_request_origin);

  // Return the Widget for the permission prompt bubble, or nullptr if
  // there is no prompt currently showing.
  views::Widget* GetPromptWindow();

  void SimulateWebContentsDestroyed();

 private:
  raw_ptr<permissions::PermissionRequestManager, AcrossTasksDanglingUntriaged>
      manager_;
  GURL permission_request_origin_ = GURL("https://example.com");
};

}  // namespace test

#endif  // CHROME_TEST_PERMISSIONS_PERMISSION_REQUEST_MANAGER_TEST_API_H_
