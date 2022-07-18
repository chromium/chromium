// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/unified_side_search_controller.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_search/side_search_browsertest.h"
#include "content/public/test/browser_test.h"
#include "ui/views/interaction/element_tracker_views.h"

// Fixture for testing side panel v2 only. Only instantiate tests for DSE
// configuration.
class SideSearchV2Test : public SideSearchBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kSideSearch, features::kSideSearchDSESupport,
         features::kUnifiedSidePanel},
        {});
    SideSearchBrowserTest::SetUp();
  }

  SidePanel* GetSidePanelFor(Browser* browser) override {
    return BrowserViewFor(browser)->right_aligned_side_panel();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SideSearchV2Test, SwitchSidePanelInSingleTab) {
  auto* browser_view = BrowserViewFor(browser());
  auto* coordinator = browser_view->side_panel_coordinator();
  coordinator->SetNoDelaysForTesting();

  // Tab 0 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Switch to reading list side panel.
  coordinator->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Switch back to side search side panel.
  coordinator->Show(SidePanelEntry::Id::kSideSearch);
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1341272): Test is flaky on Mac.
#define MAYBE_SwitchTabsWithGlobalSidePanel \
  DISABLED_SwitchTabsWithGlobalSidePanel
#else
#define MAYBE_SwitchTabsWithGlobalSidePanel SwitchTabsWithGlobalSidePanel
#endif
IN_PROC_BROWSER_TEST_F(SideSearchV2Test, MAYBE_SwitchTabsWithGlobalSidePanel) {
  auto* browser_view = BrowserViewFor(browser());
  auto* coordinator = browser_view->side_panel_coordinator();
  coordinator->SetNoDelaysForTesting();

  // Tab 0 without side search available and open with reading list.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  coordinator->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Tab 1 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Tab 2 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Tab 3 with side search available but not open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Switch to tab 0, side panel is open with reading list.
  ActivateTabAt(browser(), 0);
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Switch to tab 1, side panel is open with side search.
  ActivateTabAt(browser(), 1);
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Switch to tab 2, side panel is open with side search.
  ActivateTabAt(browser(), 2);
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Switch to tab 3, side panel is open with reading list.
  ActivateTabAt(browser(), 3);
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/1340387): Test is flaky on Mac.
#define MAYBE_SwitchTabsWithoutGlobalSidePanel \
  DISABLED_SwitchTabsWithoutGlobalSidePanel
#else
#define MAYBE_SwitchTabsWithoutGlobalSidePanel SwitchTabsWithoutGlobalSidePanel
#endif
IN_PROC_BROWSER_TEST_F(SideSearchV2Test,
                       MAYBE_SwitchTabsWithoutGlobalSidePanel) {
  auto* browser_view = BrowserViewFor(browser());
  auto* coordinator = browser_view->side_panel_coordinator();

  // Tab 0 without side search available.
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_FALSE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_EQ(nullptr, coordinator->GetCurrentSidePanelEntryForTesting());

  // Tab 1 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Tab 2 with side search available and open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  NotifyButtonClick(browser());
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Tab 3 with side search available but not open.
  AppendTab(browser(), GetMatchingSearchUrl());
  NavigateActiveTab(browser(), GetNonMatchingUrl());
  EXPECT_TRUE(GetSidePanelButtonFor(browser())->GetVisible());
  EXPECT_EQ(nullptr, coordinator->GetCurrentSidePanelEntryForTesting());

  // Switch to tab 0, side panel is closed.
  ActivateTabAt(browser(), 0);
  EXPECT_EQ(nullptr, coordinator->GetCurrentSidePanelEntryForTesting());

  // Switch to tab 1, side panel is open with side search.
  ActivateTabAt(browser(), 1);
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Switch to tab 2, side panel is open with side search.
  ActivateTabAt(browser(), 2);
  EXPECT_EQ(SidePanelEntry::Id::kSideSearch,
            coordinator->GetCurrentSidePanelEntryForTesting()->id());

  // Switch to tab 3, side panel is closed.
  ActivateTabAt(browser(), 3);
  EXPECT_EQ(nullptr, coordinator->GetCurrentSidePanelEntryForTesting());
}
