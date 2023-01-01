// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/web_app_helpers.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

class WebAppNavigateBrowserTest : public WebAppControllerBrowserTest {
 public:
  static GURL GetGoogleURL() { return GURL("http://www.google.com/"); }

  NavigateParams MakeNavigateParams() const {
    NavigateParams params(browser(), GetGoogleURL(), ui::PAGE_TRANSITION_LINK);
    params.window_action = NavigateParams::SHOW_WINDOW;
    return params;
  }
};

// This test verifies that navigating with "open_pwa_window_if_possible = true"
// opens a new app window if there is an installed Web App for the URL.
IN_PROC_BROWSER_TEST_F(WebAppNavigateBrowserTest,
                       AppInstalled_OpenAppWindowIfPossible_True) {
  InstallPWA(GetGoogleURL());

  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.open_pwa_window_if_possible = true;
  Navigate(&params);

  EXPECT_NE(browser(), params.browser);
  EXPECT_FALSE(params.browser->is_type_normal());
  EXPECT_TRUE(params.browser->is_type_app());
  EXPECT_TRUE(params.browser->is_trusted_source());
}

// This test verifies that navigating with "open_pwa_window_if_possible = false"
// opens a new foreground tab even if there is an installed Web App for the
// URL.
IN_PROC_BROWSER_TEST_F(WebAppNavigateBrowserTest,
                       AppInstalled_OpenAppWindowIfPossible_False) {
  InstallPWA(GetGoogleURL());

  int num_tabs = browser()->tab_strip_model()->count();

  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.open_pwa_window_if_possible = false;
  Navigate(&params);

  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());
}

// This test verifies that navigating with "open_pwa_window_if_possible = true"
// opens a new foreground tab when there is no app installed for the URL.
IN_PROC_BROWSER_TEST_F(WebAppNavigateBrowserTest,
                       NoAppInstalled_OpenAppWindowIfPossible) {
  int num_tabs = browser()->tab_strip_model()->count();

  NavigateParams params(MakeNavigateParams());
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.open_pwa_window_if_possible = true;
  Navigate(&params);

  EXPECT_EQ(browser(), params.browser);
  EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(WebAppNavigateBrowserTest, NewPopup) {
  BrowserList* const browser_list = BrowserList::GetInstance();
  InstallPWA(GetGoogleURL());

  {
    NavigateParams params(MakeNavigateParams());
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
    params.open_pwa_window_if_possible = true;
    Navigate(&params);
  }
  Browser* const app_browser = browser_list->GetLastActive();
  const AppId app_id = app_browser->app_controller()->app_id();

  {
    NavigateParams params(MakeNavigateParams());
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
    params.app_id = app_id;
    Navigate(&params);
  }
  content::WebContents* const web_contents =
      browser_list->GetLastActive()->tab_strip_model()->GetActiveWebContents();

  {
    // From a browser tab, a popup window opens.
    NavigateParams params(MakeNavigateParams());
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.source_contents = web_contents;
    Navigate(&params);
    EXPECT_FALSE(browser_list->GetLastActive()->app_controller());
  }

  {
    // From a browser tab, an app window opens if app_id is specified.
    NavigateParams params(MakeNavigateParams());
    params.app_id = app_id;
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    Navigate(&params);
    EXPECT_EQ(browser_list->GetLastActive()->app_controller()->app_id(),
              app_id);
  }

  {
    // From an app window, another app window opens.
    NavigateParams params(MakeNavigateParams());
    params.browser = app_browser;
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    Navigate(&params);
    EXPECT_EQ(browser_list->GetLastActive()->app_controller()->app_id(),
              app_id);
  }
}

class WebAppNavigatePrerenderingBrowserTest : public WebAppNavigateBrowserTest {
 public:
  WebAppNavigatePrerenderingBrowserTest()
      : app_browser_(browser()),
        prerender_helper_(base::BindRepeating(
            &WebAppNavigatePrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}

  ~WebAppNavigatePrerenderingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  content::WebContents* GetWebContents() const {
    return app_browser_->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }
  void set_app_browser(Browser* browser) { app_browser_ = browser; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  raw_ptr<Browser, DanglingUntriaged> app_browser_ = nullptr;
  content::test::PrerenderTestHelper prerender_helper_;
  base::HistogramTester histogram_tester_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

// Tests that prerendering doesn't change the existing App ID. It also doesn't
// call ManifestUpdateManager as a primary page is not changed.
IN_PROC_BROWSER_TEST_F(WebAppNavigatePrerenderingBrowserTest,
                       NotUpdateInPrerendering) {
  const GURL example_url = embedded_test_server()->GetURL("/simple.html");

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = example_url;
  web_app_info->scope = example_url;
  web_app_info->user_display_mode = absl::make_optional<mojom::UserDisplayMode>(
      mojom::UserDisplayMode::kStandalone);
  AppId app_id = InstallWebApp(std::move(web_app_info));

  Browser* app_browser = LaunchWebAppBrowser(app_id);
  set_app_browser(app_browser);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, example_url));

  constexpr char kUpdateHistogramName[] = "Webapp.Update.ManifestUpdateResult";
  histogram_tester().ExpectTotalCount(kUpdateHistogramName, 2);

  content::WebContents* web_contents = GetWebContents();
  const AppId* first_app_id = WebAppTabHelper::GetAppId(web_contents);
  EXPECT_EQ(app_id, *first_app_id);

  const GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  int host_id = prerender_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents, host_id);
  // Prerendering doesn't update the existing App ID.
  const AppId* app_id_on_prerendering = WebAppTabHelper::GetAppId(web_contents);
  EXPECT_EQ(app_id, *app_id_on_prerendering);

  // In prerendering navigation, it doesn't call ManifestUpdateManager.
  // The total count of the histogram doesn't increase.
  histogram_tester().ExpectTotalCount(kUpdateHistogramName, 2);

  prerender_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  const AppId* app_id_after_activation =
      WebAppTabHelper::GetAppId(web_contents);
  EXPECT_EQ(nullptr, app_id_after_activation);
}

}  // namespace web_app
