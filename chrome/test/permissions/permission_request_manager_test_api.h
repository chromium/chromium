// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PERMISSIONS_PERMISSION_REQUEST_MANAGER_TEST_API_H_
#define CHROME_TEST_PERMISSIONS_PERMISSION_REQUEST_MANAGER_TEST_API_H_

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"

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

  permissions::PermissionRequestManager* manager() { return manager_; }

  // Add a "simple" permission request originating from the given frame. One
  // that uses PermissionRequestImpl, such as for ContentSettingsType including
  // MIDI_SYSEX, PUSH_MESSAGING, NOTIFICATIONS, GEOLOCATON, or PLUGINS.
  void AddSimpleRequest(content::RenderFrameHost* source_frame,
                        ContentSettingsType type);

  // Return the Widget for the permission prompt bubble, or nullptr if
  // there is no prompt currently showing.
  views::Widget* GetPromptWindow();

  void SimulateWebContentsDestroyed();

 private:
  permissions::PermissionRequestManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestManagerTestApi);
};

}  // namespace test

#endif  // CHROME_TEST_PERMISSIONS_PERMISSION_REQUEST_MANAGER_TEST_API_H_
