// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#include <utility>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "content/public/common/drop_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/pointer/touch_ui_controller.h"

class WebUITabStripContainerViewTest : public TestWithBrowserView {
 public:
  template <typename... Args>
  explicit WebUITabStripContainerViewTest(Args... args)
      : TestWithBrowserView(args...) {
    feature_override_.InitAndEnableFeature(features::kWebUITabStrip);
  }

  ~WebUITabStripContainerViewTest() override = default;

 private:
  base::test::ScopedFeatureList feature_override_;
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
};

TEST_F(WebUITabStripContainerViewTest, TabStripStartsClosed) {
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip(
      browser_view()->browser()));
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip());
  EXPECT_FALSE(browser_view()->webui_tab_strip()->GetVisible());
}

TEST_F(WebUITabStripContainerViewTest, TouchModeTransition) {
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

TEST_F(WebUITabStripContainerViewTest, ButtonsPresentInToolbar) {
  ASSERT_NE(nullptr, browser_view()->toolbar()->new_tab_button_for_testing());
  EXPECT_TRUE(browser_view()->toolbar()->Contains(
      browser_view()->toolbar()->new_tab_button_for_testing()));
  EXPECT_TRUE(
      browser_view()->toolbar()->new_tab_button_for_testing()->GetVisible());
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip()->tab_counter());
  EXPECT_TRUE(browser_view()->toolbar()->Contains(
      browser_view()->webui_tab_strip()->tab_counter()));
}

TEST_F(WebUITabStripContainerViewTest, CloseContainerOnRendererCrash) {
  auto* webui_tab_strip = browser_view()->webui_tab_strip();
  webui_tab_strip->PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED);
  EXPECT_EQ(false, webui_tab_strip->GetVisible());
  webui_tab_strip->SetVisibleForTesting(true);
  EXPECT_EQ(true, webui_tab_strip->GetVisible());
}

class WebUITabStripDevToolsTest : public WebUITabStripContainerViewTest {
 public:
  WebUITabStripDevToolsTest()
      : WebUITabStripContainerViewTest(Browser::TYPE_DEVTOOLS) {}
  ~WebUITabStripDevToolsTest() override = default;
};

// Regression test for crbug.com/1010247, crbug.com/1090208.
TEST_F(WebUITabStripDevToolsTest, DevToolsWindowHasNoTabStrip) {
  EXPECT_FALSE(WebUITabStripContainerView::UseTouchableTabStrip(
      browser_view()->browser()));
  EXPECT_EQ(nullptr, browser_view()->webui_tab_strip());

  ui::TouchUiController::TouchUiScoperForTesting disable_touch_mode(false);
  ui::TouchUiController::TouchUiScoperForTesting reenable_touch_mode(true);
  EXPECT_EQ(nullptr, browser_view()->webui_tab_strip());
}

// TODO(crbug.com/40124617): add coverage of open and close gestures.
