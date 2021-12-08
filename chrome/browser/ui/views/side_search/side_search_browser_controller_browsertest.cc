// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_util.h"
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
#include "ui/views/test/button_test_api.h"

namespace {

ui::MouseEvent GetDummyEvent() {
  return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                        base::TimeTicks::Now(), 0, 0);
}

// Strips out the port from a given `url`. Useful when testing side search with
// the EmbeddedTestServer.
GURL GetFilteredURL(const GURL& url) {
  GURL filtered_url = url;
  if (url.has_port()) {
    GURL::Replacements remove_port;
    remove_port.ClearPort();
    filtered_url = filtered_url.ReplaceComponents(remove_port);
  }
  return filtered_url;
}

// In the spirit of testing for the current use of side search for the Google
// DSE set the callback to allow Google SRP urls to proceed in the side panel.
// Note we clear away the port here since google_util::IsGoogleSearchUrl does
// not allow arbitrary ports in the url but the EmbeddedTestServer requires us
// to use URLs with the specific non-standard port it listens on.
// TODO(tluk): Eliminate the Google specific nature of both of these. We should
// be able to update tests to use URLs that satisfy a combination of the below
// two checks without needing such complicated and specific logic.
bool ShouldNavigateInSidePanel(const GURL& url) {
  return google_util::IsGoogleSearchUrl(GetFilteredURL(url));
}

bool CanShowSidePanelForURL(const GURL& url) {
  GURL filtered_url = GetFilteredURL(url);
  return !google_util::IsGoogleSearchUrl(filtered_url) &&
         !google_util::IsGoogleHomePageUrl(filtered_url) &&
         filtered_url.spec() != chrome::kChromeUINewTabURL;
}

}  // namespace

// TODO(tluk): Refactor out google specific references and have this apply
// more generically to DSEs.
class SideSearchBrowserControllerTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    InitializeFeatureList(scoped_feature_list_);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&SideSearchBrowserControllerTest::HandleRequest,
                            base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();

    InProcessBrowserTest::SetUpOnMainThread();
    auto* config = SideSearchConfig::Get(browser()->profile());
    config->SetShouldNavigateInSidePanelCalback(
        base::BindRepeating(ShouldNavigateInSidePanel));
    config->SetCanShowSidePanelForURLCallback(
        base::BindRepeating(CanShowSidePanelForURL));
    SetIsSidePanelSRPAvailableAt(browser(), 0, true);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  virtual void InitializeFeatureList(
      base::test::ScopedFeatureList& feature_list) {
    feature_list.InitWithFeatures({features::kSideSearch},
                                  {features::kSideSearchStatePerTab});
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

  void SetIsSidePanelSRPAvailableAt(Browser* browser,
                                    int index,
                                    bool is_available) {
    SideSearchConfig::Get(browser->profile())
        ->set_is_side_panel_srp_available(is_available);
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

  content::WebContents* GetSidePanelContentsFor(Browser* browser, int index) {
    auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
        browser->tab_strip_model()->GetWebContentsAt(index));
    return tab_contents_helper->side_panel_contents_for_testing();
  }

  void NavigateToSRPAndNonGoogleUrl(
      Browser* browser,
      absl::optional<GURL> google_url = absl::nullopt) {
    // The side panel button should never be visible on the Google search page.
    NavigateActiveTab(browser, google_url.value_or(google_search_url()));
    EXPECT_FALSE(GetSidePanelButtonFor(browser)->GetVisible());
    EXPECT_FALSE(GetSidePanelFor(browser)->GetVisible());

    // The side panel button should be visible if on a non-Google page and the
    // current tab has previously encountered a Google search page.
    NavigateActiveTab(browser, non_google_url());
    EXPECT_TRUE(GetSidePanelButtonFor(browser)->GetVisible());
    EXPECT_FALSE(GetSidePanelFor(browser)->GetVisible());
  }

  void NavigateToSRPAndOpenSidePanel(
      Browser* browser,
      absl::optional<GURL> google_url = absl::nullopt) {
    NavigateToSRPAndNonGoogleUrl(browser,
                                 google_url.value_or(google_search_url()));

    NotifyButtonClick(browser);
    EXPECT_TRUE(GetSidePanelButtonFor(browser)->GetVisible());
    EXPECT_TRUE(GetSidePanelFor(browser)->GetVisible());
    EXPECT_TRUE(
        content::WaitForLoadStop(GetActiveSidePanelWebContents(browser)));
  }

 protected:
  GURL google_search_url() {
    return embedded_test_server()->GetURL("www.google.com", "/search?q=test");
  }

  GURL google_homepage_url() {
    return embedded_test_server()->GetURL("www.google.com", "/");
  }

  GURL non_google_url() {
    return embedded_test_server()->GetURL("www.test.com", "/");
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

// This interaction tests that pages in the tab frame opened from the side panel
// are correctly marked as being non-skippable despite the tab frame not
// receiving a user gesture.
//   1. Have the side panel open A in the tab.
//   2. Have the side panel open B1 in the tab.
//   3. B1 automatically redirects to B2 to attempt to trap the user.
//   4. Navigating backwards from B2 should skip back to A.
//   5. Navigating backwards from A should skip back to the tab's initial page.
IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelOpenedPagesInTheTabFrameAreNonSkippable) {
  // Start with the side panel in a toggled open state. The side panel will
  // intercept non-google search URLs and redirect these to the tab contents.
  NavigateToSRPAndOpenSidePanel(browser());

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
IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       RedirectedPagesOpenedFromTheSidePanelAreSkippable) {
  // Start with the side panel in a toggled open state. The side panel will
  // intercept non-google search URLs and redirect these to the tab contents.
  NavigateToSRPAndOpenSidePanel(browser());

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

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlySingleTab) {
  // The side panel button should never be visible on the Google home page.
  NavigateActiveTab(browser(), google_homepage_url());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // If no previous Google search page has been navigated to the button should
  // not be visible.
  NavigateActiveTab(browser(), non_google_url());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // The side panel button should never be visible on the Google search page.
  NavigateActiveTab(browser(), google_search_url());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(browser(), non_google_url());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.AvailabilityChanged",
      SideSearchAvailabilityChangeType::kBecomeAvailable, 1);

  // The side panel button should never be visible on the Google home page even
  // if it has already been navigated to a Google search page.
  NavigateActiveTab(browser(), google_homepage_url());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.AvailabilityChanged",
      SideSearchAvailabilityChangeType::kBecomeUnavailable, 1);

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(browser(), non_google_url());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.AvailabilityChanged",
      SideSearchAvailabilityChangeType::kBecomeAvailable, 2);
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlyMultipleTabs) {
  // The side panel button should never be visible on the Google home page.
  AppendTab(browser(), google_homepage_url());
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());

  // Navigate to a Google search page and then to a non-Google search page. This
  // should show the side panel button in the toolbar.
  AppendTab(browser(), google_search_url());
  ActivateTabAt(browser(), 2);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  NavigateActiveTab(browser(), non_google_url());
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
  NavigateActiveTab(browser(), google_search_url());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-Google page and the
  // current tab has previously encountered a Google search page.
  NavigateActiveTab(browser(), non_google_url());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Toggle the side panel.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.OpenAction",
      SideSearchOpenActionType::kTapOnSideSearchToolbarButton, 1);

  // Toggling the button again should close the side panel.
  NotifyButtonClick(browser());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.CloseAction",
      SideSearchCloseActionType::kTapOnSideSearchToolbarButton, 1);
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelTogglesCorrectlyMultipleTabs) {
  // Navigate to a Google search URL followed by a non-Google URL in two
  // independent browser tabs such that both have the side panel ready.

  // Tab 1.
  NavigateActiveTab(browser(), google_search_url());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), non_google_url());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Tab 2.
  AppendTab(browser(), google_search_url());
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), non_google_url());
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
  histogram_tester_.ExpectBucketCount(
      "SideSearch.CloseAction",
      SideSearchCloseActionType::kTapOnSideSearchCloseButton, 1);
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
  NavigateActiveTab(browser2, google_search_url());
  NavigateActiveTab(browser2, non_google_url());

  EXPECT_EQ(nullptr, GetSidePanelButtonFor(browser2));
  EXPECT_EQ(nullptr, GetSidePanelFor(browser2));
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelButtonIsNotShownWhenSRPIsUnavailable) {
  // Set the side panel SRP be unavailable.
  SetIsSidePanelSRPAvailableAt(browser(), 0, false);

  // The side panel button should never be visible on the Google home page.
  NavigateActiveTab(browser(), google_homepage_url());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // If no previous Google search page has been navigated to the button should
  // not be visible.
  NavigateActiveTab(browser(), non_google_url());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should never be visible on the Google search page.
  NavigateActiveTab(browser(), google_search_url());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should not be visible if the side panel SRP is not
  // available.
  NavigateActiveTab(browser(), non_google_url());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
}

// TODO(crbug.com/1269277): Fix flakiness on Linux and Lacros then reenable.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_OpeningAndClosingTheSidePanelHandlesFocusCorrectly \
  DISABLED_OpeningAndClosingTheSidePanelHandlesFocusCorrectly
#else
#define MAYBE_OpeningAndClosingTheSidePanelHandlesFocusCorrectly \
  OpeningAndClosingTheSidePanelHandlesFocusCorrectly
#endif
IN_PROC_BROWSER_TEST_F(
    SideSearchBrowserControllerTest,
    MAYBE_OpeningAndClosingTheSidePanelHandlesFocusCorrectly) {
  // Navigate to a Google SRP and then a non-Google page. The side panel will be
  // available but closed.
  NavigateToSRPAndNonGoogleUrl(browser());

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
  NotifyButtonClick(browser());
  EXPECT_FALSE(side_panel->GetVisible());
  EXPECT_TRUE(contents_view->HasFocus());
  EXPECT_FALSE(side_panel->Contains(focus_manager->GetFocusedView()));
}

IN_PROC_BROWSER_TEST_F(SideSearchBrowserControllerTest,
                       SidePanelCrashesCloseSidePanel) {
  // Navigate to a Google SRP and then a non-Google page. The side panel will be
  // available but closed.
  NavigateToSRPAndOpenSidePanel(browser());

  auto* side_panel = GetSidePanelFor(browser());

  // Side panel should be open with a hosted WebContents.
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_NE(nullptr, GetSidePanelContentsFor(browser(), 0));

  // Simulate a crash in the side panel contents.
  auto* rph =
      GetSidePanelContentsFor(browser(), 0)->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph->Shutdown(content::RESULT_CODE_KILLED));
  crash_observer.Wait();

  // Side panel should be closed and the WebContents cleared.
  EXPECT_FALSE(side_panel->GetVisible());
  EXPECT_EQ(nullptr, GetSidePanelContentsFor(browser(), 0));

  // Reopening the side panel should restore the side panel and its contents.
  NotifyButtonClick(browser());
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_NE(nullptr, GetSidePanelContentsFor(browser(), 0));
}

// Base class for Extensions API tests for the side panel WebContents.
class SideSearchExtensionsTest : public SideSearchBrowserControllerTest {
 public:
  void SetUpOnMainThread() override {
    SideSearchBrowserControllerTest::SetUpOnMainThread();
    // We want all navigations to be routed through the side panel for the
    // purposes of testing extension support.
    auto* config = SideSearchConfig::Get(browser()->profile());
    config->SetShouldNavigateInSidePanelCalback(
        base::BindRepeating([](const GURL& url) { return true; }));
    config->SetCanShowSidePanelForURLCallback(
        base::BindRepeating([](const GURL& url) { return true; }));

    // Navigate to the first URL and open the side panel. This should create and
    // initiate a navigation in the side panel WebContents.
    NavigateActiveTab(browser(),
                      embedded_test_server()->GetURL("initial.example", "/"));
    NotifyButtonClick(browser());
    EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
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

IN_PROC_BROWSER_TEST_F(SideSearchExtensionsTest,
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

IN_PROC_BROWSER_TEST_F(SideSearchExtensionsTest,
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

IN_PROC_BROWSER_TEST_F(SideSearchExtensionsTest,
                       DeclarativeNetRequestInterceptsSidePanelNavigations) {
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

class SideSearchStatePerTabBrowserControllerTest
    : public SideSearchBrowserControllerTest {
 public:
  // SideSearchBrowserControllerTest:
  void InitializeFeatureList(
      base::test::ScopedFeatureList& feature_list) override {
    feature_list.InitWithFeatures(
        {features::kSideSearch, features::kSideSearchStatePerTab}, {});
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
  NavigateActiveTab(browser(), google_search_url());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), non_google_url());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Tab 2.
  AppendTab(browser(), google_search_url());
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), non_google_url());
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

IN_PROC_BROWSER_TEST_F(SideSearchStatePerTabBrowserControllerTest,
                       SwitchingTabsHandlesFocusCorrectly) {
  auto* browser_view = BrowserViewFor(browser());
  auto* side_panel = GetSidePanelFor(browser());
  auto* contents_view = browser_view->contents_web_view();
  auto* focus_manager = browser_view->GetFocusManager();
  ASSERT_NE(nullptr, focus_manager);

  // The side panel should currently have focus as it was opened via the toolbar
  // button.
  NavigateToSRPAndOpenSidePanel(browser());
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_TRUE(side_panel->Contains(focus_manager->GetFocusedView()));
  EXPECT_FALSE(contents_view->HasFocus());

  // Switch to another tab and open the side panel. The side panel should still
  // have focus as it was opened via the toolbar button.
  AppendTab(browser(), non_google_url());
  ActivateTabAt(browser(), 1);
  NavigateToSRPAndOpenSidePanel(browser());
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

IN_PROC_BROWSER_TEST_F(SideSearchStatePerTabBrowserControllerTest,
                       SidePanelTogglesClosedCorrectlyDuringNavigation) {
  // Navigate to a Google SRP and then a non-Google page. The side panel will be
  // available and open.
  NavigateToSRPAndOpenSidePanel(browser());
  auto* side_panel = GetSidePanelFor(browser());

  // Navigating to a Google SRP URL should automatically hide the side panel as
  // it should not be available.
  EXPECT_TRUE(side_panel->GetVisible());
  NavigateActiveTab(browser(), google_search_url());
  EXPECT_FALSE(side_panel->GetVisible());

  // When navigating again to a non-Google / non-NTP page the side panel will
  // become available again but should not automatically reopen.
  NavigateActiveTab(browser(), google_search_url());
  EXPECT_FALSE(side_panel->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchStatePerTabBrowserControllerTest,
                       SidePanelCrashesCloseSidePanel) {
  // Open two tabs with the side panel open.
  NavigateToSRPAndOpenSidePanel(browser());
  AppendTab(browser(), non_google_url());
  ActivateTabAt(browser(), 1);
  NavigateToSRPAndOpenSidePanel(browser());

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
