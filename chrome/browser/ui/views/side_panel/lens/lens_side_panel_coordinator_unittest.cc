// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "components/lens/lens_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class LensSidePanelCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeaturesAndParameters(
        {{lens::features::kLensStandalone,
          {{lens::features::kEnableSidePanelForLens.name, "true"}}},
         {features::kUnifiedSidePanel, {{}}}},
        {});
    TestWithBrowserView::SetUp();

    GetSidePanelCoordinator()->SetNoDelaysForTesting();
    auto* browser = browser_view()->browser();
    auto* global_registry =
        GetSidePanelCoordinator()->GetGlobalSidePanelRegistry();
    SidePanelUtil::PopulateGlobalEntries(browser, global_registry);

    EXPECT_EQ(global_registry->entries().size(), 2u);

    // Create the lens coordinator in Browser.
    lens_side_panel_coordinator_ =
        LensSidePanelCoordinator::GetOrCreateForBrowser(browser);
    // Create an active web contents.
    AddTab(browser, GURL("about:blank"));
  }

  SidePanelCoordinator* GetSidePanelCoordinator() {
    return browser_view()->side_panel_coordinator();
  }

  SidePanel* GetRightAlignedSidePanel() {
    return browser_view()->right_aligned_side_panel();
  }

 protected:
  raw_ptr<LensSidePanelCoordinator> lens_side_panel_coordinator_;
};

TEST_F(LensSidePanelCoordinatorTest,
       OpenWithUrlShowsUnifiedSidePanelWithLensSelected) {
  base::UserActionTester user_action_tester;

  lens_side_panel_coordinator_->RegisterEntryAndShow(
      content::OpenURLParams(GURL("http://foo.com"), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  EXPECT_TRUE(GetRightAlignedSidePanel()->GetVisible());
  EXPECT_EQ(
      GetSidePanelCoordinator()->GetCurrentSidePanelEntryForTesting()->id(),
      SidePanelEntry::Id::kLens);
}
}  // namespace
