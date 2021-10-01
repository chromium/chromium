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
    scoped_feature_list_.InitWithFeatures(GetEnabledFeatures(), {});
    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
    SetIsSidePanelSRPAvailableAt(browser(), 0, true);
  }

  virtual std::vector<base::Feature> GetEnabledFeatures() {
    return {features::kSideSearch};
  }

  void ActivateTabAt(Browser* browser, int index) {
    browser->tab_strip_model()->ActivateTabAt(index);
  }

  void AppendTab(Browser* browser, const std::string& url) {
    chrome::AddTabAt(browser, GURL(url), -1, true);
    SetIsSidePanelSRPAvailableAt(
        browser, browser->tab_strip_model()->GetTabCount() - 1, true);
  }

  void NavigateActiveTab(Browser* browser, const std::string& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, GURL(url)));
  }

  void NotifyButtonClick(Browser* browser) {
    views::test::ButtonTestApi(GetSidePanelButtonFor(browser))
        .NotifyClick(GetDummyEvent());
  }

  void NotifyCloseButtonClick(Browser* browser) {
    ASSERT_TRUE(GetSidePanelFor(browser)->GetVisible());
    views::test::ButtonTestApi(GetSideButtonClosePanelFor(browser))
        .NotifyClick(GetDummyEvent());
  }

  void SetIsSidePanelSRPAvailableAt(Browser* browser,
                                    int index,
                                    bool is_available) {
    auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
        browser->tab_strip_model()->GetWebContentsAt(index));
    tab_contents_helper->SetIsSidePanelSRPAvailableForTesting(is_available);
  }

  BrowserView* BrowserViewFor(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser);
  }

  ToolbarButton* GetSidePanelButtonFor(Browser* browser) {
    return BrowserViewFor(browser)->toolbar()->left_side_panel_button();
  }

  views::ImageButton* GetSideButtonClosePanelFor(Browser* browser) {
    return static_cast<views::ImageButton*>(
        GetSidePanelFor(browser)->GetViewByID(static_cast<int>(
            SideSearchBrowserController::VIEW_ID_SIDE_PANEL_CLOSE_BUTTON)));
  }

  SidePanel* GetSidePanelFor(Browser* browser) {
    return BrowserViewFor(browser)->left_aligned_side_panel_for_testing();
  }

  void NavigateToSRPAndNonGoogleUrl(Browser* browser) {
    // The side panel button should never be visible on the Google search page.
    NavigateActiveTab(browser, kGoogleSearchURL);
    EXPECT_FALSE(GetSidePanelButtonFor(browser)->GetVisible());
    EXPECT_FALSE(GetSidePanelFor(browser)->GetVisible());

    // The side panel button should be visible if on a non-Google page and the
    // current tab has previously encountered a Google search page.
    NavigateActiveTab(browser, kNonGoogleURL);
    EXPECT_TRUE(GetSidePanelButtonFor(browser)->GetVisible());
    EXPECT_FALSE(GetSidePanelFor(browser)->GetVisible());
  }

  void NavigateToSRPAndOpenSidePanel(Browser* browser) {
    NavigateToSRPAndNonGoogleUrl(browser);

    NotifyButtonClick(browser);
    EXPECT_TRUE(GetSidePanelButtonFor(browser)->GetVisible());
    EXPECT_TRUE(GetSidePanelFor(browser)->GetVisible());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlySingleTab) {
  // The side panel button should never be visible on the Google home page.
  NavigateActiveTab(browser(), kGoogleSearchHomePageURL);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // If no previous Google search page has been navigated to the button should
  // not be visible.
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // The side panel button should never be visible on the Google search page.
  NavigateActiveTab(browser(), kGoogleSearchURL);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());

  // The side panel button should never be visible on the Google home page even
  // if it has already been navigated to a Google search page.
  NavigateActiveTab(browser(), kGoogleSearchHomePageURL);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlyMultipleTabs) {
  // The side panel button should never be visible on the Google home page.
  AppendTab(browser(), kGoogleSearchHomePageURL);
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // Navigate to a Google search page and then to a non-Google search page. This
  // should show the side panel button in the toolbar.
  AppendTab(browser(), kGoogleSearchURL);
  ActivateTabAt(browser(), 2);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());

  // Switch back to the Google search page, the side panel button should no
  // longer be visible.
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // When switching back to the tab on the non-Google page with a previously
  // visited Google search page the button should be visible.
  ActivateTabAt(browser(), 2);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelTogglesCorrectlySingleTab) {
  NavigateActiveTab(browser(), kGoogleSearchURL);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Toggle the side panel.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  // Toggling the button again should close the side panel.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelTogglesCorrectlyMultipleTabs) {
  // Navigate to a Google search URL followed by a non-Google URL in two
  // independent browser tabs such that both have the side panel ready.

  // Tab 1.
  NavigateActiveTab(browser(), kGoogleSearchURL);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Tab 2.
  AppendTab(browser(), kGoogleSearchURL);
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Show the side panel on Tab 2 and switch to Tab 1. The side panel should
  // still be visible.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  // Hide the side panel on Tab 1 and switch to Tab 2. The side panel should be
  // hidden after the tab switch.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 1);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       CloseButtonClosesSidePanel) {
  // The close button should be visible in the toggled state.
  NavigateToSRPAndOpenSidePanel(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  NotifyCloseButtonClick(browser());
}

IN_PROC_BROWSER_TEST_F(
    SideSearchBrowserControllerTest,
    SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows) {
  NavigateToSRPAndOpenSidePanel(browser());

  Browser* browser2 = CreateBrowser(browser()->profile());
  NavigateToSRPAndNonGoogleUrl(browser2);

  std::unique_ptr<content::WebContents> web_contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  browser()->tab_strip_model()->InsertWebContentsAt(1, std::move(web_contents),
                                                    TabStripModel::ADD_ACTIVE);

  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SideSearchNotAvailableInOTR) {
  Browser* browser2 = CreateIncognitoBrowser();
  EXPECT_TRUE(browser2->profile()->IsOffTheRecord());
  NavigateActiveTab(browser2, kGoogleSearchURL);
  NavigateActiveTab(browser2, kNonGoogleURL);

  EXPECT_EQ(nullptr, GetSidePanelButtonFor(browser2));
  EXPECT_EQ(nullptr, GetSidePanelFor(browser2));
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelButtonIsNotShownWhenSRPIsUnavailable) {
  // Set the side panel SRP be unavailable.
  SetIsSidePanelSRPAvailableAt(browser(), 0, false);

  // The side panel button should never be visible on the Google home page.
  NavigateActiveTab(browser(), kGoogleSearchHomePageURL);
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // If no previous Google search page has been navigated to the button should
  // not be visible.
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should never be visible on the Google search page.
  NavigateActiveTab(browser(), kGoogleSearchURL);
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should not be visible if the side panel SRP is not
  // available.
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
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

IN_PROC_BROWSER_TEST_F(
    SideSearchStatePerTabBrowserControllerTest,
    SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows) {
  NavigateToSRPAndOpenSidePanel(browser());

  Browser* browser2 = CreateBrowser(browser()->profile());
  NavigateToSRPAndNonGoogleUrl(browser2);

  std::unique_ptr<content::WebContents> web_contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  browser()->tab_strip_model()->InsertWebContentsAt(1, std::move(web_contents),
                                                    TabStripModel::ADD_ACTIVE);

  ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchStatePerTabBrowserControllerTest,
                       SidePanelTogglesCorrectlyMultipleTabs) {
  // Navigate to a Google search URL followed by a non-Google URL in two
  // independent browser tabs such that both have the side panel ready. The
  // side panel should respect the state-per-tab flag.

  // Tab 1.
  NavigateActiveTab(browser(), kGoogleSearchURL);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Tab 2.
  AppendTab(browser(), kGoogleSearchURL);
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), kNonGoogleURL);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Show the side panel on Tab 2 and switch to Tab 1. The side panel should
  // not be visible for Tab 1.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Show the side panel on Tab 1 and switch to Tab 2. The side panel should be
  // still be visible for Tab 2, respecting its per-tab state.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 1);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  // Close the side panel on Tab 2 and switch to Tab 1. The side panel should be
  // still be visible for Tab 1, respecting its per-tab state.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
}
