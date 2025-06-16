// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/pointer/touch_ui_controller.h"

class WebUITabStripContainerViewBrowserTest : public InProcessBrowserTest {
 public:
  WebUITabStripContainerViewBrowserTest() {
    feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
  }

  ~WebUITabStripContainerViewBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());
  }

  void TearDownOnMainThread() override {
    browser_view_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BrowserView* browser_view() { return browser_view_; }

 private:
  base::test::ScopedFeatureList feature_override_;
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};

 protected:
  raw_ptr<BrowserView> browser_view_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(WebUITabStripContainerViewBrowserTest,
                       TabStripStartsClosed) {
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip(
      browser_view()->browser()));
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip());
  EXPECT_FALSE(browser_view()->webui_tab_strip()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(WebUITabStripContainerViewBrowserTest,
                       TouchModeTransition) {
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip(
      browser_view()->browser()));
  EXPECT_NE(nullptr, browser_view()->webui_tab_strip());
  EXPECT_FALSE(browser_view()->GetTabStripVisible());

  ui::TouchUiController::TouchUiScoperForTesting disable_touch_mode(false);
  EXPECT_FALSE(WebUITabStripContainerView::UseTouchableTabStrip(
      browser_view()->browser()));
  EXPECT_TRUE(browser_view()->GetTabStripVisible());

  ui::TouchUiController::TouchUiScoperForTesting reenable_touch_mode(true);
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip(
      browser_view()->browser()));
  EXPECT_FALSE(browser_view()->GetTabStripVisible());
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip());
}

IN_PROC_BROWSER_TEST_F(WebUITabStripContainerViewBrowserTest,
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

IN_PROC_BROWSER_TEST_F(WebUITabStripContainerViewBrowserTest,
                       CloseContainerOnRendererCrash) {
  auto* webui_tab_strip = browser_view()->webui_tab_strip();
  webui_tab_strip->PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED);
  EXPECT_EQ(false, webui_tab_strip->GetVisible());
  webui_tab_strip->SetVisibleForTesting(true);
  EXPECT_EQ(true, webui_tab_strip->GetVisible());
}

class WebUITabStripDevToolsBrowserTest
    : public WebUITabStripContainerViewBrowserTest {
 public:
  WebUITabStripDevToolsBrowserTest() = default;
  ~WebUITabStripDevToolsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebUITabStripContainerViewBrowserTest::SetUpOnMainThread();
    Browser::CreateParams params =
        Browser::CreateParams::CreateForDevTools(browser()->profile());
    devtools_browser_ = Browser::Create(params);
  }

  void TearDownOnMainThread() override {
    devtools_browser_ = nullptr;
    WebUITabStripContainerViewBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<Browser> devtools_browser_ = nullptr;
};

// Regression test for crbug.com/1010247, crbug.com/1090208.
IN_PROC_BROWSER_TEST_F(WebUITabStripDevToolsBrowserTest,
                       DevToolsWindowHasNoTabStrip) {
  BrowserView* devtools_browser_view =
      BrowserView::GetBrowserViewForBrowser(devtools_browser_);
  ASSERT_TRUE(devtools_browser_view);

  EXPECT_FALSE(
      WebUITabStripContainerView::UseTouchableTabStrip(devtools_browser_));
  EXPECT_EQ(nullptr, devtools_browser_view->webui_tab_strip());

  ui::TouchUiController::TouchUiScoperForTesting disable_touch_mode(false);
  ui::TouchUiController::TouchUiScoperForTesting reenable_touch_mode(true);
  EXPECT_EQ(nullptr, devtools_browser_view->webui_tab_strip());
}

// TODO(crbug.com/40124617): add coverage of open and close gestures.
