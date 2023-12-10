// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/views/side_search/unified_side_search_controller.h"

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_search/side_search_browsertest.h"
#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"

// Fixture for testing side panel v2 only. Only instantiate tests for DSE
// configuration.
class SideSearchV2Test : public SideSearchBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kSideSearch, features::kSearchWebInSidePanel}, {});
    SideSearchBrowserTest::SetUp();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
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

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
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
  EXPECT_TRUE(GetSideSearchButtonFor(chrome::FindBrowserWithTab(new_tab))
                  ->GetVisible());

  // Verify new_tab_helper has correct last_search_url_.
  auto* new_tab_helper = SideSearchTabContentsHelper::FromWebContents(new_tab);
  ASSERT_TRUE(new_tab_helper);
  EXPECT_EQ(new_tab_helper->last_search_url(), srp_tab_url);
}

IN_PROC_BROWSER_TEST_F(
    SideSearchV2Test,
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
  EXPECT_FALSE(GetSideSearchButtonFor(chrome::FindBrowserWithTab(new_tab)));
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, DisplayPageActionIconInNewTab) {
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

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, DisplayPageActionIconInNewWindow) {
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
  EXPECT_TRUE(
      GetSideSearchButtonFor(chrome::FindBrowserWithTab(tab))->GetVisible());

  // Verify new_tab_helper has correct last_search_url_.
  auto* new_tab_helper = SideSearchTabContentsHelper::FromWebContents(tab);
  ASSERT_TRUE(new_tab_helper);
  EXPECT_EQ(new_tab_helper->last_search_url(), srp_tab);
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, NoPageActionIconInIncognitoWindow) {
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
  EXPECT_FALSE(GetSideSearchButtonFor(chrome::FindBrowserWithTab(tab)));
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
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
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
                       SidePanelButtonShowsCorrectlyMultipleTabs) {
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

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, SidePanelTogglesCorrectlySingleTab) {
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

  // Toggling the close button should close the side panel.
  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, CloseButtonClosesSidePanel) {
  // The close button should be visible in the toggled state.
  NavigateToMatchingSearchPageAndOpenSidePanel(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  NotifyCloseButtonClick(browser());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, SideSearchNotAvailableInOTR) {
  Browser* browser2 = CreateIncognitoBrowser();
  EXPECT_TRUE(browser2->profile()->IsOffTheRecord());
  NavigateActiveTab(browser2, GetMatchingSearchUrl());
  NavigateActiveTab(browser2, GetNonMatchingUrl());

  EXPECT_EQ(nullptr, GetSideSearchButtonFor(browser2));
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
                       SearchWebInSidePanelNotAvailableInOTR) {
  Browser* browser2 = CreateIncognitoBrowser();
  EXPECT_TRUE(browser2->profile()->IsOffTheRecord());
  auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
      browser2->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(nullptr, tab_contents_helper);
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, MenuEntryPointNotAvailableOnSRP) {
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(tab_contents_helper->CanShowSidePanelFromContextMenuSearch());
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
                       MenuEntryPointAvailableOnPageWithoutSRP) {
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(tab_contents_helper->CanShowSidePanelFromContextMenuSearch());
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
                       MenuEntryPointDisplayAndUpdateSidePanel) {
  // Initially side panel does not exist.
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
  auto* helper = SideSearchTabContentsHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  GURL matchingURL_1 = GetMatchingSearchUrl();
  helper->OpenSidePanelFromContextMenuSearch(matchingURL_1);

  // Clicking menu entrypoint displays search results in side panel.
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  EXPECT_EQ(matchingURL_1,
            GetActiveSidePanelWebContents(browser())->GetVisibleURL());

  // Clicking menu entrypoint with a newly-generated search URL updates the
  // existing side panel.
  GURL matchingURL_2 = GetMatchingSearchUrl();
  helper->OpenSidePanelFromContextMenuSearch(matchingURL_2);
  EXPECT_EQ(matchingURL_2,
            GetActiveSidePanelWebContents(browser())->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    SideSearchV2Test,
    SidePanelStatePreservedWhenMovingTabsAcrossBrowserWindows) {
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

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
                       SidePanelTogglesCorrectlyMultipleTabs) {
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

  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // Show the side panel on Tab 1 and switch to Tab 2. The side panel should be
  // still be visible for Tab 2, respecting its per-tab state.
  NotifyButtonClick(browser());
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 1);
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  // Close the side panel on Tab 2 and switch to Tab 1. The side panel should be
  // still be visible for Tab 1, respecting its per-tab state.
  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  ActivateTabAt(browser(), 0);
  TestSidePanelOpenEntrypointState(browser());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());

  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
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

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, SideSearchCrashesCloseSideSearch) {
  auto* coordinator = side_panel_coordinator();
  coordinator->SetNoDelaysForTesting(true);

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

  // Side panel should be closed and the crashed WebContents cleared.
  EXPECT_FALSE(side_panel->GetVisible());
  EXPECT_EQ(nullptr, GetSidePanelContentsFor(browser(), 1));
  EXPECT_NE(nullptr, GetSidePanelContentsFor(browser(), 0));

  // Reopen side panel.
  coordinator->Show();
  EXPECT_TRUE(side_panel->GetVisible());

  // Simulate a crash in the side panel contents of the first tab which is not
  // currently active.
  EXPECT_NE(nullptr, GetSidePanelContentsFor(browser(), 0));
  auto* rph_first_tab = GetSidePanelContentsFor(browser(), 0)
                            ->GetPrimaryMainFrame()
                            ->GetProcess();
  content::RenderProcessHostWatcher crash_observer_first_tab(
      rph_first_tab, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph_first_tab->Shutdown(content::RESULT_CODE_KILLED));
  crash_observer_first_tab.Wait();

  // Switch to the first tab, the side panel should still be open.
  ActivateTabAt(browser(), 0);
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_EQ(nullptr, GetSidePanelContentsFor(browser(), 0));

  // Reopening the side panel should restore the side panel and its contents.
  NotifyButtonClick(browser());
  EXPECT_TRUE(side_panel->GetVisible());
  EXPECT_NE(nullptr, GetSidePanelContentsFor(browser(), 0));
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, SwitchSidePanelInSingleTab) {
  auto* coordinator = side_panel_coordinator();
  coordinator->SetNoDelaysForTesting(true);

  // Tab 0 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Switch to reading list side panel.
  coordinator->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Switch back to side search side panel.
  coordinator->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, SwitchTabsWithGlobalSidePanel) {
  auto* coordinator = side_panel_coordinator();
  coordinator->SetNoDelaysForTesting(true);

  // Tab 0 without side search available and open with reading list.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());
  coordinator->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Tab 1 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Tab 2 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Tab 3 with side search available but not open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Switch to tab 0, side panel is open with reading list.
  ActivateTabAt(browser(), 0);
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Switch to tab 1, side panel is open with side search.
  ActivateTabAt(browser(), 1);
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Switch to tab 2, side panel is open with side search.
  ActivateTabAt(browser(), 2);
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Switch to tab 3, side panel is open with reading list.
  ActivateTabAt(browser(), 3);
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, SwitchTabsWithoutGlobalSidePanel) {
  auto* coordinator = side_panel_coordinator();

  // Tab 0 without side search available.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_FALSE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_EQ(nullptr, coordinator->GetCurrentSidePanelEntryForTesting());

  // Tab 1 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Tab 2 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Tab 3 with side search available but not open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  EXPECT_EQ(nullptr, coordinator->GetCurrentSidePanelEntryForTesting());

  // Switch to tab 0, side panel is closed.
  ActivateTabAt(browser(), 0);
  EXPECT_EQ(nullptr, coordinator->GetCurrentSidePanelEntryForTesting());

  // Switch to tab 1, side panel is open with side search.
  ActivateTabAt(browser(), 1);
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Switch to tab 2, side panel is open with side search.
  ActivateTabAt(browser(), 2);
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->key().id());

  // Switch to tab 3, side panel is closed.
  ActivateTabAt(browser(), 3);
  EXPECT_EQ(nullptr, coordinator->GetCurrentSidePanelEntryForTesting());
}

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, CloseSidePanelShouldClearCache) {
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id());

  // When side panel is open,  side panel web contents is present.
  auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(nullptr, tab_contents_helper->side_panel_contents_for_testing());

  side_panel_coordinator()->Close();

  // When side panel is closed, side panel web contents is destroyed.
  EXPECT_EQ(nullptr, tab_contents_helper->side_panel_contents_for_testing());
}

// Test added for crbug.com/1349687 .
IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
                       NewForegroundTabShouldNotDestroySidePanelContents) {
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id());

  // When side panel is open,  side panel web contents is present.
  auto* tab_contents_helper = SideSearchTabContentsHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(nullptr, tab_contents_helper->side_panel_contents_for_testing());

  // Open URL with a new foreground tab.
  tab_contents_helper->side_panel_contents_for_testing()->OpenURL(
      content::OpenURLParams(embedded_test_server()->GetURL("/foo"),
                             content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_TYPED, false));

  // When swtiched to the new tab, side panel web contents should not be
  // destroyed. Otherwise a UAF will occur.
  EXPECT_NE(nullptr, tab_contents_helper->side_panel_contents_for_testing());
}

// Test added for crbug.com/1356966 .
IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
                       CloseTabWithSideSearchOpenShouldNotCrash) {
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSideSearchButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id());
  browser()->tab_strip_model()->CloseAllTabs();
}

IN_PROC_BROWSER_TEST_F(
    SideSearchV2Test,
    SidePanelAvailabilityChangedShouldNotCloseSidePanelWhenSideSearchIsNotOpen) {
  auto* coordinator = side_panel_coordinator();
  coordinator->SetNoDelaysForTesting(true);
  coordinator->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  auto* side_search_controller = UnifiedSideSearchController::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  side_search_controller->SidePanelAvailabilityChanged(true);
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
}

// Fixture base to test feature engagement functionality for the side search
// feature.
class SideSearchFeatureEngagementTest : public SideSearchBrowserTest {
 public:
  SideSearchFeatureEngagementTest()
      : subscription_(
            BrowserContextDependencyManager::GetInstance()
                ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                    &SideSearchFeatureEngagementTest::RegisterTestTracker))) {}

  // Navigates one page backwards in navigation history and waits for the
  // navigation to complete.
  void GoBackInActiveTabFor(Browser* browser) {
    auto* tab_contents = browser->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver tab_observer(tab_contents);
    tab_contents->GetController().GoBack();
    tab_observer.Wait();
  }

  void NavigateActiveTabRendererInitiated(Browser* browser, const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  }

  std::map<std::string, std::string> GetFeatureEngagementParams() {
    return {
        {"availability", "any"},
        {"event_used", "name:used;comparator:any;window:360;storage:360"},
        {"event_trigger", "name:trigger;comparator:any;window:360;storage:360"},
        {"session_rate", "<1"}};
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  static void RegisterTestTracker(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestTracker));
  }

  static std::unique_ptr<KeyedService> CreateTestTracker(
      content::BrowserContext*) {
    return feature_engagement::CreateTestTracker();
  }

  base::HistogramTester histogram_tester_;
  base::CallbackListSubscription subscription_;
};

class SideSearchIPHAndTutorialBrowserTest
    : public InteractiveBrowserTestT<SideSearchFeatureEngagementTest> {
 public:
  SideSearchIPHAndTutorialBrowserTest() {
    feature_list_.InitAndEnableFeaturesWithParameters({
        {feature_engagement::kIPHSideSearchFeature,
         GetFeatureEngagementParams()},
        {features::kSideSearch, {}},
    });
  }

  // TODO(crbug.com/1491942): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTestT::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTestT::SetUp();
  }

  bool CurrentBubbleAnchoredToCorrectElement(
      const ui::ElementIdentifier& anchored_element_id) {
    ui::TrackedElement* t =
        ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting);
    auto* const view_element = t->AsA<views::TrackedElementViews>()->view();
    return views::AsViewClass<views::BubbleDialogDelegateView>(view_element)
               ->GetAnchorView()
               ->GetProperty(views::kElementIdentifierKey) ==
           anchored_element_id;
  }

  auto StartNavigationFromSidePanel(Browser* browser, const GURL& url) {
    auto* side_contents = GetActiveSidePanelWebContents(browser);
    side_contents->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(url));
  }

  auto CheckIPHTriggeredCorrectly(const ui::ElementIdentifier& primary_tab_id) {
    const auto srp_url = GetMatchingSearchUrl();
    const auto non_srp_url_1 = GetNonMatchingUrl();
    return Steps(
        InstrumentTab(primary_tab_id),
        // Navigate to a SRP URL and then once to a non-SRP URL.
        NavigateWebContents(primary_tab_id, srp_url),
        NavigateWebContents(primary_tab_id, non_srp_url_1),
        // Ensure that the side search button is present, but the side search
        // panel isn't open.
        WaitForShow(kSideSearchButtonElementId),
        EnsureNotPresent(kSidePanelElementId),

        // IPH bubble appears.
        // Verify it's created with correct body text and anchored to side
        // search page action icon button.
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                          &views::Label::GetText,
                          l10n_util::GetStringUTF16(IDS_SIDE_SEARCH_PROMO)),
        Check(base::BindLambdaForTesting([this]() {
          return CurrentBubbleAnchoredToCorrectElement(
              kSideSearchButtonElementId);
        })));
  }

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SideSearchIPHAndTutorialBrowserTest,
                       IPHDismissedCorrectly) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);

  RunTestSequence(
      CheckIPHTriggeredCorrectly(kPrimaryTabId),

      // Press "Remind me later".
      PressButton(
          user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(SideSearchIPHAndTutorialBrowserTest,
                       IPHTriggersTutorialCorrectly) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);
  const auto non_srp_url = GetNonMatchingUrl();

  RunTestSequence(
      CheckIPHTriggeredCorrectly(kPrimaryTabId),

      // Press "Show me how".
      PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),

      // 1st tutorial bubble appears.
      // Verify it's created with correct body text and anchored to side search
      // page action icon button.
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetStringUTF16(IDS_SIDE_SEARCH_TUTORIAL_OPEN_SIDE_PANEL)),
      Check(base::BindLambdaForTesting([this]() {
        return CurrentBubbleAnchoredToCorrectElement(
            kSideSearchButtonElementId);
      })),

      // Click on side search page action icon to pop up side panel.
      PressButton(kSideSearchButtonElementId),

      // 2nd tutorial bubble appears.
      // Verify it's created with correct body text and anchored to side panel
      // web view.
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(
                            IDS_SIDE_SEARCH_TUTORIAL_OPEN_A_LINK_TO_TAB)),
      Check(base::BindLambdaForTesting([this]() {
        return CurrentBubbleAnchoredToCorrectElement(
            kSideSearchWebViewElementId);
      })),

      // Simulate a click on a random result in side panel.
      Do(base::BindLambdaForTesting([&, this]() {
        StartNavigationFromSidePanel(browser(), non_srp_url);
      })),
      WaitForWebContentsNavigation(kPrimaryTabId),

      // 3rd tutorial bubble appears.
      // Verify it's created with correct body text and anchored to side panel
      // close button.
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetStringUTF16(IDS_SIDE_SEARCH_TUTORIAL_CLOSE_SIDE_PANEL)),
      Check(base::BindLambdaForTesting([this]() {
        return CurrentBubbleAnchoredToCorrectElement(
            kSidePanelCloseButtonElementId);
      })),

      // Press side panel close button.
      PressButton(kSidePanelCloseButtonElementId), FlushEvents(),

      // Final tutorial button appears.
      // Verify it's created with correct body text and anchored to side search
      // page action icon button.
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_SIDE_SEARCH_PROMO)),
      Check(base::BindLambdaForTesting([this]() {
        return CurrentBubbleAnchoredToCorrectElement(
            kSideSearchButtonElementId);
      })),

      // Verify the final tutorial bubble has a Restart button.
      CheckViewProperty(
          user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting,
          &views::MdTextButton::GetText,
          l10n_util::GetStringUTF16(IDS_TUTORIAL_RESTART_TUTORIAL)),

      // Pressing Restart button restarts the tutorial flow.
      // 1st tutorial bubble appears.
      // Verify it's created with correct body text and anchored to side search
      // page action icon button.
      PressButton(
          user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckViewProperty(
          user_education::HelpBubbleView::kBodyTextIdForTesting,
          &views::Label::GetText,
          l10n_util::GetStringUTF16(IDS_SIDE_SEARCH_TUTORIAL_OPEN_SIDE_PANEL)),
      Check(base::BindLambdaForTesting([this]() {
        return CurrentBubbleAnchoredToCorrectElement(
            kSideSearchButtonElementId);
      })));
}

class SideSearchAutoTriggeringBrowserTest
    : public SideSearchFeatureEngagementTest,
      public InteractiveBrowserTestApi {
 public:
  SideSearchAutoTriggeringBrowserTest() {
    constexpr char kParam[] = "SideSearchAutoTriggeringReturnCount";
    constexpr char kTriggerCount[] = "2";
    base::FieldTrialParams params = {{kParam, kTriggerCount}};

    feature_list_.InitAndEnableFeaturesWithParameters({
        {features::kSideSearch, {}},
        {features::kSideSearchAutoTriggering, params},
        {feature_engagement::kIPHSideSearchAutoTriggeringFeature,
         GetFeatureEngagementParams()},
    });
  }

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    SideSearchFeatureEngagementTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SideSearchFeatureEngagementTest::SetUpOnMainThread();
    private_test_impl().DoTestSetUp();
    SetContextWidget(
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget());
  }

  void TearDownOnMainThread() override {
    private_test_impl().DoTestTearDown();
    SideSearchFeatureEngagementTest::TearDownOnMainThread();
  }

  auto CheckHistogramCounts(int expected_count) {
    return Do(base::BindOnce(
        [](base::HistogramTester* tester, int count) {
          tester->ExpectUniqueSample(
              "SideSearch.RedirectionToTabCountPerJourney2", 1, count);
          tester->ExpectUniqueSample(
              "SideSearch.AutoTrigger.RedirectionToTabCountPerJourney", 1,
              count);
          tester->ExpectUniqueSample(
              "SideSearch.NavigationCommittedWithinSideSearchCountPerJourney2",
              1, count);
          tester->ExpectUniqueSample(
              "SideSearch.AutoTrigger."
              "NavigationCommittedWithinSideSearchCountPerJourney",
              1, count);
        },
        base::Unretained(&histogram_tester()), expected_count));
  }

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SideSearchAutoTriggeringBrowserTest,
                       SidePanelAutoTriggersAfterReturningToAPreviousSRP) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);

  const auto srp_url = GetMatchingSearchUrl();
  const auto non_srp_url_1 = GetNonMatchingUrl();
  const auto non_srp_url_2 = GetNonMatchingUrl();
  const auto non_srp_url_3 = GetNonMatchingUrl();

  auto* const coordinator =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());

  RunTestSequence(
      InstrumentTab(kPrimaryTabId),
      // Navigate to a SRP URL and then once to a non-SRP URL.
      NavigateWebContents(kPrimaryTabId, srp_url),
      NavigateWebContents(kPrimaryTabId, non_srp_url_1),
      // Ensure that the side search button is present, but the side search
      // panel isn't open.
      WaitForShow(kSideSearchButtonElementId),
      EnsureNotPresent(kSidePanelElementId),
      // Going back will increase the returned-to-SRP count to 1.
      PressButton(kToolbarBackButtonElementId),
      WaitForWebContentsNavigation(kPrimaryTabId),
      // The side panel should not automatically open when navigating to a
      // non-SRP URL.
      NavigateWebContents(kPrimaryTabId, non_srp_url_2),
      WaitForShow(kSideSearchButtonElementId),
      EnsureNotPresent(kSidePanelElementId),
      // Going back will increase the returned-to-SRP count to 2.
      PressButton(kToolbarBackButtonElementId),
      WaitForWebContentsNavigation(kPrimaryTabId),
      // Navigating to a non-SRP URL should now automatically trigger the side
      // search side panel.
      NavigateWebContents(kPrimaryTabId, non_srp_url_3),
      WaitForHide(kSideSearchButtonElementId), WaitForShow(kSidePanelElementId),
      // Verify that the side search panel is selected.
      CheckResult(base::BindLambdaForTesting([coordinator]() {
                    return coordinator->GetCurrentSidePanelEntryForTesting()
                        ->key()
                        .id();
                  }),
                  SidePanelEntry::Id::kSideSearch),
      // When the side search WebContents is displayed, instrument it.
      InstrumentNonTabWebView(kSidePanelWebContentsId,
                              kSideSearchWebViewElementId),
      // Navigate matching and non-matching URLs in the side contents and verify
      // that metrics are emitted correctly. A matching URL navigates in side
      // panel.
      AfterShow(kSidePanelWebContentsId,
                base::BindLambdaForTesting([this](ui::TrackedElement* el) {
                  // This isn't guaranteed to load in the side panel, so we
                  // don't use the NavigateWebContents() verb.
                  AsInstrumentedWebContents(el)->LoadPage(
                      GetMatchingSearchUrl());
                })),
      WaitForWebContentsNavigation(kSidePanelWebContentsId),
      // A non-matching URL navigates in the current browser tab.
      WithElement(kSidePanelWebContentsId,
                  base::BindLambdaForTesting([this](ui::TrackedElement* el) {
                    AsInstrumentedWebContents(el)->LoadPage(
                        GetNonMatchingUrl());
                  }))
          .SetMustRemainVisible(false),
      WaitForWebContentsNavigation(kPrimaryTabId),
      // Metrics should not be emitted until the side panel is closed (i.e. the
      // Side Search contents is destroyed).
      CheckHistogramCounts(0),
      // Side panel should still be visible.
      WithElement(kSidePanelElementId, base::DoNothing()),
      // Close side panel.
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId), WaitForHide(kSidePanelWebContentsId),
      // Process any pending cleanup and check the histograms, which should now
      // be updated.
      FlushEvents(), CheckHistogramCounts(1));
}

class SideSearchPageActionLabelTriggerBrowserTest
    : public SideSearchFeatureEngagementTest {
 public:
  SideSearchPageActionLabelTriggerBrowserTest() {
    feature_list_.InitAndEnableFeaturesWithParameters({
        {features::kSideSearch, {}},
        {feature_engagement::kIPHSideSearchPageActionLabelFeature,
         GetFeatureEngagementParams()},
    });
  }

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SideSearchPageActionLabelTriggerBrowserTest,
                       SideSearchPageActionLabelAnimationTriggersCorrectly) {
  auto* button_view = GetSideSearchButtonFor(browser());
  ASSERT_NE(nullptr, button_view);
  auto* icon_view = views::AsViewClass<SideSearchIconView>(button_view);

  // Get the browser into a state where the icon view is visible.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  ASSERT_FALSE(icon_view->GetVisible());
  NavigateActiveTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());

  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(icon_view->IsLabelVisibleForTesting());

  // Show the icon's label and toggle the side panel. It should correctly log
  // being shown while the label was visible.
  NotifyButtonClick(browser());
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_FALSE(icon_view->IsLabelVisibleForTesting());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester().ExpectBucketCount(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled",
      SideSearchPageActionLabelVisibility::kVisible, 1);
  histogram_tester().ExpectBucketCount(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled",
      SideSearchPageActionLabelVisibility::kNotVisible, 0);

  // Close the side panel.
  NotifyCloseButtonClick(browser());
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_FALSE(GetSidePanelFor(browser())->GetVisible());

  // The Label should no longer be visible.
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_FALSE(icon_view->IsLabelVisibleForTesting());

  // Toggle the side panel again, it should correctly log
  // being shown while the label was hidden.
  NotifyButtonClick(browser());
  EXPECT_FALSE(icon_view->GetVisible());
  EXPECT_FALSE(icon_view->IsLabelVisibleForTesting());
  EXPECT_TRUE(GetSidePanelFor(browser())->GetVisible());
  histogram_tester().ExpectBucketCount(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled",
      SideSearchPageActionLabelVisibility::kVisible, 1);
  histogram_tester().ExpectBucketCount(
      "SideSearch.PageActionIcon.LabelVisibleWhenToggled",
      SideSearchPageActionLabelVisibility::kNotVisible, 1);
}
