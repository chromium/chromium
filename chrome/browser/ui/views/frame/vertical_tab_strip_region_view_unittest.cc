// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/separator.h"

class VerticalTabStripRegionViewTest : public testing::Test {
 public:
  VerticalTabStripRegionViewTest() = default;
  ~VerticalTabStripRegionViewTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kVerticalTabsEnabled, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    controller_ =
        std::make_unique<tabs::VerticalTabStripStateController>(&pref_service_);
    region_view_ =
        std::make_unique<VerticalTabStripRegionView>(controller_.get());
  }

  void TearDown() override {
    controller_.reset();
    testing::Test::TearDown();
  }

  VerticalTabStripRegionView* region_view() { return region_view_.get(); }
  tabs::VerticalTabStripStateController* controller() {
    return controller_.get();
  }

 private:
  std::unique_ptr<tabs::VerticalTabStripStateController> controller_;
  std::unique_ptr<VerticalTabStripRegionView> region_view_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(VerticalTabStripRegionViewTest,
       SeparatorVisibilityChangesWithCollapsedState) {
  controller()->SetCollapsed(true);
  EXPECT_TRUE(controller()->IsCollapsed());
  EXPECT_TRUE(region_view()->tabs_separator_for_testing()->GetVisible());

  controller()->SetCollapsed(false);
  EXPECT_FALSE(controller()->IsCollapsed());
  EXPECT_FALSE(region_view()->tabs_separator_for_testing()->GetVisible());
}

TEST_F(VerticalTabStripRegionViewTest, ResizeAreaBounds) {
  region_view()->SetBounds(0, 0, 200, 600);
  // Verify resize area is on the right side of the VerticalTabStripRegionView.
  EXPECT_EQ(region_view()->bounds().right(),
            region_view()->resize_area_for_testing()->bounds().right());
  // Verify resize area fills VerticalTabStripRegionView height.
  EXPECT_EQ(region_view()->bounds().height(),
            region_view()->resize_area_for_testing()->bounds().height());
  EXPECT_EQ(0, region_view()->resize_area_for_testing()->bounds().y());
  // Verify resize area width.
  EXPECT_EQ(VerticalTabStripRegionView::kResizeAreaWidth,
            region_view()->resize_area_for_testing()->bounds().width());
}

// Verify that the pinned tabs container will never be larger than the unpinned
// tabs area.
TEST_F(VerticalTabStripRegionViewTest, PinnedTabsAreaSmallerThanUnpinned) {
  region_view()->SetBounds(0, 0, 200, 600);
  region_view()->pinned_tabs_container_for_testing()->SetPreferredSize(
      gfx::Size(100, 500));
  region_view()->unpinned_tabs_container_for_testing()->SetPreferredSize(
      gfx::Size(100, 400));
  EXPECT_LE(
      region_view()->pinned_tabs_container_for_testing()->bounds().height(),
      region_view()->unpinned_tabs_container_for_testing()->bounds().height());

  region_view()->unpinned_tabs_container_for_testing()->SetPreferredSize(
      gfx::Size(100, 50));
  EXPECT_LE(
      region_view()->pinned_tabs_container_for_testing()->bounds().height(),
      region_view()->unpinned_tabs_container_for_testing()->bounds().height());
}
