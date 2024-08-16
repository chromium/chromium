// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/memory_saver_chip_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_bubble_view.h"
#include "chrome/browser/ui/views/performance_controls/memory_saver_resource_view.h"
#include "chrome/browser/ui/views/performance_controls/test_support/memory_saver_unit_test_mixin.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"

namespace {
constexpr int64_t kMemorySavingsKilobytes = 100 * 1024;
constexpr int64_t kHighMemorySavingsKilobytes = 300 * 1024;
constexpr int64_t kVeryHighMemorySavingsKilobytes = 3 * 1024 * 1024;
}  // namespace

class MemorySaverChipViewTest
    : public MemorySaverUnitTestMixin<TestWithBrowserView> {
 public:
  MemorySaverChipViewTest()
      : MemorySaverUnitTestMixin(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    MemorySaverUnitTestMixin::SetUp();

    AddNewTab(kMemorySavingsKilobytes,
              ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

    SetMemorySaverModeEnabled(true);
  }

  void SetChipExpandedCount(int count) {
    browser_view()->browser()->profile()->GetPrefs()->SetInteger(
        prefs::kMemorySaverChipExpandedCount, count);
  }

  void SetChipExpandedTimeToNow() {
    browser_view()->browser()->profile()->GetPrefs()->SetTime(
        prefs::kLastMemorySaverChipExpandedTimestamp, base::Time::Now());
  }

  base::HistogramTester histogram_tester_;
};

// When the previous page has a tab discard state of true, when the icon is
// updated it should be visible.
TEST_F(MemorySaverChipViewTest, ShouldShowChipForProactivelyDiscardedPage) {
  SetTabDiscardState(0, true);
  EXPECT_TRUE(GetPageActionIconView()->GetVisible());
}

TEST_F(MemorySaverChipViewTest,
       ShouldNotShowChipWhenNonProactivelyDiscardPage) {
  // Add a new tab that was discarded through extensions
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::EXTERNAL);
  SetTabDiscardState(0, true);
  EXPECT_FALSE(GetPageActionIconView()->GetVisible());

  // Add a new tab that was urgently discarded
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::URGENT);
  SetTabDiscardState(0, true);
  EXPECT_FALSE(GetPageActionIconView()->GetVisible());
}

// If a discard is triggered when the user doesn't have memory saver mode
// enabled, we don't show the chip.
TEST_F(MemorySaverChipViewTest, ShouldNotShowWhenPrefIsFalse) {
  SetMemorySaverModeEnabled(false);
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();

  EXPECT_FALSE(view->GetVisible());
}

// When the collapsed chip is shown, UMA metrics should be logged.
TEST_F(MemorySaverChipViewTest, ShouldLogMetricsForCollapsedChip) {
  SetChipExpandedCount(MemorySaverChipView::kChipAnimationCount);
  SetTabDiscardState(0, true);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.MemorySaver.ChipState",
      MemorySaverChipState::kCollapsed, 1);
}

// When the educational expanded chip is shown, UMA metrics should be logged.
TEST_F(MemorySaverChipViewTest, ShouldLogMetricsForInformationalExpandedChip) {
  SetTabDiscardState(0, true);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.MemorySaver.ChipState",
      MemorySaverChipState::kExpandedEducation, 1);
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(MemorySaverChipViewTest, ShouldNotShowForRegularPage) {
  SetTabDiscardState(0, false);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_FALSE(view->GetVisible());
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(MemorySaverChipViewTest, ShouldHideLabelAfterMultipleDiscards) {
  // Open the tab the max number of times for the label to be visible
  for (int i = 0; i < MemorySaverChipTabHelper::kChipAnimationCount; i++) {
    SetTabDiscardState(0, true);
    EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());
    SetTabDiscardState(0, false);
  }

  // The label should be hidden on subsequent discards
  SetTabDiscardState(0, true);
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());
}

TEST_F(MemorySaverChipViewTest, ShouldCollapseChipAfterNavigatingTabs) {
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* web_contents_0 =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(2, tab_strip_model->GetTabCount());

  SetTabDiscardState(0, true);
  EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectNextTab();
  web_contents_0->WasHidden();
  web_contents_1->WasShown();
  PageActionIconView* view = GetPageActionIconView();
  EXPECT_FALSE(view->GetVisible());

  SetTabDiscardState(1, true);
  EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectPreviousTab();
  web_contents_0->WasShown();
  web_contents_1->WasHidden();
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectNextTab();
  web_contents_0->WasHidden();
  web_contents_1->WasShown();
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());
}

// When the savings are above the threshold then the chip is
// eligible to expand.
TEST_F(MemorySaverChipViewTest, ShouldExpandChipWhenConditionsAreMet) {
  SetChipExpandedCount(MemorySaverChipView::kChipAnimationCount);
  AddNewTab(kHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->ShouldShowLabel());
}

// When the savings are below the threshold then the chip won't
// expand.
TEST_F(MemorySaverChipViewTest, ShouldNotExpandForSavingsBelowThreshold) {
  SetChipExpandedCount(MemorySaverChipTabHelper::kChipAnimationCount);

  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
}

// When the savings chip has been expanded recently then it does not show in
// the expanded mode.
TEST_F(MemorySaverChipViewTest, ShouldNotExpandWhenChipHasExpandedRecently) {
  SetChipExpandedCount(MemorySaverChipTabHelper::kChipAnimationCount);
  SetChipExpandedTimeToNow();
  AddNewTab(kHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
}

// When the tab has been expanded recently then the chip does not show in the
// expanded mode.
TEST_F(MemorySaverChipViewTest, ShouldNotExpandWhenTabWasDiscardedRecently) {
  SetChipExpandedCount(MemorySaverChipTabHelper::kChipAnimationCount);
  AddNewTab(kHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
}

// When the celebratory expanded chip is shown, UMA metrics should be logged.
TEST_F(MemorySaverChipViewTest, ShouldLogMetricsForCelebratoryExpandedChip) {
  SetChipExpandedCount(MemorySaverChipTabHelper::kChipAnimationCount);
  AddNewTab(kHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.MemorySaver.ChipState",
      MemorySaverChipState::kExpandedWithSavings, 1);
}

// When a tab is proactively discarded with >2Gb memory saving, we should show
// the expanded chip with savings, and not crash.
TEST_F(MemorySaverChipViewTest, MoreThan2GbMemorySavings) {
  // Skip the "education" expansion.
  SetChipExpandedCount(MemorySaverChipTabHelper::kChipAnimationCount);

  // Add a new tab with a >2GB memory saving.
  AddNewTab(kVeryHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  // Fast-forward time, to exceed the time threshold for the chip to be shown.
  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  // Ensure that the expanded-with-savings chip was shown.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  MemorySaverChipTabHelper* const tab_helper =
      MemorySaverChipTabHelper::FromWebContents(web_contents);
  EXPECT_EQ(tab_helper->chip_state(),
            memory_saver::ChipState::EXPANDED_WITH_SAVINGS);
  EXPECT_TRUE(GetPageActionIconView()->GetVisible());
}
