// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#endif

using SystemWebAppNonClientFrameViewBrowserTest =
    web_app::SystemWebAppManagerBrowserTest;

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// System Web Apps don't get the web app menu button.
IN_PROC_BROWSER_TEST_P(SystemWebAppNonClientFrameViewBrowserTest,
                       HideWebAppMenuButton) {
  WaitForTestSystemAppInstall();
  Browser* app_browser;
  LaunchApp(ash::SystemWebAppType::SETTINGS, &app_browser);
  EXPECT_EQ(nullptr, BrowserView::GetBrowserViewForBrowser(app_browser)
                         ->frame()
                         ->GetFrameView()
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
          ->frame()
          ->GetFrameView()
          ->web_app_frame_toolbar_for_testing();
  EXPECT_FALSE(
      toolbar->GetPageActionIconView(PageActionIconType::kFileSystemAccess));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    SystemWebAppNonClientFrameViewBrowserTest);
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
