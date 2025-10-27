// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/base/pointer/touch_ui_controller.h"

class WebUITabStripContainerViewTest : public InProcessBrowserTest {
 public:
  WebUITabStripContainerViewTest() {
    feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
  }
  ~WebUITabStripContainerViewTest() override = default;

 protected:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList feature_override_;
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
};

IN_PROC_BROWSER_TEST_F(WebUITabStripContainerViewTest, TabStripStartsClosed) {
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip(browser()));
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip());
  EXPECT_FALSE(browser_view()->webui_tab_strip()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebUITabStripContainerViewTest, TouchModeTransition) {
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip(browser()));
  EXPECT_NE(nullptr, browser_view()->webui_tab_strip());
  EXPECT_FALSE(browser_view()->GetTabStripVisible());

  ui::TouchUiController::TouchUiScoperForTesting disable_touch_mode(false);
  EXPECT_FALSE(WebUITabStripContainerView::UseTouchableTabStrip(browser()));
  EXPECT_TRUE(browser_view()->GetTabStripVisible());

  ui::TouchUiController::TouchUiScoperForTesting reenable_touch_mode(true);
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip(browser()));
  EXPECT_FALSE(browser_view()->GetTabStripVisible());
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip());
}

IN_PROC_BROWSER_TEST_F(WebUITabStripContainerViewTest,
                       ButtonsPresentInToolbar) {
  ASSERT_NE(nullptr, browser_view()->toolbar()->new_tab_button_for_testing());
  EXPECT_TRUE(browser_view()->toolbar()->Contains(
      browser_view()->toolbar()->new_tab_button_for_testing()));
  EXPECT_TRUE(
      browser_view()->toolbar()->new_tab_button_for_testing()->GetVisible());
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip()->tab_counter());
  EXPECT_TRUE(browser_view()->toolbar()->Contains(
      browser_view()->webui_tab_strip()->tab_counter()));
}

IN_PROC_BROWSER_TEST_F(WebUITabStripContainerViewTest,
                       CloseContainerOnRendererCrash) {
  auto* webui_tab_strip = browser_view()->webui_tab_strip();
  webui_tab_strip->PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED);
  EXPECT_EQ(false, webui_tab_strip->GetVisible());
  webui_tab_strip->SetVisibleForTesting(true);
  EXPECT_EQ(true, webui_tab_strip->GetVisible());
}

using WebUITabStripDevToolsTest = WebUITabStripContainerViewTest;

// Regression test for crbug.com/1010247, crbug.com/1090208.
IN_PROC_BROWSER_TEST_F(WebUITabStripDevToolsTest, DevToolsWindowHasNoTabStrip) {
  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);

  auto devtools_browsers =
      ui_test_utils::FindMatchingBrowsers([](BrowserWindowInterface* browser) {
        return browser->GetType() == BrowserWindowInterface::TYPE_DEVTOOLS;
      });
  ASSERT_EQ(1u, devtools_browsers.size());
  Browser* dev_tools_browser =
      devtools_browsers[0]->GetBrowserForMigrationOnly();

  EXPECT_FALSE(
      WebUITabStripContainerView::UseTouchableTabStrip(dev_tools_browser));
  EXPECT_EQ(nullptr, dev_tools_browser->GetBrowserView().webui_tab_strip());

  ui::TouchUiController::TouchUiScoperForTesting disable_touch_mode(false);
  ui::TouchUiController::TouchUiScoperForTesting reenable_touch_mode(true);
  EXPECT_EQ(nullptr, dev_tools_browser->GetBrowserView().webui_tab_strip());

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
}

// TODO(crbug.com/40124617): add coverage of open and close gestures.
