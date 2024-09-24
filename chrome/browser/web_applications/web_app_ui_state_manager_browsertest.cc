// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

namespace web_app {
class WebAppUiStateManagerTest : public WebAppBrowserTestBase {
 public:
  WebAppUiStateManagerTest() = default;
  WebAppUiStateManagerTest(const WebAppUiStateManagerTest&) = delete;
  WebAppUiStateManagerTest& operator=(const WebAppUiStateManagerTest&) = delete;
  ~WebAppUiStateManagerTest() override = default;

  const webapps::AppId InstallWebApp() {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(GetInstallableAppURL());
    web_app_info->title = u"A Web App";
    web_app_info->display_mode = DisplayMode::kStandalone;
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }
};

IN_PROC_BROWSER_TEST_F(WebAppUiStateManagerTest, ReparentIntoWebAppWindow) {
  webapps::AppId app_id = InstallWebApp();

  // Reparent browser web contents into a web app window. Note browser() is
  // opened to a new tab.
  ReparentWebContentsIntoAppBrowser(
      browser()->tab_strip_model()->GetActiveWebContents(), app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppUiStateManagerTest,
                       ReparentIntoWebAppWindowSameScope) {
  webapps::AppId app_id = InstallWebApp();
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GetInstallableAppURL()));

  // Reparent browser web contents into a web app window.
  ReparentWebContentsIntoAppBrowser(
      browser()->tab_strip_model()->GetActiveWebContents(), app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppUiStateManagerTest,
                       ReparentWebAppWindowIntoBrowser) {
  webapps::AppId app_id = InstallWebApp();
  web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
}

}  // namespace web_app
