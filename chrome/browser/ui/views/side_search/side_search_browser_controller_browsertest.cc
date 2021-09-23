// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"

namespace {

constexpr char kGoogleSearchURL[] = "https://www.google.com/search?q=test1";
constexpr char kGoogleSearchHomePageURL[] = "https://www.google.com";
constexpr char kNonGoogleURL[] = "https://www.test.com";

ui::MouseEvent GetDummyEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                        base::TimeTicks::Now(), 0, 0);
}

}  // namespace

// TODO(tluk): Add more tests for the different side panel configurations.
class SideSearchBrowserControllerTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    std::vector<base::Feature> enabled_features;
    scoped_feature_list_.InitWithFeatures(GetEnabledFeatures(), {});
    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  virtual std::vector<base::Feature> GetEnabledFeatures() {
    return {features::kSideSearch};
  }

  void ActivateTabAt(int index) {
    browser()->tab_strip_model()->ActivateTabAt(index);
  }

  void AppendTab(const std::string& url) {
    chrome::AddTabAt(browser(), GURL(url), -1, true);
  }

  void NavigateActiveTab(const std::string& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  }

  void NotifyButtonClick() {
    views::test::ButtonTestApi(side_panel_button())
        .NotifyClick(GetDummyEvent());
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  ToolbarButton* side_panel_button() {
    return browser_view()->toolbar()->left_side_panel_button();
  }

  SidePanel* side_panel() {
    return browser_view()->left_aligned_side_panel_for_testing();
  }

  void NavigateToSRPAndOpenSidePanel() {
    NavigateActiveTab(kGoogleSearchURL);
    EXPECT_FALSE(side_panel_button()->GetVisible());
    EXPECT_FALSE(side_panel()->GetVisible());

    NavigateActiveTab(kNonGoogleURL);
    EXPECT_TRUE(side_panel_button()->GetVisible());
    EXPECT_FALSE(side_panel()->GetVisible());

    NotifyButtonClick();
    EXPECT_TRUE(side_panel_button()->GetVisible());
    EXPECT_TRUE(side_panel()->GetVisible());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlySingleTab) {
  // The side panel button should never be visible on the Google home page.
  NavigateActiveTab(kGoogleSearchHomePageURL);
  EXPECT_FALSE(side_panel_button()->GetVisible());

  // If no previous Google search page has been navigated to the button should
  // not be visible.
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_FALSE(side_panel_button()->GetVisible());

  // The side panel button should never be visible on the Google search page.
  NavigateActiveTab(kGoogleSearchURL);
  EXPECT_FALSE(side_panel_button()->GetVisible());

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_TRUE(side_panel_button()->GetVisible());

  // The side panel button should never be visible on the Google home page even
  // if it has already been navigated to a Google search page.
  NavigateActiveTab(kGoogleSearchHomePageURL);
  EXPECT_FALSE(side_panel_button()->GetVisible());

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_TRUE(side_panel_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlyMultipleTabs) {
  // The side panel button should never be visible on the Google home page.
  AppendTab(kGoogleSearchHomePageURL);
  ActivateTabAt(1);
  EXPECT_FALSE(side_panel_button()->GetVisible());

  // Navigate to a Google search page and then to a non-Google search page. This
  // should show the side panel button in the toolbar.
  AppendTab(kGoogleSearchURL);
  ActivateTabAt(2);
  EXPECT_FALSE(side_panel_button()->GetVisible());
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_TRUE(side_panel_button()->GetVisible());

  // Switch back to the Google search page, the side panel button should no
  // longer be visible.
  ActivateTabAt(1);
  EXPECT_FALSE(side_panel_button()->GetVisible());

  // When switching back to the tab on the non-Google page with a previously
  // visited Google search page the button should be visible.
  ActivateTabAt(2);
  EXPECT_TRUE(side_panel_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelTogglesCorrectlySingleTab) {
  NavigateActiveTab(kGoogleSearchURL);
  EXPECT_FALSE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  // Toggle the side panel.
  NotifyButtonClick();
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_TRUE(side_panel()->GetVisible());

  // Toggling the button again should close the side panel.
  NotifyButtonClick();
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelTogglesCorrectlyMultipleTabs) {
  // Navigate to a Google search URL followed by a non-Google URL in two
  // independent browser tabs such that both have the side panel ready.

  // Tab 1.
  NavigateActiveTab(kGoogleSearchURL);
  EXPECT_FALSE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  // Tab 2.
  AppendTab(kGoogleSearchURL);
  ActivateTabAt(1);
  EXPECT_FALSE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  // Show the side panel on Tab 2 and switch to Tab 1. The side panel should
  // still be visible.
  NotifyButtonClick();
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_TRUE(side_panel()->GetVisible());

  ActivateTabAt(0);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_TRUE(side_panel()->GetVisible());

  // Hide the side panel on Tab 1 and switch to Tab 2. The side panel should be
  // hidden after the tab switch.
  NotifyButtonClick();
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  ActivateTabAt(1);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());
}

class SideSearchStatePerTabBrowserControllerTest
    : public SideSearchBrowserControllerTest {
 public:
  // SideSearchBrowserControllerTest:
  std::vector<base::Feature> GetEnabledFeatures() override {
    auto features = SideSearchBrowserControllerTest::GetEnabledFeatures();
    features.push_back(features::kSideSearchStatePerTab);
    return features;
  }
};

IN_PROC_BROWSER_TEST_F(SideSearchStatePerTabBrowserControllerTest,
                       SidePanelTogglesCorrectlyMultipleTabs) {
  // Navigate to a Google search URL followed by a non-Google URL in two
  // independent browser tabs such that both have the side panel ready. The
  // side panel should respect the state-per-tab flag.

  // Tab 1.
  NavigateActiveTab(kGoogleSearchURL);
  EXPECT_FALSE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  // Tab 2.
  AppendTab(kGoogleSearchURL);
  ActivateTabAt(1);
  EXPECT_FALSE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());
  NavigateActiveTab(kNonGoogleURL);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  // Show the side panel on Tab 2 and switch to Tab 1. The side panel should
  // not be visible for Tab 1.
  NotifyButtonClick();
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_TRUE(side_panel()->GetVisible());

  ActivateTabAt(0);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  // Show the side panel on Tab 1 and switch to Tab 2. The side panel should be
  // still be visible for Tab 2, respecting its per-tab state.
  NotifyButtonClick();
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_TRUE(side_panel()->GetVisible());

  ActivateTabAt(1);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_TRUE(side_panel()->GetVisible());

  // Close the side panel on Tab 2 and switch to Tab 1. The side panel should be
  // still be visible for Tab 1, respecting its per-tab state.
  NotifyButtonClick();
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_FALSE(side_panel()->GetVisible());

  ActivateTabAt(0);
  EXPECT_TRUE(side_panel_button()->GetVisible());
  EXPECT_TRUE(side_panel()->GetVisible());
}
