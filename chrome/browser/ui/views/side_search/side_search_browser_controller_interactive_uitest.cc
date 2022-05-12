// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace {

constexpr char kSearchMatchPath[] = "/search-match";
constexpr char kNonMatchPath[] = "/non-match";

ui::MouseEvent GetDummyEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                        base::TimeTicks::Now(), 0, 0);
}

bool IsSearchURLMatch(const GURL& url) {
  // Test via path prefix match as the embedded test server ensures that all
  // URLs are using the same host and paths are made unique via appending a
  // monotonically increasing value to the end of their paths.
  return url.path().find(kSearchMatchPath) != std::string::npos;
}

}  // namespace

class SideSearchBrowserControllerTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    const bool enable_dse_support = GetParam();
    if (enable_dse_support) {
      scoped_feature_list_.InitWithFeatures(
          {features::kSideSearch, features::kSideSearchDSESupport}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({features::kSideSearch},
                                            {features::kSideSearchDSESupport});
    }
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }
  void TearDown() override {
    InProcessBrowserTest::TearDown();
    scoped_feature_list_.Reset();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&SideSearchBrowserControllerTest::HandleRequest,
                            base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();

    InProcessBrowserTest::SetUpOnMainThread();
    auto* config = SideSearchConfig::Get(browser()->profile());
    // Basic configuration for testing that allows navigations to URLs with
    // paths prefixed with `kSearchMatchPath` to proceed within the side panel,
    // and only allows showing the side panel on non-matching pages.
    config->SetShouldNavigateInSidePanelCallback(
        base::BindRepeating(IsSearchURLMatch));
    config->SetCanShowSidePanelForURLCallback(base::BindRepeating(
        [](const GURL& url) { return !IsSearchURLMatch(url); }));
    config->SetGenerateSideSearchURLCallback(
        base::BindRepeating([](const GURL& url) { return url; }));
    SetIsSidePanelSRPAvailableAt(browser(), 0, true);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void ActivateTabAt(Browser* browser, int index) {
    browser->tab_strip_model()->ActivateTabAt(index);
  }

  void AppendTab(Browser* browser, const GURL& url) {
    chrome::AddTabAt(browser, url, -1, true);
    SetIsSidePanelSRPAvailableAt(
        browser, browser->tab_strip_model()->GetTabCount() - 1, true);
  }

  void NavigateActiveTab(Browser* browser, const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  }

  content::WebContents* GetActiveSidePanelWebContents(Browser* browser) {
    auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents());
    return tab_contents_helper->side_panel_contents_for_testing();
  }

  void NavigateActiveSideContents(Browser* browser, const GURL& url) {
    auto* side_contents = GetActiveSidePanelWebContents(browser);
    content::TestNavigationObserver nav_observer(side_contents);
    side_contents->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(url));
    nav_observer.Wait();
    if (SideSearchConfig::Get(browser->profile())
            ->ShouldNavigateInSidePanel(url)) {
      // If allowed to proceed in the side panel the side contents committed URL
      // should have been updated to reflect.
      EXPECT_EQ(url, side_contents->GetLastCommittedURL());
    } else {
      // If redirected to the tab contents ensure we observe the correct
      // committed URL in the tab.
      auto* tab_contents = browser->tab_strip_model()->GetActiveWebContents();
      content::TestNavigationObserver tab_observer(tab_contents);
      tab_observer.Wait();
      EXPECT_EQ(url, tab_contents->GetLastCommittedURL());
    }
  }

  void NotifyButtonClick(Browser* browser) {
    views::test::ButtonTestApi(GetSidePanelButtonFor(browser))
        .NotifyClick(GetDummyEvent());
    BrowserViewFor(browser)->GetWidget()->LayoutRootViewIfNecessary();
  }

  void NotifyCloseButtonClick(Browser* browser) {
    ASSERT_TRUE(GetSidePanelFor(browser)->GetVisible());
    views::test::ButtonTestApi(GetSideButtonClosePanelFor(browser))
        .NotifyClick(GetDummyEvent());
    BrowserViewFor(browser)->GetWidget()->LayoutRootViewIfNecessary();
  }

  void NotifyReadLaterButtonClick(Browser* browser) {
    views::test::ButtonTestApi(GetReadLaterButtonFor(browser))
        .NotifyClick(GetDummyEvent());
  }

  void SetIsSidePanelSRPAvailableAt(Browser* browser,
                                    int index,
                                    bool is_available) {
    SideSearchConfig::Get(browser->profile())
        ->set_is_side_panel_srp_available(is_available);
  }

  BrowserView* BrowserViewFor(Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser);
  }

  views::Button* GetSidePanelButtonFor(Browser* browser) {
    views::View* button_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kSideSearchButtonElementId, browser->window()->GetElementContext());
    return button_view ? views::AsViewClass<views::Button>(button_view)
                       : nullptr;
  }

  views::Button* GetReadLaterButtonFor(Browser* browser) {
    views::View* button_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kReadLaterButtonElementId, browser->window()->GetElementContext());
    return button_view ? views::AsViewClass<views::Button>(button_view)
                       : nullptr;
  }

  // Extract the testing of the entrypoint when the side panel is open into its
  // own method as this will vary depending on whether or not the DSE support
  // flag is enabled. For e.g. when DSE support is enabled and the side panel is
  // open the omnibox button is hidden while if DSE support is disabled the
  // toolbar button is visible.
  void TestSidePanelOpenEntrypointState(Browser* browser) {
    // If the side panel is visible and DSE support is enabled then the
    // entrypoint should be hidden. Otherwise the entrypoint should be visible.
    if (side_search::IsDSESupportEnabled(browser->profile())) {
      EXPECT_FALSE(GetSidePanelButtonFor(browser)->GetVisible());
    } else {
      EXPECT_TRUE(GetSidePanelButtonFor(browser)->GetVisible());
    }
  }

  views::ImageButton* GetSideButtonClosePanelFor(Browser* browser) {
    return static_cast<views::ImageButton*>(
        GetSidePanelFor(browser)->GetViewByID(static_cast<int>(
            SideSearchBrowserController::VIEW_ID_SIDE_PANEL_CLOSE_BUTTON)));
  }

  SidePanel* GetSidePanelFor(Browser* browser) {
    return BrowserViewFor(browser)->side_search_side_panel_for_testing();
  }

  content::WebContents* GetSidePanelContentsFor(Browser* browser, int index) {
    auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
        browser->tab_strip_model()->GetWebContentsAt(index));
    return tab_contents_helper->side_panel_contents_for_testing();
  }

  void NavigateToMatchingAndNonMatchingSearchPage(Browser* browser) {
    // The side panel button should never be visible on the matched search page.
    NavigateActiveTab(browser, GetMatchingSearchUrl());
    EXPECT_FALSE(GetSidePanelButtonFor(browser)->GetVisible());
    EXPECT_FALSE(GetSidePanelFor(browser)->GetVisible());

    // The side panel button should be visible if on a non-matched page and the
    // current tab has previously encountered a matched search page.
    NavigateActiveTab(browser, GetNonMatchingUrl());
    EXPECT_TRUE(GetSidePanelButtonFor(browser)->GetVisible());
    EXPECT_FALSE(GetSidePanelFor(browser)->GetVisible());
  }

  void NavigateToMatchingSearchPageAndOpenSidePanel(Browser* browser) {
    NavigateToMatchingAndNonMatchingSearchPage(browser);

    NotifyButtonClick(browser);
    TestSidePanelOpenEntrypointState(browser);
    EXPECT_TRUE(GetSidePanelFor(browser)->GetVisible());
    EXPECT_TRUE(
        content::WaitForLoadStop(GetActiveSidePanelWebContents(browser)));
  }

 protected:
  GURL GetMatchingSearchUrl() {
    // Ensure that each returned matching URL is unique.
    static int id = 1;
    return embedded_test_server()->GetURL(
        base::StrCat({kSearchMatchPath, base::NumberToString(id++)}));
  }

  GURL GetNonMatchingUrl() {
    // Ensure that each returned non-matching URL is unique.
    static int id = 1;
    return embedded_test_server()->GetURL(
        base::StrCat({kNonMatchPath, base::NumberToString(id++)}));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    return std::move(http_response);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SideSearchBrowserControllerTest,
                         ::testing::Bool());

// This interaction tests that pages in the tab frame opened from the side panel
// are correctly marked as being non-skippable despite the tab frame not
// receiving a user gesture.
//   1. Have the side panel open A in the tab.
//   2. Have the side panel open B1 in the tab.
//   3. B1 automatically redirects to B2 to attempt to trap the user.
//   4. Navigating backwards from B2 should skip back to A.
//   5. Navigating backwards from A should skip back to the tab's initial page.
IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelOpenedPagesInTheTabFrameAreNonSkippable) {
  // Start with the side panel in a toggled open state. The side panel will
  // intercept non-matching search URLs and redirect these to the tab contents.
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());

  // Always show the side panel for the duration of the navigation test.
  SideSearchConfig::Get(browser()->profile())
      ->SetCanShowSidePanelForURLCallback(
          base::BindRepeating([](const GURL& url) { return true; }));

  const GURL initial_url = embedded_test_server()->GetURL("/initial.html");
  const GURL a_url = embedded_test_server()->GetURL("/A.html");
  const GURL b1_url = embedded_test_server()->GetURL("/B1.html");
  const GURL b2_url = embedded_test_server()->GetURL("/B2.html");

  // Start the tab contents at the initial url.
  NavigateActiveTab(browser(), initial_url);
  auto* tab_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(initial_url, tab_contents->GetLastCommittedURL());

  // Have the side panel open page A in the tab contents.
  NavigateActiveSideContents(browser(), a_url);
  EXPECT_EQ(a_url, tab_contents->GetLastCommittedURL());

  // Have the side panel open page B1 in the tab contents. Immediately redirect
  // this to page B2.
  NavigateActiveSideContents(browser(), b1_url);
  EXPECT_EQ(b1_url, tab_contents->GetLastCommittedURL());
  {
    content::TestNavigationObserver tab_observer(tab_contents);
    EXPECT_TRUE(content::ExecJs(tab_contents,
                                "location = '" + b2_url.spec() + "';",
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    tab_observer.Wait();
    EXPECT_EQ(b2_url, tab_contents->GetLastCommittedURL());
  }

  // Go back from page B2. B1 should be skippable and we should return to A.
  {
    content::TestNavigationObserver tab_observer(tab_contents);
    tab_contents->GetController().GoBack();
    tab_observer.Wait();
    EXPECT_EQ(a_url, tab_contents->GetLastCommittedURL());
  }

  // Go back from page A. We should return to the initial page.
  {
    content::TestNavigationObserver tab_observer(tab_contents);
    tab_contents->GetController().GoBack();
    tab_observer.Wait();
    EXPECT_EQ(initial_url, tab_contents->GetLastCommittedURL());
  }
}

// This interaction tests that only the final page in the tab frame arrived at
// from a redirection chain initiated from the side panel is marked as skippable
// and not the intermediate pages in the chain.
//   1. Have the side panel open A1 in the tab.
//   2. A1 automatically redirects to A2 to attempt to trap the user.
//   3. Have the side panel open B in the tab.
//   4. Navigating backwards from B should skip back to A2.
//   5. Navigating backwards from A2 should skip back to the tab's initial page.
IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       RedirectedPagesOpenedFromTheSidePanelAreSkippable) {
  // Start with the side panel in a toggled open state. The side panel will
  // intercept non-matching search URLs and redirect these to the tab contents.
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());

  // Always show the side panel for the duration of the navigation test.
  SideSearchConfig::Get(browser()->profile())
      ->SetCanShowSidePanelForURLCallback(
          base::BindRepeating([](const GURL& url) { return true; }));

  const GURL initial_url = embedded_test_server()->GetURL("/initial.html");
  const GURL a1_url = embedded_test_server()->GetURL("/A1.html");
  const GURL a2_url = embedded_test_server()->GetURL("/A2.html");
  const GURL b_url = embedded_test_server()->GetURL("/B.html");

  // Start the tab contents at the initial url.
  NavigateActiveTab(browser(), initial_url);
  auto* tab_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(initial_url, tab_contents->GetLastCommittedURL());

  // Have the side panel open page A1 in the tab contents. Immediately redirect
  // this to page A2.
  NavigateActiveSideContents(browser(), a1_url);
  EXPECT_EQ(a1_url, tab_contents->GetLastCommittedURL());
  {
    content::TestNavigationObserver tab_observer(tab_contents);
    EXPECT_TRUE(content::ExecJs(tab_contents,
                                "location = '" + a2_url.spec() + "';",
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    tab_observer.Wait();
    EXPECT_EQ(a2_url, tab_contents->GetLastCommittedURL());
  }

  // Have the side panel open page B in the tab contents.
  NavigateActiveSideContents(browser(), b_url);
  EXPECT_EQ(b_url, tab_contents->GetLastCommittedURL());

  // Go back from page B. We should return to page A2.
  {
    content::TestNavigationObserver tab_observer(tab_contents);
    tab_contents->GetController().GoBack();
    tab_observer.Wait();
    EXPECT_EQ(a2_url, tab_contents->GetLastCommittedURL());
  }

  // Go back from page A2. A1 should be skippable and we should return to the
  // initial page
  {
    content::TestNavigationObserver tab_observer(tab_contents);
    tab_contents->GetController().GoBack();
    tab_observer.Wait();
    EXPECT_EQ(initial_url, tab_contents->GetLastCommittedURL());
  }
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlySingleTab) {
  // If no previous matched search page has been navigated to the button should
  // not be visible.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // The side panel button should never be visible on a matched search page.
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-matched page and the
  // current tab has previously encountered a matched search page.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.AvailabilityChanged",
      SideSearchAvailabilityChangeType::kBecomeAvailable, 1);
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlyMultipleTabs) {
  // The side panel button should never be visible on non-matching pages.
  AppendTab(browser(), GetNonMatchingUrl());
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // Navigate to a matched search page and then to a non-matched search page.
  // This should show the side panel button in the toolbar.
  AppendTab(browser(), GetMatchingSearchUrl());
  ActivateTabAt(browser(), 2);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());

  // Switch back to the matched search page, the side panel button should no
  // longer be visible.
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // When switching back to the tab on the non-matched page with a previously
  // visited matched search page, the button should be visible.
  ActivateTabAt(browser(), 2);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelTogglesCorrectlySingleTab) {
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-matched page and the
  // current tab has previously encountered a matched search page.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Toggle the side panel.
  NotifyButtonClick(browser());
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.OpenAction",
      SideSearchOpenActionType::kTapOnSideSearchToolbarButton, 1);
  histogram_tester_.ExpectTotalCount(
      "SideSearch.TimeSinceSidePanelAvailableToFirstOpen", 1);

  // Toggling the close button should close the side panel.
  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.CloseAction",
      SideSearchCloseActionType::kTapOnSideSearchCloseButton, 1);
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       CloseButtonClosesSidePanel) {
  // The close button should be visible in the toggled state.
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  NotifyCloseButtonClick(browser());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.CloseAction",
      SideSearchCloseActionType::kTapOnSideSearchCloseButton, 1);
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SideSearchNotAvailableInOTR) {
  Browser* browser2 = CreateIncognitoBrowser();
  EXPECT_TRUE(browser2->profile()->IsOffTheRecord());
  NavigateActiveTab(browser2, GetMatchingSearchUrl());
  NavigateActiveTab(browser2, GetNonMatchingUrl());

  EXPECT_EQ(nullptr, GetSidePanelButtonFor(browser2));
  EXPECT_EQ(nullptr, GetSidePanelFor(browser2));
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest, ReadLaterWorkInOTR) {
  Browser* browser2 = CreateIncognitoBrowser();
  EXPECT_TRUE(browser2->profile()->IsOffTheRecord());
  NotifyReadLaterButtonClick(browser2);
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelButtonIsNotShownWhenSRPIsUnavailable) {
  // Set the side panel SRP be unavailable.
  SetIsSidePanelSRPAvailableAt(browser(), 0, false);

  // If no previous matched search page has been navigated to the button should
  // not be visible.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should never be visible on the matched search page.
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should not be visible if the side panel SRP is not
  // available.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       OpeningAndClosingTheSidePanelHandlesFocusCorrectly) {
  // Navigate to a matching search page and then a non-matched page. The side
  // panel will be available but closed.
  NavigateToMatchingAndNonMatchingSearchPage(browser());

  auto* browser_view = BrowserViewFor(browser());
  auto* side_panel = GetSidePanelFor(browser());
  auto* contents_view = browser_view->contents_web_view();
  auto* focus_manager = browser_view->GetFocusManager();
  ASSERT_NE(nullptr, focus_manager);

  // Set focus to the contents view.
  contents_view->RequestFocus();
  EXPECT_FALSE(side_panel->GetVisible());
  EXPECT_TRUE(contents_view->HasFocus());

  // Open the side panel. The side panel should receive focus.
  NotifyButtonClick(browser());
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_FALSE(contents_view->HasFocus());
  EXPECT_TRUE(side_panel->Contains(focus_manager->GetFocusedView()));

  // Close the side panel. The contents view should have its focus restored.
  NotifyCloseButtonClick(browser());
  EXPECT_FALSE(side_panel->GetVisible());
  EXPECT_TRUE(contents_view->HasFocus());
  EXPECT_FALSE(side_panel->Contains(focus_manager->GetFocusedView()));
}

IN_PROC_BROWSER_TEST_P(
    SideSearchBrowserControllerTest,
    SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows) {
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());

  Browser* browser2 = CreateBrowser(browser()->profile());
  NavigateToMatchingAndNonMatchingSearchPage(browser2);

  std::unique_ptr<content::WebContents> web_contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  browser()->tab_strip_model()->InsertWebContentsAt(1, std::move(web_contents),
                                                    TabStripModel::ADD_ACTIVE);

  ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 0);
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelTogglesCorrectlyMultipleTabs) {
  // Navigate to a matching search URL followed by a non-matching URL in two
  // independent browser tabs such that both have the side panel ready. The
  // side panel should respect the state-per-tab flag.

  // Tab 1.
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Tab 2.
  AppendTab(browser(), GetMatchingSearchUrl());
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Show the side panel on Tab 2 and switch to Tab 1. The side panel should
  // not be visible for Tab 1.
  NotifyButtonClick(browser());
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.TimeSinceSidePanelAvailableToFirstOpen", 1);

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Show the side panel on Tab 1 and switch to Tab 2. The side panel should be
  // still be visible for Tab 2, respecting its per-tab state.
  NotifyButtonClick(browser());
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.TimeSinceSidePanelAvailableToFirstOpen", 2);
  // TimeShownOpenedVia[Entrypoint/TabSwitch] is emitted when the side panel for
  // a given tab is hidden.
  histogram_tester_.ExpectTotalCount(
      "SideSearch.SidePanel.TimeShownOpenedViaEntrypoint", 1);

  ActivateTabAt(browser(), 1);
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.SidePanel.TimeShownOpenedViaEntrypoint", 2);

  // Close the side panel on Tab 2 and switch to Tab 1. The side panel should be
  // still be visible for Tab 1, respecting its per-tab state.
  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.SidePanel.TimeShownOpenedViaTabSwitch", 1);

  ActivateTabAt(browser(), 0);
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.SidePanel.TimeShownOpenedViaTabSwitch", 2);
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SwitchingTabsHandlesFocusCorrectly) {
  auto* browser_view = BrowserViewFor(browser());
  auto* side_panel = GetSidePanelFor(browser());
  auto* contents_view = browser_view->contents_web_view();
  auto* focus_manager = browser_view->GetFocusManager();
  ASSERT_NE(nullptr, focus_manager);

  // The side panel should currently have focus as it was opened via the toolbar
  // button.
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_TRUE(side_panel->Contains(focus_manager->GetFocusedView()));
  EXPECT_FALSE(contents_view->HasFocus());

  // Switch to another tab and open the side panel. The side panel should still
  // have focus as it was opened via the toolbar button.
  AppendTab(browser(), GetNonMatchingUrl());
  ActivateTabAt(browser(), 1);
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_TRUE(side_panel->Contains(focus_manager->GetFocusedView()));
  EXPECT_FALSE(contents_view->HasFocus());

  // Set focus to the contents view and switch to the first tab (which also has
  // its side panel toggled open). In this switch the focus should return to the
  // side panel as the BrowserView will update focus on a tab switch.
  contents_view->RequestFocus();
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_FALSE(side_panel->Contains(focus_manager->GetFocusedView()));
  EXPECT_TRUE(contents_view->HasFocus());

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_TRUE(side_panel->Contains(focus_manager->GetFocusedView()));
  EXPECT_FALSE(contents_view->HasFocus());
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelTogglesClosedCorrectlyDuringNavigation) {
  // Navigate to a matching SRP and then a non-matched page. The side panel will
  // be available and open.
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());
  auto* side_panel = GetSidePanelFor(browser());

  // Navigating to a matching SRP URL should automatically hide the side panel
  // as it should not be available.
  EXPECT_TRUE(side_panel->GetVisible());
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(side_panel->GetVisible());

  // When navigating again to a non-matching page the side panel will become
  // available again but should not automatically reopen.
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(side_panel->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelCrashesCloseSidePanel) {
  // Open two tabs with the side panel open.
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());
  AppendTab(browser(), GetNonMatchingUrl());
  ActivateTabAt(browser(), 1);
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());

  auto* side_panel = GetSidePanelFor(browser());

  // Side panel should be open with the side contents present.
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_NE(nullptr, GetSidePanelContentsFor(browser(), 1));

  // Simulate a crash in the hosted side panel contents.
  auto* rph_second_tab =
      GetSidePanelContentsFor(browser(), 1)->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer_second_tab(
      rph_second_tab,
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph_second_tab->Shutdown(content::RESULT_CODE_KILLED));
  crash_observer_second_tab.Wait();

  // Side panel should be closed and the WebContents cleared.
  EXPECT_FALSE(side_panel->GetVisible());
  EXPECT_EQ(nullptr, GetSidePanelContentsFor(browser(), 1));

  // Simulate a crash in the side panel contents of the first tab which is not
  // currently active.
  auto* rph_first_tab =
      GetSidePanelContentsFor(browser(), 0)->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer_first_tab(
      rph_first_tab, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph_first_tab->Shutdown(content::RESULT_CODE_KILLED));
  crash_observer_first_tab.Wait();

  // Switch to the first tab, the side panel should still be closed.
  ActivateTabAt(browser(), 0);
  EXPECT_FALSE(side_panel->GetVisible());
  EXPECT_EQ(nullptr, GetSidePanelContentsFor(browser(), 0));

  // Reopening the side panel should restore the side panel and its contents.
  NotifyButtonClick(browser());
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_NE(nullptr, GetSidePanelContentsFor(browser(), 0));
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       TimeUntilOpenMetricEmittedCorrectlyMultipleNavigations) {
  // Perform a search and navigate multiple times to non-matching pages before
  // finally opening the side panel.
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  NotifyButtonClick(browser());
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.TimeSinceSidePanelAvailableToFirstOpen", 1);
}

// Only test the side search icon view chip in the DSE configuration.
using SideSearchIconViewTest = SideSearchBrowserControllerTest;
INSTANTIATE_TEST_SUITE_P(All, SideSearchIconViewTest, testing::Values(true));

// Tests that metrics correctly capture whether the label was visible when the
// entrypoint was toggled.
IN_PROC_BROWSER_TEST_P(SideSearchIconViewTest,
                       LabelVisibilityMetricsCorrectlyEmittedWhenToggled) {
  auto* button_view = GetSidePanelButtonFor(browser());
  ASSERT_NE(nullptr, button_view);
  auto* icon_view = views::AsViewClass<SideSearchIconView>(button_view);

  // Get the browser into a state where the icon view is visible.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  ASSERT_FALSE(icon_view->GetVisible());
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(icon_view->GetVisible());

  // Show the icon's label and toggle the side panel. It should correctly log
  // being shown while the label was visible.
  EXPECT_TRUE(icon_view->GetVisible());
  icon_view->SetLabelVisibilityForTesting(true);
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled",
      SideSearchPageActionLabelVisibility::kVisible, 1);
  histogram_tester_.ExpectBucketCount(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled",
      SideSearchPageActionLabelVisibility::kNotVisible, 0);

  // Close the side panel.
  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Hide the icon's label and toggle the side panel. It should correctly log
  // being shown while the label was hidden.
  EXPECT_TRUE(icon_view->GetVisible());
  icon_view->SetLabelVisibilityForTesting(false);
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled",
      SideSearchPageActionLabelVisibility::kVisible, 1);
  histogram_tester_.ExpectBucketCount(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled",
      SideSearchPageActionLabelVisibility::kNotVisible, 1);
}

// Fixture for testing side panel clobbering behavior with global panels.
class SideSearchDSEClobberingTest : public SideSearchBrowserControllerTest {
 public:
  // SideSearchBrowserControllerTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kSidePanelImprovedClobbering}, {});
    SideSearchBrowserControllerTest::SetUp();
  }
  void TearDown() override {
    SideSearchBrowserControllerTest::TearDown();
    scoped_feature_list_.Reset();
  }

  // Immediately open and make visible the global side panel.
  void ShowGlobalSidePanel(Browser* browser) {
    ASSERT_FALSE(GetGlobalSidePanelFor(browser)->GetVisible());
    auto* side_panel_button = GetToolbarSidePanelButtonFor(browser);
    views::test::ButtonTestApi(side_panel_button).NotifyClick(GetDummyEvent());

    // The WebUI typically loads and is shown asynchronously. Synchronously show
    // the view here for testing.
    views::View* web_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kReadLaterSidePanelWebViewElementId,
            browser->window()->GetElementContext());
    DCHECK(web_view);
    views::AsViewClass<SidePanelWebUIView>(web_view)->ShowUI();

    BrowserViewFor(browser)->GetWidget()->LayoutRootViewIfNecessary();
  }

  // Uses the toolbar side panel button to close whichever side panel is
  // currently open.
  void CloseActiveSidePanel(Browser* browser) {
    ASSERT_TRUE(GetGlobalSidePanelFor(browser)->GetVisible() ||
                GetSidePanelFor(browser));
    auto* side_panel_button = GetToolbarSidePanelButtonFor(browser);
    views::test::ButtonTestApi(side_panel_button).NotifyClick(GetDummyEvent());
    BrowserViewFor(browser)->GetWidget()->LayoutRootViewIfNecessary();
  }

  // Sets up a browser with three tabs, an open global panel and an open side
  // search panel for the last tab.
  void SetupBrowserForClobberingTests(Browser* browser) {
    auto* global_panel = GetGlobalSidePanelFor(browser);
    EXPECT_FALSE(global_panel->GetVisible());
    ShowGlobalSidePanel(browser);
    EXPECT_TRUE(global_panel->GetVisible());

    // Add another two tabs, the global panel should remain open for each.
    AppendTab(browser, GetNonMatchingUrl());
    ActivateTabAt(browser, 1);
    EXPECT_TRUE(global_panel->GetVisible());

    AppendTab(browser, GetNonMatchingUrl());
    ActivateTabAt(browser, 2);
    EXPECT_TRUE(global_panel->GetVisible());

    // Open the side search contextual panel for the current active tab.
    auto* side_search_panel = GetSidePanelFor(browser);
    NavigateToMatchingSearchPageAndOpenSidePanel(browser);
    EXPECT_TRUE(side_search_panel->GetVisible());
    EXPECT_FALSE(global_panel->GetVisible());
  }

  SidePanelToolbarButton* GetToolbarSidePanelButtonFor(Browser* browser) {
    views::View* button_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kReadLaterButtonElementId, browser->window()->GetElementContext());
    return button_view ? views::AsViewClass<SidePanelToolbarButton>(button_view)
                       : nullptr;
  }

  SidePanel* GetGlobalSidePanelFor(Browser* browser) {
    return BrowserViewFor(browser)->right_aligned_side_panel();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Only instantiate tests for the DSE configuration.
INSTANTIATE_TEST_SUITE_P(All,
                         SideSearchDSEClobberingTest,
                         ::testing::Values(true));

IN_PROC_BROWSER_TEST_P(SideSearchDSEClobberingTest,
                       GlobalBrowserSidePanelIsToggleable) {
  auto* global_panel = GetGlobalSidePanelFor(browser());
  EXPECT_FALSE(global_panel->GetVisible());
  ShowGlobalSidePanel(browser());
  EXPECT_TRUE(global_panel->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SideSearchDSEClobberingTest,
                       ContextualPanelsDoNotClobberGlobalPanels) {
  SetupBrowserForClobberingTests(browser());
  auto* global_panel = GetGlobalSidePanelFor(browser());
  auto* side_search_panel = GetSidePanelFor(browser());

  // Switching to tabs with no open contextual panels should instead show the
  // global panel.
  ActivateTabAt(browser(), 1);
  EXPECT_TRUE(global_panel->GetVisible());
  EXPECT_FALSE(side_search_panel->GetVisible());

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(global_panel->GetVisible());
  EXPECT_FALSE(side_search_panel->GetVisible());

  // Switching back to the tab with the contextual panel should show the
  // contextual panel and not the global panel.
  ActivateTabAt(browser(), 2);
  EXPECT_FALSE(global_panel->GetVisible());
  EXPECT_TRUE(side_search_panel->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SideSearchDSEClobberingTest,
                       OpeningGlobalPanelsClosesAllContextualPanels) {
  auto* global_panel = GetGlobalSidePanelFor(browser());
  auto* side_search_panel = GetSidePanelFor(browser());
  AppendTab(browser(), GetNonMatchingUrl());
  AppendTab(browser(), GetNonMatchingUrl());

  // There should be three tabs and no panels open.
  for (int i = 0; i < 3; ++i) {
    ActivateTabAt(browser(), i);
    EXPECT_FALSE(global_panel->GetVisible());
    EXPECT_FALSE(side_search_panel->GetVisible());
  }

  // Open a contextual panel on the last tab.
  ActivateTabAt(browser(), 2);
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());
  EXPECT_FALSE(global_panel->GetVisible());
  EXPECT_TRUE(side_search_panel->GetVisible());

  // Switch to the first tab and open a global panel.
  ActivateTabAt(browser(), 0);
  ShowGlobalSidePanel(browser());
  EXPECT_TRUE(global_panel->GetVisible());
  EXPECT_FALSE(side_search_panel->GetVisible());

  // The global panel should now be open for all browser tabs.
  for (int i = 0; i < 3; ++i) {
    ActivateTabAt(browser(), i);
    EXPECT_TRUE(global_panel->GetVisible());
    EXPECT_FALSE(side_search_panel->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_P(
    SideSearchDSEClobberingTest,
    ContextualAndGlobalPanelsBehaveAsExpectedWhenDraggingBetweenWindows) {
  // Open two browsers with three tabs each. Both have open global side panel
  // and an open side search panel for their last tab.
  Browser* browser2 = CreateBrowser(browser()->profile());
  SetupBrowserForClobberingTests(browser());
  SetupBrowserForClobberingTests(browser2);

  // Move the currently active tab with side search from browser2 to browser1.
  std::unique_ptr<content::WebContents> web_contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(2);
  browser()->tab_strip_model()->InsertWebContentsAt(3, std::move(web_contents),
                                                    TabStripModel::ADD_ACTIVE);

  // The global panel should now be visibe in browser2 and the contextual panel
  // should be visible in browser1.
  auto* global_panel1 = GetGlobalSidePanelFor(browser());
  auto* global_panel2 = GetGlobalSidePanelFor(browser2);
  auto* side_search_panel1 = GetSidePanelFor(browser());
  auto* side_search_panel2 = GetSidePanelFor(browser2);

  EXPECT_TRUE(global_panel2->GetVisible());
  EXPECT_FALSE(side_search_panel2->GetVisible());

  EXPECT_FALSE(global_panel1->GetVisible());
  EXPECT_TRUE(side_search_panel1->GetVisible());

  // In browser1 switch to the tab that originally had the side search panel
  // open. The global panels should remain closed.
  ActivateTabAt(browser(), 2);
  EXPECT_FALSE(global_panel1->GetVisible());
  EXPECT_TRUE(side_search_panel1->GetVisible());

  // In browser1 switch to tabs that did not have a side search panel open. The
  // side search panel should be hidden and the global panel should be visible.
  ActivateTabAt(browser(), 1);
  EXPECT_TRUE(global_panel1->GetVisible());
  EXPECT_FALSE(side_search_panel1->GetVisible());

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(global_panel1->GetVisible());
  EXPECT_FALSE(side_search_panel1->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SideSearchDSEClobberingTest,
                       ClosingTheContextualPanelClosesAllBrowserPanels) {
  SetupBrowserForClobberingTests(browser());
  auto* global_panel = GetGlobalSidePanelFor(browser());
  auto* side_search_panel = GetSidePanelFor(browser());

  // Append an additional browser tab with an open side search panel.
  AppendTab(browser(), GetNonMatchingUrl());
  ActivateTabAt(browser(), 3);
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());

  // Close the contextual panel. The global and contextual panels in the current
  // and other tabs should all be closed.
  CloseActiveSidePanel(browser());
  for (int i = 0; i < 3; ++i) {
    ActivateTabAt(browser(), i);
    EXPECT_FALSE(global_panel->GetVisible());
    EXPECT_FALSE(side_search_panel->GetVisible());
  }
}

IN_PROC_BROWSER_TEST_P(SideSearchDSEClobberingTest,
                       ClosingTheGlobalPanelClosesAllBrowserPanels) {
  SetupBrowserForClobberingTests(browser());
  auto* global_panel = GetGlobalSidePanelFor(browser());
  auto* side_search_panel = GetSidePanelFor(browser());

  // Append an additional browser tab with an open side search panel.
  AppendTab(browser(), GetNonMatchingUrl());
  ActivateTabAt(browser(), 3);
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());

  // Close the global panel. The global and contextual panels in the current
  // and other tabs should all be closed.
  ActivateTabAt(browser(), 0);
  CloseActiveSidePanel(browser());
  for (int i = 0; i < 3; ++i) {
    ActivateTabAt(browser(), i);
    EXPECT_FALSE(global_panel->GetVisible());
    EXPECT_FALSE(side_search_panel->GetVisible());
  }
}

// Base class for Extensions API tests for the side panel WebContents.
class SideSearchExtensionsTest : public SideSearchBrowserControllerTest {
 public:
  void SetUpOnMainThread() override {
    SideSearchBrowserControllerTest::SetUpOnMainThread();
    // We want all navigations to be routed through the side panel for the
    // purposes of testing extension support.
    auto* config = SideSearchConfig::Get(browser()->profile());
    config->SetShouldNavigateInSidePanelCallback(
        base::BindRepeating([](const GURL& url) { return true; }));
    config->SetCanShowSidePanelForURLCallback(
        base::BindRepeating([](const GURL& url) { return true; }));

    // Navigate to the first URL and open the side panel. This should create and
    // initiate a navigation in the side panel WebContents.
    NavigateActiveTab(browser(),
                      embedded_test_server()->GetURL("initial.example", "/"));
    NotifyButtonClick(browser());
    TestSidePanelOpenEntrypointState(browser());
    EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

    // Wait for the side panel to finish loading the test URL.
    content::WebContents* side_contents = GetSidePanelContentsFor(browser(), 0);
    EXPECT_TRUE(content::WaitForLoadStop(side_contents));
  }

  void NavigateInSideContents(const GURL& navigation_url,
                              const GURL& expected_url) {
    content::WebContents* side_contents = GetSidePanelContentsFor(browser(), 0);

    content::TestNavigationObserver nav_observer(side_contents);
    side_contents->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(navigation_url));
    nav_observer.Wait();

    EXPECT_EQ(expected_url, side_contents->GetLastCommittedURL());
  }
};

INSTANTIATE_TEST_SUITE_P(All, SideSearchExtensionsTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(SideSearchExtensionsTest,
                       ContentScriptsExecuteInSidePanel) {
  const GURL first_url = embedded_test_server()->GetURL("first.example", "/");
  const GURL second_url = embedded_test_server()->GetURL("second.example", "/");
  const GURL third_url = embedded_test_server()->GetURL("third.example", "/");
  constexpr char kManifest[] = R"(
      {
        "name": "Side Search Content Script Test",
        "manifest_version": 2,
        "version": "0.1",
        "content_scripts": [{
          "matches": ["*://*.second.example/*"],
          "js": ["script.js"],
          "run_at": "document_end"
        }]
      }
  )";
  constexpr char kContentScript[] =
      "document.body.innerText = 'content script has run';";
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kContentScript);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(browser()->profile())
          .LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  content::WebContents* side_contents = GetSidePanelContentsFor(browser(), 0);

  // The extension should not run for the first URL.
  NavigateInSideContents(first_url, first_url);
  EXPECT_EQ("", content::EvalJs(side_contents, "document.body.innerText;"));

  // The extension should run for the second URL.
  NavigateInSideContents(second_url, second_url);
  EXPECT_EQ("content script has run",
            content::EvalJs(side_contents, "document.body.innerText;"));

  // The extension should not run for the third URL.
  NavigateInSideContents(third_url, third_url);
  EXPECT_EQ("", content::EvalJs(side_contents, "document.body.innerText;"));
}

IN_PROC_BROWSER_TEST_P(SideSearchExtensionsTest,
                       WebRequestInterceptsSidePanelNavigations) {
  const GURL first_url = embedded_test_server()->GetURL("first.example", "/");
  const GURL second_url = embedded_test_server()->GetURL("second.example", "/");
  const GURL third_url = embedded_test_server()->GetURL("third.example", "/");
  const GURL redirect_url =
      embedded_test_server()->GetURL("example.redirect", "/");
  constexpr char kManifest[] = R"(
      {
        "name": "WebRequest Test Extension",
        "version": "0.1",
        "manifest_version": 2,
        "background": {
          "scripts": ["background.js"]
        },
        "permissions": [
          "webRequest",
          "webRequestBlocking",
          "*://first.example/*",
          "*://second.example/*"
        ]
      }
  )";
  constexpr char kRulesScriptTemplate[] = R"(
      chrome.webRequest.onBeforeRequest.addListener(function(d) {
          return {redirectUrl: $1};
        }, {urls: ["*://*.second.example/*"]}, ["blocking"]);
  )";
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      content::JsReplace(kRulesScriptTemplate, redirect_url));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(browser()->profile())
          .LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigation to the first URL should be initiated in the side panel as
  // expected.
  NavigateInSideContents(first_url, first_url);

  // Navigation to the second URL should be redirected by the webRequest API.
  NavigateInSideContents(second_url, redirect_url);

  // Navigation to the third URL should proceed as expected.
  NavigateInSideContents(third_url, third_url);
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1305891): Test is flaky on Mac bots.
#define MAYBE_DeclarativeNetRequestInterceptsSidePanelNavigations \
  DISABLED_DeclarativeNetRequestInterceptsSidePanelNavigations
#else
#define MAYBE_DeclarativeNetRequestInterceptsSidePanelNavigations \
  DeclarativeNetRequestInterceptsSidePanelNavigations
#endif
IN_PROC_BROWSER_TEST_P(
    SideSearchExtensionsTest,
    MAYBE_DeclarativeNetRequestInterceptsSidePanelNavigations) {
  const GURL first_url = embedded_test_server()->GetURL("first.example", "/");
  const GURL second_url = embedded_test_server()->GetURL("second.example", "/");
  const GURL third_url = embedded_test_server()->GetURL("third.example", "/");
  const GURL redirect_url =
      embedded_test_server()->GetURL("example.redirect", "/");
  constexpr char kManifest[] = R"(
      {
        "name": "WebRequest Test Extension",
        "version": "0.1",
        "manifest_version": 2,
        "declarative_net_request": {
          "rule_resources": [{
            "id": "ruleset_1",
            "enabled": true,
            "path": "rules.json"
          }]
        },
        "permissions": [
          "declarativeNetRequest",
          "*://first.example/*",
          "*://second.example/*"
        ]
      }
  )";
  constexpr char kRulesJsonTemplate[] = R"(
    [{
      "id": 1,
      "priority": 1,
      "action": {
        "type": "redirect",
        "redirect": { "url": $1 } },
      "condition": {
        "urlFilter": "*second.example*",
        "resourceTypes": ["main_frame"]
      }
    }]
  )";
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("rules.json"),
                          content::JsReplace(kRulesJsonTemplate, redirect_url));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(browser()->profile())
          .LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigation to the first URL should proceed as expected.
  NavigateInSideContents(first_url, first_url);

  // Navigation to the secind URL should be redirected by the netRequest API.
  NavigateInSideContents(second_url, redirect_url);

  // Navigation to the third URL should proceed as expected.
  NavigateInSideContents(third_url, third_url);
}
