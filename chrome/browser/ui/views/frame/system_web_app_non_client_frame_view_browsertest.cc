// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

using SystemWebAppNonClientFrameViewBrowserTest =
    ash::SystemWebAppManagerBrowserTest;

// System Web Apps don't get the web app menu button.
IN_PROC_BROWSER_TEST_P(SystemWebAppNonClientFrameViewBrowserTest,
                       HideWebAppMenuButton) {
  WaitForTestSystemAppInstall();
  Browser* app_browser;
  LaunchApp(ash::SystemWebAppType::SETTINGS, &app_browser);
  EXPECT_EQ(nullptr, BrowserView::GetBrowserViewForBrowser(app_browser)
                         ->web_app_frame_toolbar_for_testing()
                         ->GetAppMenuButton());
}

// Regression test for https://crbug.com/1090169.
IN_PROC_BROWSER_TEST_P(SystemWebAppNonClientFrameViewBrowserTest,
                       HideFileSystemAccessPageAction) {
  WaitForTestSystemAppInstall();
  Browser* app_browser;
  LaunchApp(ash::SystemWebAppType::SETTINGS, &app_browser);
  WebAppFrameToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(app_browser)
          ->web_app_frame_toolbar_for_testing();
  EXPECT_FALSE(
      toolbar->GetPageActionIconView(PageActionIconType::kFileSystemAccess));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppNonClientFrameViewBrowserTest);
