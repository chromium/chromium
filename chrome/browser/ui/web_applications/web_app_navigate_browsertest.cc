// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

class WebAppNavigateBrowserTest : public WebAppBrowserTestBase {
 public:
  static GURL GetGoogleURL() { return GURL("http://www.google.com/"); }

  NavigateParams MakeNavigateParams() const {
    NavigateParams params(browser(), GetGoogleURL(), ui::PAGE_TRANSITION_LINK);
    params.window_action = NavigateParams::SHOW_WINDOW;
    return params;
  }
};

namespace {

// Navigate and wait until the browse becomes active.
// Returns the active browser.
Browser* NavigateAndWaitUntilBrowserBecomeActive(NavigateParams* params) {
  Navigate(params);
  ui_test_utils::WaitUntilBrowserBecomeActive(params->browser);
  return params->browser;
}

}  // namespace

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
  InstallPWA(GetGoogleURL());

  Browser* active_browser;
  {
    NavigateParams params(MakeNavigateParams());
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
    params.open_pwa_window_if_possible = true;
    active_browser = NavigateAndWaitUntilBrowserBecomeActive(&params);
  }
  Browser* const app_browser = active_browser;
  const webapps::AppId app_id = app_browser->app_controller()->app_id();

  {
    NavigateParams params(MakeNavigateParams());
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
    params.app_id = app_id;
    active_browser = NavigateAndWaitUntilBrowserBecomeActive(&params);
  }
  content::WebContents* const web_contents =
      active_browser->tab_strip_model()->GetActiveWebContents();

  {
    // From a browser tab, a popup window opens.
    NavigateParams params(MakeNavigateParams());
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    params.source_contents = web_contents;
    active_browser = NavigateAndWaitUntilBrowserBecomeActive(&params);
    EXPECT_FALSE(active_browser->app_controller());
  }

  {
    // From a browser tab, an app window opens if app_id is specified.
    NavigateParams params(MakeNavigateParams());
    params.app_id = app_id;
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    active_browser = NavigateAndWaitUntilBrowserBecomeActive(&params);
    EXPECT_EQ(active_browser->app_controller()->app_id(), app_id);
  }

  {
    // From an app window, another app window opens.
    NavigateParams params(MakeNavigateParams());
    params.browser = app_browser;
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    active_browser = NavigateAndWaitUntilBrowserBecomeActive(&params);
    EXPECT_EQ(active_browser->app_controller()->app_id(), app_id);
  }
}

}  // namespace web_app
