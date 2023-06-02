// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/high_efficiency_chip_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/high_efficiency_chip_tab_helper.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_bubble_view.h"
#include "chrome/browser/ui/views/performance_controls/high_efficiency_resource_view.h"
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

constexpr int kMemorySavingsKilobytes = 100 * 1024;
constexpr int kHighMemorySavingsKilobytes = 300 * 1024;

class DiscardMockNavigationHandle : public content::MockNavigationHandle {
 public:
  void SetWasDiscarded(bool was_discarded) { was_discarded_ = was_discarded; }
  bool ExistingDocumentWasDiscarded() const override { return was_discarded_; }
  void SetWebContents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }
  content::WebContents* GetWebContents() override { return web_contents_; }

 private:
  bool was_discarded_ = false;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

class HighEfficiencyChipViewTest : public TestWithBrowserView {
 public:
 protected:
  HighEfficiencyChipViewTest()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    feature_list_.InitAndDisableFeature(
        performance_manager::features::kMemorySavingsReportingImprovements);
    TestWithBrowserView::SetUp();

    AddNewTab(kMemorySavingsKilobytes,
              ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

    SetHighEfficiencyModeEnabled(true);
  }

  // Creates a new tab at index 0 that would report the given memory savings and
  // discard reason if the tab was discarded
  void AddNewTab(int memory_savings,
                 mojom::LifecycleUnitDiscardReason discard_reason) {
    AddTab(browser(), GURL("http://foo"));
    content::WebContents* const contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    HighEfficiencyChipTabHelper::CreateForWebContents(contents);
    performance_manager::user_tuning::UserPerformanceTuningManager::
        PreDiscardResourceUsage::CreateForWebContents(contents, memory_savings,
                                                      discard_reason);
  }

  void SetTabDiscardState(int tab_index, bool is_discarded) {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(tab_index);
    std::unique_ptr<DiscardMockNavigationHandle> navigation_handle =
        std::make_unique<DiscardMockNavigationHandle>();
    navigation_handle.get()->SetWasDiscarded(is_discarded);
    navigation_handle.get()->SetWebContents(web_contents);
    HighEfficiencyChipTabHelper::FromWebContents(web_contents)
        ->DidStartNavigation(navigation_handle.get());

    browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->UpdateAll();
  }

  void SetHighEfficiencyModeEnabled(bool enabled) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetHighEfficiencyModeEnabled(enabled);
  }

  void SetChipExpandedCount(int count) {
    browser_view()->browser()->profile()->GetPrefs()->SetInteger(
        prefs::kHighEfficiencyChipExpandedCount, count);
  }

  void SetChipExpandedTimeToNow() {
    browser_view()->browser()->profile()->GetPrefs()->SetTime(
        prefs::kLastHighEfficiencyChipExpandedTimestamp, base::Time::Now());
  }

  PageActionIconView* GetPageActionIconView() {
    return browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kHighEfficiency);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// When the previous page has a tab discard state of true, when the icon is
// updated it should be visible.
TEST_F(HighEfficiencyChipViewTest, ShouldShowChipForProactivelyDiscardedPage) {
  SetTabDiscardState(0, true);
  EXPECT_TRUE(GetPageActionIconView()->GetVisible());
}

TEST_F(HighEfficiencyChipViewTest,
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

// If a discard is triggered when the user doesn't have high efficiency mode
// enabled, we don't show the chip.
TEST_F(HighEfficiencyChipViewTest, ShouldNotShowWhenPrefIsFalse) {
  SetHighEfficiencyModeEnabled(false);
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();

  EXPECT_FALSE(view->GetVisible());
}

// When the collapsed chip is shown, UMA metrics should be logged.
TEST_F(HighEfficiencyChipViewTest, ShouldLogMetricsForCollapsedChip) {
  SetChipExpandedCount(HighEfficiencyChipView::kChipAnimationCount);
  SetTabDiscardState(0, true);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.HighEfficiency.ChipState",
      HighEfficiencyChipState::kCollapsed, 1);
}

// When the educational expanded chip is shown, UMA metrics should be logged.
TEST_F(HighEfficiencyChipViewTest,
       ShouldLogMetricsForInformationalExpandedChip) {
  SetTabDiscardState(0, true);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.HighEfficiency.ChipState",
      HighEfficiencyChipState::kExpandedEducation, 1);
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(HighEfficiencyChipViewTest, ShouldNotShowForRegularPage) {
  SetTabDiscardState(0, false);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_FALSE(view->GetVisible());
}

// When kMemorySavingsReportingImprovements is disabled, the chip should not
// expand.
TEST_F(HighEfficiencyChipViewTest, ShouldNotExpandWhenFeatureIsDisabled) {
  SetChipExpandedCount(HighEfficiencyChipView::kChipAnimationCount);
  AddNewTab(kHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
}

// When the previous page was not previously discarded, the icon should not be
// visible.
TEST_F(HighEfficiencyChipViewTest, ShouldHideLabelAfterMultipleDiscards) {
  // Open the tab the max number of times for the label to be visible
  for (int i = 0; i < HighEfficiencyChipView::kChipAnimationCount; i++) {
    SetTabDiscardState(0, true);
    EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());
    SetTabDiscardState(0, false);
  }

  // The label should be hidden on subsequent discards
  SetTabDiscardState(0, true);
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());
}

TEST_F(HighEfficiencyChipViewTest, ShouldCollapseChipAfterNavigatingTabs) {
  AddNewTab(kMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(2, tab_strip_model->GetTabCount());

  SetTabDiscardState(0, true);
  EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectNextTab();
  web_contents->WasHidden();
  PageActionIconView* view = GetPageActionIconView();
  EXPECT_FALSE(view->GetVisible());

  SetTabDiscardState(1, true);
  EXPECT_TRUE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectPreviousTab();
  web_contents->WasShown();
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());

  tab_strip_model->SelectNextTab();
  web_contents->WasHidden();
  EXPECT_FALSE(GetPageActionIconView()->ShouldShowLabel());
}

class HighEfficiencyChipViewMemorySavingsImprovementsTest
    : public HighEfficiencyChipViewTest {
 public:
  HighEfficiencyChipViewMemorySavingsImprovementsTest() = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        performance_manager::features::kMemorySavingsReportingImprovements);
    TestWithBrowserView::SetUp();

    AddNewTab(kMemorySavingsKilobytes,
              ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

    SetHighEfficiencyModeEnabled(true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// When the savings are above the FeatureParam threshold then the chip is
// eligible to expand.
TEST_F(HighEfficiencyChipViewMemorySavingsImprovementsTest,
       ShouldExpandChipWhenConditionsAreMet) {
  SetChipExpandedCount(HighEfficiencyChipView::kChipAnimationCount);
  AddNewTab(kHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_TRUE(view->ShouldShowLabel());
}

// When the savings are below the FeatureParam threshold then the chip won't
// expand.
TEST_F(HighEfficiencyChipViewMemorySavingsImprovementsTest,
       ShouldNotExpandForSavingsBelowThreshold) {
  SetChipExpandedCount(HighEfficiencyChipView::kChipAnimationCount);

  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
}

// When the savings chip has been expanded recently then it does not show in
// the expanded mode.
TEST_F(HighEfficiencyChipViewMemorySavingsImprovementsTest,
       ShouldNotExpandWhenChipHasExpandedRecently) {
  SetChipExpandedCount(HighEfficiencyChipView::kChipAnimationCount);
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
TEST_F(HighEfficiencyChipViewMemorySavingsImprovementsTest,
       ShouldNotExpandWhenTabWasDiscardedRecently) {
  SetChipExpandedCount(HighEfficiencyChipView::kChipAnimationCount);
  AddNewTab(kHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  SetTabDiscardState(0, true);

  PageActionIconView* view = GetPageActionIconView();
  EXPECT_TRUE(view->GetVisible());
  EXPECT_FALSE(view->ShouldShowLabel());
}

// When the celebratory expanded chip is shown, UMA metrics should be logged.
TEST_F(HighEfficiencyChipViewMemorySavingsImprovementsTest,
       ShouldLogMetricsForCelebratoryExpandedChip) {
  SetChipExpandedCount(HighEfficiencyChipView::kChipAnimationCount);
  AddNewTab(kHighMemorySavingsKilobytes,
            ::mojom::LifecycleUnitDiscardReason::PROACTIVE);

  task_environment()->AdvanceClock(base::Hours(8));
  SetTabDiscardState(0, true);

  histogram_tester_.ExpectUniqueSample(
      "PerformanceControls.HighEfficiency.ChipState",
      HighEfficiencyChipState::kExpandedWithSavings, 1);
}
