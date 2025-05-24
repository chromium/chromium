// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

enum class ReparentingUrl { kAppUrl, kNonAppUrl, kAppUrlLoading };

std::string ToString(const ReparentingUrl& reparenting_url) {
  switch (reparenting_url) {
    case ReparentingUrl::kAppUrl:
      return "kAppUrl";
    case ReparentingUrl::kNonAppUrl:
      return "kNonAppUrl";
    case ReparentingUrl::kAppUrlLoading:
      return "kLoadingUrl";
  }
}

class ReparentWebContentsTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<ReparentingUrl> {
 protected:
  ReparentingUrl GetReparentingUrlType() { return GetParam(); }

  GURL GetNonInstalledUrl() {
    return https_server()->GetURL("/web_apps/no_manifest.html");
  }
  GURL GetInstalledUrl() {
    return https_server()->GetURL("/web_apps/simple/index.html");
  }
  GURL GetReparentingUrl() {
    return GetReparentingUrlType() == ReparentingUrl::kNonAppUrl
               ? GetNonInstalledUrl()
               : GetInstalledUrl();
  }
};

IN_PROC_BROWSER_TEST_P(ReparentWebContentsTest, ReparentToAppAndBack) {
  // Tests reparenting a browser tab into an app window, and back into the
  // browser window.

  webapps::AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), GetInstalledUrl());

  content::WebContents* to_reparent =
      browser()->tab_strip_model()->GetActiveWebContents();

  if (GetReparentingUrlType() == ReparentingUrl::kAppUrlLoading) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GetReparentingUrl(), WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NO_WAIT);
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetReparentingUrl()));
  }

  if (GetReparentingUrlType() == ReparentingUrl::kAppUrl) {
    EXPECT_EQ(app_id, WebAppTabHelper::FromWebContents(to_reparent)->app_id());
  } else {
    EXPECT_EQ(std::nullopt,
              WebAppTabHelper::FromWebContents(to_reparent)->app_id());
  }

  // Create a second tab in the source browser to ensure it doesn't close when
  // we reparent.
  chrome::NewTab(browser());

  // Reparent into the app browser.
  Browser* app_browser;
  {
    app_browser = Browser::Create(Browser::CreateParams::CreateForApp(
        GenerateApplicationNameFromAppId(app_id), true /* trusted_source */,
        gfx::Rect(), profile(), true /* user_gesture */));
    // If the current url isn't in scope, then set the initial url on the
    // AppBrowserController so that the 'x' button still shows up.
    CHECK(app_browser->app_controller());
    app_browser->app_controller()->MaybeSetInitialUrlOnReparentTab();
  }
  ReparentWebContentsIntoBrowserImpl(browser(), to_reparent, app_browser);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
  EXPECT_EQ(app_browser->tab_strip_model()->GetWebContentsAt(0), to_reparent);
  EXPECT_TRUE(WebAppBrowserController::IsForWebApp(app_browser, app_id));

  switch (GetReparentingUrlType()) {
    case ReparentingUrl::kAppUrl:
      EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
      break;
    case ReparentingUrl::kNonAppUrl:
      EXPECT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());
      break;
    case ReparentingUrl::kAppUrlLoading:
      EXPECT_TRUE(app_browser->app_controller()->ShouldShowCustomTabBar());
      content::WaitForLoadStop(to_reparent);
      EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
      break;
  }

  // TODO(crbug.com/371277602): Add testing of session storage state.

  // Reparent back into the browser window, and wait for the app window to
  // close.
  ui_test_utils::BrowserChangeObserver closed_observer(
      app_browser, ui_test_utils::BrowserChangeObserver::ChangeType::kRemoved);
  ReparentWebContentsIntoBrowserImpl(app_browser, to_reparent, browser());
  closed_observer.Wait();
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), to_reparent);

  // TODO(crbug.com/371277602): Add testing of session storage state.
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ReparentWebContentsTest,
    testing::Values(ReparentingUrl::kAppUrl,
                    ReparentingUrl::kNonAppUrl,
                    ReparentingUrl::kAppUrlLoading),
    [](const testing::TestParamInfo<ReparentingUrl>& param_info) {
      return ToString(param_info.param);
    });

}  // namespace
}  // namespace web_app
