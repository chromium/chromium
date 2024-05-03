// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kWebUIScheme[] = "chrome://";

}  // namespace

namespace web_app {

class WebUIWebAppBrowserTest : public WebAppBrowserTestBase {
 public:
  WebUIWebAppBrowserTest() = default;
  ~WebUIWebAppBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    WebAppBrowserTestBase::SetUp();
  }

  struct App {
    webapps::AppId id;
    std::string start_url;
    raw_ptr<Browser> browser;
    raw_ptr<BrowserView> browser_view;
    raw_ptr<content::WebContents> web_contents;
  };

  App InstallAndLaunch() {
    Profile* profile = browser()->profile();
    std::string start_url = base::StrCat(
        {kWebUIScheme, password_manager::kChromeUIPasswordManagerHost});

    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(start_url));
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    webapps::AppId app_id =
        test::InstallWebApp(profile, std::move(web_app_info));

    Browser* app_browser = ::web_app::LaunchWebAppBrowser(profile, app_id);
    return App{app_id, start_url, app_browser,
               BrowserView::GetBrowserViewForBrowser(app_browser),
               app_browser->tab_strip_model()->GetActiveWebContents()};
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(WebUIWebAppBrowserTest, NavigationsToOtherWebUIs) {
  App app = InstallAndLaunch();

  // Navigation to another page within the app scope should not open new browser
  // tabs.
  GURL in_scope_url = GURL(base::StrCat({app.start_url, "/settings"}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app.browser, in_scope_url));
  EXPECT_EQ(app.web_contents->GetVisibleURL(), in_scope_url);

  // Navigation to another WebUI should open a new browser tab.
  GURL out_of_scope_url = GURL(base::StrCat({kWebUIScheme, "history"}));
  // Add a waiter for the app to open.
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app.browser, out_of_scope_url));
  content::WebContents* new_web_contents = waiter.Wait();
  ASSERT_TRUE(new_web_contents);

  // URL of the WebApp should not change.
  EXPECT_EQ(app.web_contents->GetVisibleURL(), in_scope_url);

  // Check that new web contents belong to the same profile.
  Browser* new_browser_window = chrome::FindTabbedBrowser(
      app.browser->profile(), /*match_original_profiles=*/true);
  EXPECT_EQ(new_web_contents,
            new_browser_window->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(new_web_contents->GetVisibleURL(), out_of_scope_url);
}

}  // namespace web_app
