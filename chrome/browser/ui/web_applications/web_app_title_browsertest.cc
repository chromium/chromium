// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

namespace web_app {

// Test app title scenarios with valid, empty and dynamic app title.
class WebAppTitleBrowserTest : public WebAppControllerBrowserTest {
 public:
  WebAppTitleBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures, "AppTitle");
  }
};

IN_PROC_BROWSER_TEST_F(WebAppTitleBrowserTest, ValidAppTitle) {
  const GURL app_url =
      https_server()->GetURL("/web_apps/page_with_app_title.html");
  const std::u16string app_title = u"A Web App";

  auto web_app_info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(app_url));
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Validate app title has app title.
  EXPECT_EQ(u"A Web App - AppTitle",
            app_browser->GetWindowTitleForCurrentTab(false));
}

IN_PROC_BROWSER_TEST_F(WebAppTitleBrowserTest, WithoutAppTitle) {
  const GURL app_url =
      https_server()->GetURL("/web_apps/page_without_app_title.html");
  const std::u16string app_title = u"A Web App";

  auto web_app_info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(app_url));
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Validate app title is the same as page title.
  EXPECT_EQ(u"A Web App", app_browser->GetWindowTitleForCurrentTab(false));
}

IN_PROC_BROWSER_TEST_F(WebAppTitleBrowserTest, DynamicAppTitle) {
  const GURL app_url =
      https_server()->GetURL("/web_apps/page_without_app_title.html");
  const std::u16string app_title = u"A Web App";

  auto web_app_info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(app_url));
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Validate that app title matches page title.
  EXPECT_EQ(u"A Web App", app_browser->GetWindowTitleForCurrentTab(false));

  {
    // Add app title via script and validate title is updated.
    std::string add_app_title =
        "var meta = document.createElement('meta'); meta.name = 'app-title'; "
        "meta.content = 'AppTitle'; "
        "document.getElementsByTagName('head')[0].appendChild(meta);";
    EXPECT_TRUE(content::ExecJs(web_contents, add_app_title));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    EXPECT_EQ(u"A Web App - AppTitle",
              app_browser->GetWindowTitleForCurrentTab(false));
  }

  {
    // Update app title via script and validate title is updated.
    std::string update_app_title =
        "document.head.getElementsByTagName('meta')['app-title'].content = "
        "'New'";
    EXPECT_TRUE(content::ExecJs(web_contents, update_app_title));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    EXPECT_EQ(u"A Web App - New",
              app_browser->GetWindowTitleForCurrentTab(false));
  }

  {
    // Remove app title via script and validate title is updated.
    std::string remove_app_title =
        "document.head.getElementsByTagName('meta')['app-title'].remove()";
    EXPECT_TRUE(content::ExecJs(web_contents, remove_app_title));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
    EXPECT_EQ(u"A Web App", app_browser->GetWindowTitleForCurrentTab(false));
  }
}

// Navigate to page with and without app title to validate app title is updated.
IN_PROC_BROWSER_TEST_F(WebAppTitleBrowserTest, AppTitleNavigation) {
  const GURL app_url =
      https_server()->GetURL("/web_apps/page_with_app_title.html");
  const std::u16string app_title = u"A Web App";

  auto web_app_info = std::make_unique<WebAppInstallInfo>(
      GenerateManifestIdFromStartUrlOnly(app_url));
  web_app_info->start_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->title = app_title;
  const webapps::AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* const app_browser = LaunchWebAppBrowser(app_id);
  content::WebContents* const web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Validate app title has app title.
  EXPECT_EQ(u"A Web App - AppTitle",
            app_browser->GetWindowTitleForCurrentTab(false));

  // Navigate to page without app title.
  const GURL page_without_url =
      https_server()->GetURL("/web_apps/page_without_app_title.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(app_browser, page_without_url));
  EXPECT_EQ(u"A Web App", app_browser->GetWindowTitleForCurrentTab(false));

  // Navigate to page with app title.
  web_contents->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(u"A Web App - AppTitle",
            app_browser->GetWindowTitleForCurrentTab(false));

  // Navigate again to page without app title.
  web_contents->GetController().GoForward();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(u"A Web App", app_browser->GetWindowTitleForCurrentTab(false));
}
}  // namespace web_app
