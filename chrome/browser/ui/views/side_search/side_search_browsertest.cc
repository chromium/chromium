// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browsertest.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace {

constexpr char kSearchMatchPath[] = "/search-match";
constexpr char kNonMatchPath[] = "/non-match";

bool IsSearchURLMatch(const GURL& url) {
  // Test via path prefix match as the embedded test server ensures that all
  // URLs are using the same host and paths are made unique via appending a
  // monotonically increasing value to the end of their paths.
  return url.path().find(kSearchMatchPath) != std::string::npos;
}

}  // namespace

void SideSearchBrowserTest::SetUpOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->RegisterDefaultHandler(base::BindRepeating(
      &SideSearchBrowserTest::HandleRequest, base::Unretained(this)));
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
  config->set_skip_on_template_url_changed_for_testing(true);
}

void SideSearchBrowserTest::TearDownOnMainThread() {
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  InProcessBrowserTest::TearDownOnMainThread();
}

void SideSearchBrowserTest::ActivateTabAt(Browser* browser, int index) {
  browser->tab_strip_model()->ActivateTabAt(index);
}

void SideSearchBrowserTest::AppendTab(Browser* browser, const GURL& url) {
  chrome::AddTabAt(browser, url, -1, true);
}

void SideSearchBrowserTest::NavigateActiveTab(Browser* browser,
                                              const GURL& url,
                                              bool is_renderer_initiated) {
  if (is_renderer_initiated) {
    // Navigate from a link and wait for loading to finish.
    content::TestNavigationObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    NavigateParams params(browser, url,
                          ui::PageTransition::PAGE_TRANSITION_LINK);
    params.initiator_origin = url::Origin();
    params.is_renderer_initiated = true;
    ui_test_utils::NavigateToURL(&params);
    observer.WaitForNavigationFinished();
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  }
}

content::WebContents* SideSearchBrowserTest::GetActiveSidePanelWebContents(
    Browser* browser) {
  auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents());
  return tab_contents_helper->side_panel_contents_for_testing();
}

void SideSearchBrowserTest::NavigateActiveSideContents(Browser* browser,
                                                       const GURL& url) {
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

void SideSearchBrowserTest::NotifyButtonClick(Browser* browser) {
  views::test::ButtonTestApi(GetSideSearchButtonFor(browser))
      .NotifyClick(GetDummyEvent());
  BrowserViewFor(browser)->GetWidget()->LayoutRootViewIfNecessary();
}

void SideSearchBrowserTest::NotifyCloseButtonClick(Browser* browser) {
  ASSERT_TRUE(GetSidePanelFor(browser)->GetVisible());
  views::test::ButtonTestApi(GetSideButtonClosePanelFor(browser))
      .NotifyClick(GetDummyEvent());
  BrowserViewFor(browser)->GetWidget()->LayoutRootViewIfNecessary();
}

void SideSearchBrowserTest::NotifyReadLaterButtonClick(Browser* browser) {
  views::test::ButtonTestApi(GetSidePanelButtonFor(browser))
      .NotifyClick(GetDummyEvent());
}

BrowserView* SideSearchBrowserTest::BrowserViewFor(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser);
}

views::Button* SideSearchBrowserTest::GetSideSearchButtonFor(Browser* browser) {
  views::View* button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kSideSearchButtonElementId, browser->window()->GetElementContext());
  return button_view ? views::AsViewClass<views::Button>(button_view) : nullptr;
}

views::Button* SideSearchBrowserTest::GetSidePanelButtonFor(Browser* browser) {
  views::View* button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kToolbarSidePanelButtonElementId,
          browser->window()->GetElementContext());
  return button_view ? views::AsViewClass<views::Button>(button_view) : nullptr;
}

// Extract the testing of the entrypoint when the side panel is open into its
// own method as this will vary depending on whether or not the DSE support
// flag is enabled. For e.g. when DSE support is enabled and the side panel is
// open the omnibox button is hidden while if DSE support is disabled the
// toolbar button is visible.
void SideSearchBrowserTest::TestSidePanelOpenEntrypointState(Browser* browser) {
  // If the side panel is visible and DSE support is enabled then the
  // entrypoint should be hidden. Otherwise the entrypoint should be visible.
  if (IsSideSearchEnabled(browser->profile())) {
    EXPECT_FALSE(GetSideSearchButtonFor(browser)->GetVisible());
  } else {
    EXPECT_TRUE(GetSideSearchButtonFor(browser)->GetVisible());
  }
}

views::Button* SideSearchBrowserTest::GetSideButtonClosePanelFor(
    Browser* browser) {
  views::View* button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kSidePanelCloseButtonElementId,
          browser->window()->GetElementContext());
  return button_view ? views::AsViewClass<views::Button>(button_view) : nullptr;
}

SidePanel* SideSearchBrowserTest::GetSidePanelFor(Browser* browser) {
  return BrowserViewFor(browser)->unified_side_panel();
}

content::WebContents* SideSearchBrowserTest::GetSidePanelContentsFor(
    Browser* browser,
    int index) {
  auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
      browser->tab_strip_model()->GetWebContentsAt(index));
  return tab_contents_helper->side_panel_contents_for_testing();
}

void SideSearchBrowserTest::NavigateToMatchingAndNonMatchingSearchPage(
    Browser* browser) {
  // The side panel button should never be visible on the matched search page.
  NavigateActiveTab(browser, GetMatchingSearchUrl());
  EXPECT_FALSE(GetSideSearchButtonFor(browser)->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser)->GetVisible());

  // The side panel button should be visible if on a non-matched page and the
  // current tab has previously encountered a matched search page.
  NavigateActiveTab(browser, GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser)->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser)->GetVisible());
}

void SideSearchBrowserTest::NavigateToMatchingSearchPageAndOpenSidePanel(
    Browser* browser) {
  NavigateToMatchingAndNonMatchingSearchPage(browser);

  NotifyButtonClick(browser);
  TestSidePanelOpenEntrypointState(browser);
  EXPECT_TRUE(GetSidePanelFor(browser)->GetVisible());
  EXPECT_TRUE(content::WaitForLoadStop(GetActiveSidePanelWebContents(browser)));
}

GURL SideSearchBrowserTest::GetMatchingSearchUrl() {
  // Ensure that each returned matching URL is unique.
  static int id = 1;
  return embedded_test_server()->GetURL(
      base::StrCat({kSearchMatchPath, base::NumberToString(id++)}));
}

GURL SideSearchBrowserTest::GetNonMatchingUrl() {
  // Ensure that each returned non-matching URL is unique.
  static int id = 1;
  return embedded_test_server()->GetURL(
      base::StrCat({kNonMatchPath, base::NumberToString(id++)}));
}

ui::MouseEvent SideSearchBrowserTest::GetDummyEvent() const {
  return ui::MouseEvent(ui::EventType::kMousePressed, gfx::PointF(),
                        gfx::PointF(), base::TimeTicks::Now(), 0, 0);
}

std::unique_ptr<net::test_server::HttpResponse>
SideSearchBrowserTest::HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  return std::move(http_response);
}
