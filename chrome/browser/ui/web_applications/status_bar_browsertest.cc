// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {
namespace {

using WebAppStatusBarTest = WebAppBrowserTestBase;

IN_PROC_BROWSER_TEST_F(WebAppStatusBarTest, NoStatusBar) {
  NavigateViaLinkClickToURLAndWait(
      browser(), https_server()->GetURL("/web_apps/basic.html"));
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  Browser* const app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(nullptr, app_browser->GetStatusBubbleForTesting());
}

IN_PROC_BROWSER_TEST_F(WebAppStatusBarTest, DisplayBrowserHasStatusBar) {
  NavigateViaLinkClickToURLAndWait(
      browser(), https_server()->GetURL("/web_apps/display_browser.html"));
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  Browser* const app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_NE(nullptr, app_browser->GetStatusBubbleForTesting());
}

IN_PROC_BROWSER_TEST_F(WebAppStatusBarTest, NoManifestHasStatusBar) {
  NavigateViaLinkClickToURLAndWait(
      browser(), https_server()->GetURL("/banners/no_manifest_test_page.html"));
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  Browser* const app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_NE(nullptr, app_browser->GetStatusBubbleForTesting());
}

IN_PROC_BROWSER_TEST_F(WebAppStatusBarTest, DisplayMinimalUiHasStatusBar) {
  NavigateViaLinkClickToURLAndWait(
      browser(), https_server()->GetURL("/web_apps/minimal_ui/basic.html"));
  const webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  Browser* const app_browser =
      ::web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_NE(nullptr, app_browser->GetStatusBubbleForTesting());
}

}  // namespace
}  // namespace web_app
