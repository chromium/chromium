// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/test_support/webui_element_test_support.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/test_support/app_menu_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_test_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"
#include "url/gurl.h"

namespace {

// Tests `WebUIElementTestSupport` against an active WebUI toolbar.
class WebUIElementTestSupportBrowserTest : public WebUIToolbarWebViewTestBase {
 public:
  WebUIElementTestSupportBrowserTest()
      : WebUIToolbarWebViewTestBase(
            /*enabled=*/{features::kInitialWebUI, features::kWebUIReloadButton,
                         features::kWebUIHomeButton,
                         features::kWebUIAvatarButton,
                         features::kWebUIAppMenuButton,
                         features::kWebUILocationBar,
                         features::kSkipIPCChannelPausingForNonGuests,
                         features::kWebUIInProcessResourceLoadingV2},
            /*disabled=*/{}) {}

  // Pins the Home button in prefs and waits for its WebUI tracked element.
  ui::TrackedElement* ShowHomeButton() {
    browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                    /*value=*/true);
    return WebUIElementTestSupport::WaitForElementShown(
        browser(), kToolbarHomeButtonElementId);
  }
};

IN_PROC_BROWSER_TEST_F(WebUIElementTestSupportBrowserTest,
                       ResolvesToolbarWebViewAndWebContents) {
  EXPECT_NE(nullptr, WebUIElementTestSupport::GetToolbarWebView(browser()));
  EXPECT_NE(nullptr, WebUIElementTestSupport::GetToolbarWebContents(browser()));
  EXPECT_EQ(nullptr, WebUIElementTestSupport::GetToolbarWebView(
                         /*browser=*/nullptr));
}

IN_PROC_BROWSER_TEST_F(WebUIElementTestSupportBrowserTest,
                       WaitsForElementShownAndHidden) {
  EXPECT_FALSE(WebUIElementTestSupport::IsElementVisible(
      browser(), kToolbarHomeButtonElementId));

  ui::TrackedElement* const shown = ShowHomeButton();
  ASSERT_NE(nullptr, shown);
  EXPECT_NE(nullptr, shown->AsA<ui::TrackedElementWebUI>());

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton,
                                                  /*value=*/false);
  EXPECT_TRUE(WebUIElementTestSupport::WaitForElementHidden(
      browser(), kToolbarHomeButtonElementId));
  EXPECT_FALSE(WebUIElementTestSupport::IsElementVisible(
      browser(), kToolbarHomeButtonElementId));
  EXPECT_FALSE(WebUIElementTestSupport::WaitForElementHidden(
      browser(), ui::ElementIdentifier()));
}

IN_PROC_BROWSER_TEST_F(WebUIElementTestSupportBrowserTest,
                       PressesTrackedElementAndFlushesPipes) {
  ui::TrackedElement* const app_menu_el =
      WebUIElementTestSupport::WaitForElementShown(
          browser(), kToolbarAppMenuButtonElementId);
  ASSERT_NE(nullptr, app_menu_el);

  AppMenuTestAccessor app_menu(browser());
  ASSERT_FALSE(app_menu.IsMenuShowing());

  WebUIElementTestSupport::PressTrackedElement(browser(), app_menu_el);
  EXPECT_TRUE(app_menu.IsMenuShowing());
  app_menu.CloseMenu();
}

IN_PROC_BROWSER_TEST_F(WebUIElementTestSupportBrowserTest,
                       FlushesBrowserControlsPipeOnPress) {
  PrefService* const prefs = browser()->GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, /*value=*/false);
  prefs->SetString(prefs::kHomePage, chrome::kChromeUIVersionURL);
  ui::TrackedElement* const home_el = ShowHomeButton();
  ASSERT_NE(nullptr, home_el);

  WebUIElementTestSupport::PressTrackedElement(browser(), home_el);
  EXPECT_EQ(
      GURL(chrome::kChromeUIVersionURL),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}

}  // namespace
