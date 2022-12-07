// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/side_search/side_search_browsertest.h"
#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"

class SideSearchBrowserControllerTest
    : public SideSearchBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  // InProcessBrowserTest:
  void SetUp() override {
    const bool enable_dse_support = GetParam();
    if (enable_dse_support) {
      scoped_feature_list_.InitWithFeatures(
          {features::kSideSearch, features::kSideSearchDSESupport},
          {features::kUnifiedSidePanel});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kSideSearch},
          {features::kSideSearchDSESupport, features::kUnifiedSidePanel});
    }
    InProcessBrowserTest::SetUp();
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SideSearchBrowserControllerTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelButtonShowsCorrectlySingleTab) {
  // If no previous matched search page has been navigated to the button should
  // not be visible.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());

  // The side panel button should never be visible on a matched search page.
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-matched page and the
  // current tab has previously encountered a matched search page.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  histogram_tester_.ExpectBucketCount(
      "SideSearch.AvailabilityChanged",
      SideSearchAvailabilityChangeType::kBecomeAvailable, 1);
}

// TODO(crbug.com/1340387): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SidePanelButtonShowsCorrectlyMultipleTabs \
  DISABLED_SidePanelButtonShowsCorrectlyMultipleTabs
#else
#define MAYBE_SidePanelButtonShowsCorrectlyMultipleTabs \
  SidePanelButtonShowsCorrectlyMultipleTabs
#endif
IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       MAYBE_SidePanelButtonShowsCorrectlyMultipleTabs) {
  // The side panel button should never be visible on non-matching pages.
  AppendTab(browser(), GetNonMatchingUrl());
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());

  // Navigate to a matched search page and then to a non-matched search page.
  // This should show the side panel button in the toolbar.
  AppendTab(browser(), GetMatchingSearchUrl());
  ActivateTabAt(browser(), 2);
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());

  // Switch back to the matched search page, the side panel button should no
  // longer be visible.
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());

  // When switching back to the tab on the non-matched page with a previously
  // visited matched search page, the button should be visible.
  ActivateTabAt(browser(), 2);
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       SidePanelTogglesCorrectlySingleTab) {
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The side panel button should be visible if on a non-matched page and the
  // current tab has previously encountered a matched search page.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
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
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
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

  EXPECT_EQ(nullptr, GetSideSearchButtonFor(browser2));
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

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1340387): Test is flaky on Mac.
#define MAYBE_SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows \
  DISABLED_SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows
#else
#define MAYBE_SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows \
  SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows
#endif
IN_PROC_BROWSER_TEST_P(
    SideSearchBrowserControllerTest,
    MAYBE_SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows) {
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());

  Browser* browser2 = CreateBrowser(browser()->profile());
  NavigateToMatchingAndNonMatchingSearchPage(browser2);

  std::unique_ptr<content::WebContents> web_contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(0);
  browser()->tab_strip_model()->InsertWebContentsAt(1, std::move(web_contents),
                                                    AddTabTypes::ADD_ACTIVE);

  ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 0);
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1348296): Test is flaky on Mac.
#define MAYBE_SidePanelTogglesCorrectlyMultipleTabs \
  DISABLED_SidePanelTogglesCorrectlyMultipleTabs
#else
#define MAYBE_SidePanelTogglesCorrectlyMultipleTabs \
  SidePanelTogglesCorrectlyMultipleTabs
#endif
IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       MAYBE_SidePanelTogglesCorrectlyMultipleTabs) {
  // Navigate to a matching search URL followed by a non-matching URL in two
  // independent browser tabs such that both have the side panel ready. The
  // side panel should respect the state-per-tab flag.

  // Tab 1.
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Tab 2.
  AppendTab(browser(), GetMatchingSearchUrl());
  ActivateTabAt(browser(), 1);
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Show the side panel on Tab 2 and switch to Tab 1. The side panel should
  // not be visible for Tab 1.
  NotifyButtonClick(browser());
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.TimeSinceSidePanelAvailableToFirstOpen", 1);

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
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
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.SidePanel.TimeShownOpenedViaTabSwitch", 1);

  ActivateTabAt(browser(), 0);
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester_.ExpectTotalCount(
      "SideSearch.SidePanel.TimeShownOpenedViaTabSwitch", 2);
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1341272): Test is flaky on Mac.
#define MAYBE_SwitchingTabsHandlesFocusCorrectly \
  DISABLED_SwitchingTabsHandlesFocusCorrectly
#else
#define MAYBE_SwitchingTabsHandlesFocusCorrectly \
  SwitchingTabsHandlesFocusCorrectly
#endif
IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       MAYBE_SwitchingTabsHandlesFocusCorrectly) {
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

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1340387): Test is flaky on Mac.
#define MAYBE_SidePanelCrashesCloseSidePanel \
  DISABLED_SidePanelCrashesCloseSidePanel
#else
#define MAYBE_SidePanelCrashesCloseSidePanel SidePanelCrashesCloseSidePanel
#endif
IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       MAYBE_SidePanelCrashesCloseSidePanel) {
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
  auto* rph_second_tab = GetSidePanelContentsFor(browser(), 1)
                             ->GetPrimaryMainFrame()
                             ->GetProcess();
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
  auto* rph_first_tab = GetSidePanelContentsFor(browser(), 0)
                            ->GetPrimaryMainFrame()
                            ->GetProcess();
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

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       CarryOverSideSearchToNewTabFromSideSearchPanel) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Set up srp tab.
  const GURL srp_tab_url(GetMatchingSearchUrl());

  // Set up a mock search result on side search panel.
  const GURL new_tab_url(GetNonMatchingUrl());

  NavigateActiveTab(browser(), srp_tab_url);

  // Navigate current tab to a random non-srp page.
  NavigateActiveTab(browser(), GetNonMatchingUrl());

  // Toggle the side panel.
  NotifyButtonClick(browser());
  ASSERT_TRUE(GetSidePanelFor(browser())->GetVisible());

  content::WebContents* active_side_contents =
      GetActiveSidePanelWebContents(browser());

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.link_url = new_tab_url;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(*active_side_contents->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  content::WebContents* new_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));

  // Verify that the new tab is correct.
  ASSERT_EQ(new_tab_url, new_tab->GetLastCommittedURL());

  // Verify that new tab has page action icon displayed.
  ActivateTabAt(browser(), 1);
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());

  // Verify new_tab_helper has correct last_search_url_.
  auto* new_tab_helper = SideSearchTabContentsHelper::FromWebContents(new_tab);
  ASSERT_TRUE(new_tab_helper);
  EXPECT_EQ(new_tab_helper->last_search_url(), srp_tab_url);
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       CarryOverSideSearchToNewWindowFromSideSearchPanel) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Set up srp tab.
  const GURL srp_tab_url(GetMatchingSearchUrl());

  // Set up a mock search result on side search panel.
  const GURL new_tab_url(GetNonMatchingUrl());

  NavigateActiveTab(browser(), srp_tab_url);

  // Navigate current tab to a random non-srp page.
  NavigateActiveTab(browser(), GetNonMatchingUrl());

  // Toggle the side panel.
  NotifyButtonClick(browser());
  ASSERT_TRUE(GetSidePanelFor(browser())->GetVisible());

  content::WebContents* active_side_contents =
      GetActiveSidePanelWebContents(browser());

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.link_url = new_tab_url;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(*active_side_contents->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW, 0);

  content::WebContents* new_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));

  // Verify that the new tab is correct.
  ASSERT_EQ(new_tab_url, new_tab->GetLastCommittedURL());

  // Verify that new window has page action icon displayed.
  EXPECT_TRUE(
      GetSideSearchButtonFor(chrome::FindBrowserWithWebContents(new_tab))
          ->GetVisible());

  // Verify new_tab_helper has correct last_search_url_.
  auto* new_tab_helper = SideSearchTabContentsHelper::FromWebContents(new_tab);
  ASSERT_TRUE(new_tab_helper);
  EXPECT_EQ(new_tab_helper->last_search_url(), srp_tab_url);
}

IN_PROC_BROWSER_TEST_P(
    SideSearchBrowserControllerTest,
    SideSearchNotCarriedOverToIncognitoWindowFromSideSearchPanel) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Set up srp tab.
  const GURL srp_tab_url(GetMatchingSearchUrl());

  // Set up a mock search result on side search panel.
  const GURL new_tab_url(GetNonMatchingUrl());

  NavigateActiveTab(browser(), srp_tab_url);

  // Navigate current tab to a random non-srp page.
  NavigateActiveTab(browser(), GetNonMatchingUrl());

  // Toggle the side panel.
  NotifyButtonClick(browser());
  ASSERT_TRUE(GetSidePanelFor(browser())->GetVisible());

  content::WebContents* active_side_contents =
      GetActiveSidePanelWebContents(browser());

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.link_url = new_tab_url;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(*active_side_contents->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* new_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));

  // Verify that the new tab is correct.
  ASSERT_EQ(new_tab_url, new_tab->GetLastCommittedURL());

  // Verify that new window has page action icon displayed.
  EXPECT_FALSE(
      GetSideSearchButtonFor(chrome::FindBrowserWithWebContents(new_tab)));
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       DisplayPageActionIconInNewTab) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Set up srp tab.
  const GURL srp_tab(GetMatchingSearchUrl());

  // Set up a mock search result from srp.
  const GURL new_tab(GetNonMatchingUrl());

  // Navigate browser to srp.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), srp_tab));

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.link_url = new_tab;

  // Select "Open Link in New Tab" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that the new tab is correct.
  ASSERT_EQ(new_tab, tab->GetLastCommittedURL());

  // Verify that new tab has page action icon displayed.
  ActivateTabAt(browser(), 1);
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());

  // Verify new_tab_helper has correct last_search_url_.
  auto* new_tab_helper = SideSearchTabContentsHelper::FromWebContents(tab);
  ASSERT_TRUE(new_tab_helper);
  EXPECT_EQ(new_tab_helper->last_search_url(), srp_tab);
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       DisplayPageActionIconInNewWindow) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Set up srp tab.
  const GURL srp_tab(GetMatchingSearchUrl());

  // Set up a mock search result from srp.
  const GURL new_tab(GetNonMatchingUrl());

  // Navigate browser to srp.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), srp_tab));

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.link_url = new_tab;

  // Select "Open Link in New Window" and wait for the new tab to be added.
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that the new tab is correct.
  ASSERT_EQ(new_tab, tab->GetLastCommittedURL());

  // Verify that new window has page action icon displayed.
  EXPECT_TRUE(GetSideSearchButtonFor(chrome::FindBrowserWithWebContents(tab))
                  ->GetVisible());

  // Verify new_tab_helper has correct last_search_url_.
  auto* new_tab_helper = SideSearchTabContentsHelper::FromWebContents(tab);
  ASSERT_TRUE(new_tab_helper);
  EXPECT_EQ(new_tab_helper->last_search_url(), srp_tab);
}

IN_PROC_BROWSER_TEST_P(SideSearchBrowserControllerTest,
                       NoPageActionIconInIncognitoWindow) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Set up srp tab.
  const GURL srp_tab(GetMatchingSearchUrl());

  // Set up a mock search result from srp.
  const GURL new_tab(GetNonMatchingUrl());

  // Navigate browser to srp.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), srp_tab));

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.link_url = new_tab;

  // Select "Open Link in Incognito Window" and wait for the new tab to be
  // added.
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that the new tab is correct.
  ASSERT_EQ(new_tab, tab->GetLastCommittedURL());

  // Verify that new window has page action icon displayed.
  EXPECT_FALSE(GetSideSearchButtonFor(chrome::FindBrowserWithWebContents(tab)));
}

// Only tested for the Side Search DSE configuration.
class SideSearchIconViewTest : public SideSearchBrowserTest {
 public:
  // SideSearchBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kSideSearch, features::kSideSearchDSESupport},
        {features::kUnifiedSidePanel});
    SideSearchBrowserTest::SetUp();
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that metrics correctly capture whether the label was visible when the
// entrypoint was toggled.
IN_PROC_BROWSER_TEST_F(SideSearchIconViewTest,
                       LabelVisibilityMetricsCorrectlyEmittedWhenToggled) {
  auto* button_view = GetSideSearchButtonFor(browser());
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

// Fixture for testing side panel clobbering behavior with global panels. Only
// tested for the Side Search DSE configuration.
class SideSearchDSEClobberingTest : public SideSearchBrowserTest {
 public:
  // SideSearchBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kSideSearch, features::kSideSearchDSESupport,
         features::kSidePanelImprovedClobbering},
        {features::kUnifiedSidePanel});
    SideSearchBrowserTest::SetUp();
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
            kSidePanelButtonElementId, browser->window()->GetElementContext());
    return button_view ? views::AsViewClass<SidePanelToolbarButton>(button_view)
                       : nullptr;
  }

  SidePanel* GetGlobalSidePanelFor(Browser* browser) {
    return BrowserViewFor(browser)->unified_side_panel();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SideSearchDSEClobberingTest,
                       GlobalBrowserSidePanelIsToggleable) {
  auto* global_panel = GetGlobalSidePanelFor(browser());
  EXPECT_FALSE(global_panel->GetVisible());
  ShowGlobalSidePanel(browser());
  EXPECT_TRUE(global_panel->GetVisible());
}

// Flaky on Mac: https://crbug.com/1340387
#if BUILDFLAG(IS_MAC)
#define MAYBE_ContextualPanelsDoNotClobberGlobalPanels \
  DISABLED_ContextualPanelsDoNotClobberGlobalPanels
#else
#define MAYBE_ContextualPanelsDoNotClobberGlobalPanels \
  ContextualPanelsDoNotClobberGlobalPanels
#endif
IN_PROC_BROWSER_TEST_F(SideSearchDSEClobberingTest,
                       MAYBE_ContextualPanelsDoNotClobberGlobalPanels) {
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

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1340387): Test is flaky on Mac.
#define MAYBE_OpeningGlobalPanelsClosesAllContextualPanels \
  DISABLED_OpeningGlobalPanelsClosesAllContextualPanels
#else
#define MAYBE_OpeningGlobalPanelsClosesAllContextualPanels \
  OpeningGlobalPanelsClosesAllContextualPanels
#endif
IN_PROC_BROWSER_TEST_F(SideSearchDSEClobberingTest,
                       MAYBE_OpeningGlobalPanelsClosesAllContextualPanels) {
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

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1340387): Test is flaky on Mac.
#define MAYBE_ContextualAndGlobalPanelsBehaveAsExpectedWhenDraggingBetweenWindows \
  DISABLED_ContextualAndGlobalPanelsBehaveAsExpectedWhenDraggingBetweenWindows
#else
#define MAYBE_ContextualAndGlobalPanelsBehaveAsExpectedWhenDraggingBetweenWindows \
  ContextualAndGlobalPanelsBehaveAsExpectedWhenDraggingBetweenWindows
#endif
IN_PROC_BROWSER_TEST_F(
    SideSearchDSEClobberingTest,
    MAYBE_ContextualAndGlobalPanelsBehaveAsExpectedWhenDraggingBetweenWindows) {
  // Open two browsers with three tabs each. Both have open global side panel
  // and an open side search panel for their last tab.
  Browser* browser2 = CreateBrowser(browser()->profile());
  SetupBrowserForClobberingTests(browser());
  SetupBrowserForClobberingTests(browser2);

  // Move the currently active tab with side search from browser2 to browser1.
  std::unique_ptr<content::WebContents> web_contents =
      browser2->tab_strip_model()->DetachWebContentsAtForInsertion(2);
  browser()->tab_strip_model()->InsertWebContentsAt(3, std::move(web_contents),
                                                    AddTabTypes::ADD_ACTIVE);

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

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1340387): Test is flaky on Mac.
#define MAYBE_ClosingTheContextualPanelClosesAllBrowserPanels \
  DISABLED_ClosingTheContextualPanelClosesAllBrowserPanels
#else
#define MAYBE_ClosingTheContextualPanelClosesAllBrowserPanels \
  ClosingTheContextualPanelClosesAllBrowserPanels
#endif
IN_PROC_BROWSER_TEST_F(SideSearchDSEClobberingTest,
                       MAYBE_ClosingTheContextualPanelClosesAllBrowserPanels) {
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

// Flaky on Mac: https://crbug.com/1340387
#if BUILDFLAG(IS_MAC)
#define MAYBE_ClosingTheGlobalPanelClosesAllBrowserPanels \
  DISABLED_ClosingTheGlobalPanelClosesAllBrowserPanels
#else
#define MAYBE_ClosingTheGlobalPanelClosesAllBrowserPanels \
  ClosingTheGlobalPanelClosesAllBrowserPanels
#endif
IN_PROC_BROWSER_TEST_F(SideSearchDSEClobberingTest,
                       MAYBE_ClosingTheGlobalPanelClosesAllBrowserPanels) {
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
