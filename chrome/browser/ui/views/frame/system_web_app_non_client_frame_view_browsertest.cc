// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

class SystemWebAppNonClientFrameViewBrowserBase
    : public ash::SystemWebAppManagerBrowserTest {
 public:
  explicit SystemWebAppNonClientFrameViewBrowserBase(
      bool migration_enabled = false)
      : migration_enabled_(migration_enabled) {}

  void HideFileSystemAccessPageAction() {
    WaitForTestSystemAppInstall();
    Browser* app_browser;
    LaunchApp(ash::SystemWebAppType::SETTINGS, &app_browser);
    WebAppFrameToolbarView* toolbar =
        BrowserView::GetBrowserViewForBrowser(app_browser)
            ->web_app_frame_toolbar_for_testing();
    if (migration_enabled_) {
      EXPECT_FALSE(toolbar->GetPageActionView(kActionShowFileSystemAccess));
    } else {
      EXPECT_FALSE(toolbar->GetPageActionIconView(
          PageActionIconType::kFileSystemAccess));
    }
  }

 private:
  bool migration_enabled_;
};
using SystemWebAppNonClientFrameViewBrowserNoMigrationTest =
    SystemWebAppNonClientFrameViewBrowserBase;

// The test that have parametrized testing do not support multiple level
// inheritance. This is the case for
// SystemWebAppNonClientFrameViewBrowserNoMigrationTest as it inherits from
// ::testing::WithParamInterface<TestProfileParam>.
//  Therefore for simplicity there are three different test classes that have
//  been defined :
// - Class 1: SystemWebAppNonClientFrameViewBrowserBase (Base class)
// - Class 2: SystemWebAppNonClientFrameViewBrowserNoMigrationTest (Migration
// disabled)
// - Class 3: SystemWebAppNonClientFrameViewBrowserMigrationTest (Migration
// enabled)
class SystemWebAppNonClientFrameViewBrowserMigrationTest
    : public SystemWebAppNonClientFrameViewBrowserBase {
 public:
  SystemWebAppNonClientFrameViewBrowserMigrationTest()
      : SystemWebAppNonClientFrameViewBrowserBase(/*migration_enabled=*/true) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {::features::kPageActionsMigration,
             {{::features::kPageActionsMigrationFileSystemAccess.name,
               "true"}}},
        },
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// System Web Apps don't get the web app menu button.
IN_PROC_BROWSER_TEST_P(SystemWebAppNonClientFrameViewBrowserNoMigrationTest,
                       HideWebAppMenuButton) {
  WaitForTestSystemAppInstall();
  Browser* app_browser;
  LaunchApp(ash::SystemWebAppType::SETTINGS, &app_browser);
  EXPECT_EQ(nullptr, BrowserView::GetBrowserViewForBrowser(app_browser)
                         ->web_app_frame_toolbar_for_testing()
                         ->GetAppMenuButton());
}

// Regression test for https://crbug.com/1090169.
IN_PROC_BROWSER_TEST_P(SystemWebAppNonClientFrameViewBrowserNoMigrationTest,
                       HideFileSystemAccessPageAction) {
  HideFileSystemAccessPageAction();
}

IN_PROC_BROWSER_TEST_P(SystemWebAppNonClientFrameViewBrowserMigrationTest,
                       HideFileSystemAccessPageActionMigrationEnabled) {
  HideFileSystemAccessPageAction();
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppNonClientFrameViewBrowserNoMigrationTest);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppNonClientFrameViewBrowserMigrationTest);
