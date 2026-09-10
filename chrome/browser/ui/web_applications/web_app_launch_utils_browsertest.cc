// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
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
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

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
    return embedded_https_test_server().GetURL("/web_apps/no_manifest.html");
  }
  GURL GetInstalledUrl() {
    return embedded_https_test_server().GetURL("/web_apps/simple/index.html");
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
      InstallWebAppInNewTabAndClose(browser(), GetInstalledUrl());

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
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  // Reparent into the app browser.
  BrowserWindowInterface* app_browser;
  {
    app_browser = CreateBrowserWindow(BrowserWindowCreateParams::CreateForApp(
        GenerateApplicationNameFromAppId(app_id),
        /*trusted_source=*/true, gfx::Rect(), profile(),
        /*user_gesture=*/true));
    // If the current url isn't in scope, then set the initial url on the
    // AppBrowserController so that the 'x' button still shows up.
    CHECK(web_app::AppBrowserController::From(app_browser));
    web_app::AppBrowserController::From(app_browser)
        ->MaybeSetInitialUrlOnReparentTab();
  }
  ReparentWebContentsIntoBrowserImpl(browser(), to_reparent, app_browser);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, app_browser->GetTabStripModel()->count());
  EXPECT_EQ(app_browser->GetTabStripModel()->GetWebContentsAt(0), to_reparent);
  EXPECT_TRUE(WebAppBrowserController::IsForWebApp(app_browser, app_id));

  switch (GetReparentingUrlType()) {
    case ReparentingUrl::kAppUrl:
      EXPECT_FALSE(web_app::AppBrowserController::From(app_browser)
                       ->ShouldShowCustomTabBar());
      break;
    case ReparentingUrl::kNonAppUrl:
      EXPECT_TRUE(web_app::AppBrowserController::From(app_browser)
                      ->ShouldShowCustomTabBar());
      break;
    case ReparentingUrl::kAppUrlLoading:
      EXPECT_TRUE(web_app::AppBrowserController::From(app_browser)
                      ->ShouldShowCustomTabBar());
      content::WaitForLoadStop(to_reparent);
      EXPECT_FALSE(web_app::AppBrowserController::From(app_browser)
                       ->ShouldShowCustomTabBar());
      break;
  }

  // TODO(crbug.com/371277602): Add testing of session storage state.

  // Reparent back into the browser window, and wait for the app window to
  // close.
  ui_test_utils::BrowserDestroyedObserver browser_destroyed_observer(
      app_browser);
  ReparentWebContentsIntoBrowserImpl(app_browser, to_reparent, browser());
  browser_destroyed_observer.Wait();
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

IN_PROC_BROWSER_TEST_F(WebAppBrowserTestBase,
                       ReparentWebContentsWindowClosedOnShow) {
  const GURL app_url =
      embedded_https_test_server().GetURL("/web_apps/simple/index.html");
  webapps::AppId app_id = InstallWebAppInNewTabAndClose(browser(), app_url);

  content::WebContents* to_reparent =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  // Create a second tab so browser() remains open after detaching to_reparent.
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  BrowserWindowInterface* app_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForApp(
          GenerateApplicationNameFromAppId(app_id),
          /*trusted_source=*/true, gfx::Rect(), profile(),
          /*user_gesture=*/true));

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);
  ASSERT_TRUE(browser_view);
  views::Widget* target_widget = browser_view->GetWidget();
  ASSERT_TRUE(target_widget);

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  observer.set_shown_callback(base::BindRepeating(
      [](views::Widget* target_widget, views::Widget* shown_widget) {
        if (shown_widget == target_widget) {
          shown_widget->CloseNow();
        }
      },
      target_widget));

  ui_test_utils::BrowserDestroyedObserver browser_destroyed_observer(
      app_browser);
  ReparentWebContentsIntoBrowserImpl(browser(), to_reparent, app_browser);
  browser_destroyed_observer.Wait();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(WebAppBrowserTestBase,
                       ReparentWebContentsIntoAppBrowserWindowClosedOnShow) {
  const GURL app_url =
      embedded_https_test_server().GetURL("/web_apps/simple/index.html");
  webapps::AppId app_id = InstallWebAppInNewTabAndClose(browser(), app_url);

  content::WebContents* to_reparent =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  // Create a second tab so browser() remains open after detaching to_reparent.
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  bool closed_app_widget = false;
  observer.set_shown_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (!closed_app_widget) {
          BrowserView* browser_view =
              BrowserView::GetBrowserViewForNativeWindow(
                  widget->GetNativeWindow());
          if (browser_view && browser_view->browser()->GetType() ==
                                  BrowserWindowInterface::Type::TYPE_APP) {
            closed_app_widget = true;
            widget->CloseNow();
          }
        }
      }));

  BrowserWindowInterface* result =
      ReparentWebContentsIntoAppBrowser(to_reparent, app_id);
  EXPECT_TRUE(closed_app_widget);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

}  // namespace
}  // namespace web_app
