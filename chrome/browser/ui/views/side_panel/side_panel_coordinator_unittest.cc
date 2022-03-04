// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

class SidePanelCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        {features::kSidePanel, features::kUnifiedSidePanel}, {});
    TestWithBrowserView::SetUp();

    // Create an active web contents.
    AddTab(browser_view()->browser(), GURL("about:blank"));
    coordinator_ = browser_view()->side_panel_coordinator();
  }

  absl::optional<SidePanelEntry::Id> GetLastActiveEntry() {
    return browser_view()->global_side_panel_registry()->last_active_entry();
  }

 protected:
  raw_ptr<SidePanelCoordinator> coordinator_;
};

TEST_F(SidePanelCoordinatorTest, ToggleSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());

  coordinator_->Toggle();
  EXPECT_FALSE(browser_view()->right_aligned_side_panel()->GetVisible());
}

TEST_F(SidePanelCoordinatorTest, SwitchBetweenSidePanelEntries) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());

  coordinator_->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntry().has_value());
  EXPECT_EQ(GetLastActiveEntry().value(), SidePanelEntry::Id::kBookmarks);

  coordinator_->Show(SidePanelEntry::Id::kReadingList);
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());
  EXPECT_TRUE(GetLastActiveEntry().has_value());
  EXPECT_EQ(GetLastActiveEntry().value(), SidePanelEntry::Id::kReadingList);
}
