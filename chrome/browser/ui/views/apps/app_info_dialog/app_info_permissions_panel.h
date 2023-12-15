// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_PANEL_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

class Profile;

namespace extensions {
class Extension;
}

// The summary panel of the app info dialog, which provides basic information
// and controls related to the app.
class AppInfoPermissionsPanel : public AppInfoPanel {
  METADATA_HEADER(AppInfoPermissionsPanel, AppInfoPanel)

 public:
  AppInfoPermissionsPanel(Profile* profile, const extensions::Extension* app);
  AppInfoPermissionsPanel(const AppInfoPermissionsPanel&) = delete;
  AppInfoPermissionsPanel& operator=(const AppInfoPermissionsPanel&) = delete;
  ~AppInfoPermissionsPanel() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsPanelTest,
                           NoPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsPanelTest,
                           RequiredPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsPanelTest,
                           OptionalPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsPanelTest,
                           RetainedFilePermissionsObtainedCorrectly);

  // Called in this order, these methods set-up, add permissions to, and layout
  // the list of permissions.
  void CreatePermissionsList();
  void FillPermissionsList();
  void LayoutPermissionsList();

  bool HasActivePermissionMessages() const;
  extensions::PermissionMessages GetActivePermissionMessages() const;

  int GetRetainedFileCount() const;
  std::u16string GetRetainedFileHeading() const;
  std::vector<std::u16string> GetRetainedFilePaths() const;
  void RevokeFilePermissions();

  int GetRetainedDeviceCount() const;
  std::u16string GetRetainedDeviceHeading() const;
  std::vector<std::u16string> GetRetainedDevices() const;
  void RevokeDevicePermissions();
};

BEGIN_VIEW_BUILDER(/* no export */, AppInfoPermissionsPanel, AppInfoPanel)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, AppInfoPermissionsPanel)

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_PANEL_H_
