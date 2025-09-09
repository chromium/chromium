// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/tab_creation_metrics_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/test/views_test_utils.h"

namespace {
ui::MouseEvent dummy_event_ = ui::MouseEvent(ui::EventType::kMousePressed,
                                             gfx::PointF(),
                                             gfx::PointF(),
                                             base::TimeTicks::Now(),
                                             0,
                                             0);

constexpr char kHistogramName[] = "Tab.GroupingTransition";
}

class BrowserTabStripControllerTestBase : public InProcessBrowserTest {
 public:
  BrowserTabStripControllerTestBase()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {}

  void SetUpOnMainThread() override {
    tabs::TabCreationMetricsController::SetTaskRunnerForTesting(task_runner_);
  }

  void TearDownOnMainThread() override {
    tabs::TabCreationMetricsController::SetTaskRunnerForTesting(nullptr);
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }
  TabStrip* tabstrip() {
    return views::AsViewClass<TabStripRegionView>(
               browser()->GetBrowserView().tab_strip_view())
        ->tab_strip();
  }
  TabStripController* controller() { return tabstrip()->controller(); }

 protected:
  void FastForwardPastDelay() {
    task_runner_->FastForwardBy(tabs::TabCreationMetricsController::kDelay +
                                base::Seconds(1));
  }

  void WaitForTabSyncServiceInitialization() {
    tab_groups::TabGroupSyncService* tgss_service =
        static_cast<tab_groups::TabGroupSyncService*>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(
                browser()->profile()));
    // Make the observer
    tab_groups::TabGroupSyncServiceInitializedObserver tgss_observer{
        tgss_service};
    tgss_observer.Wait();
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

class BrowserTabStripControllerTestAddTabActiveGroupEnabled
    : public BrowserTabStripControllerTestBase {
 public:
  BrowserTabStripControllerTestAddTabActiveGroupEnabled() {
    scoped_feature_list_.InitWithFeatures({features::kNewTabAddsToActiveGroup},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class BrowserTabStripControllerTestAddTabActiveGroupDisabled
    : public BrowserTabStripControllerTestBase {
 public:
  BrowserTabStripControllerTestAddTabActiveGroupDisabled() {
    scoped_feature_list_.InitWithFeatures({},
                                          {features::kNewTabAddsToActiveGroup});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserTabStripControllerTestAddTabActiveGroupEnabled,
                       AddTabsWithActiveTabGroup) {
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  EXPECT_EQ(tab_strip_model()->count(), 4);

  tab_groups::TabGroupId group_id = tab_strip_model()->AddToNewGroup({1, 2});

  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));

  // Select a tab in the group.
  controller()->SelectTab(1, dummy_event_);
  controller()->CreateNewTab();

  // Create a new tab, it should be at position 3 because
  // there is an active tab group
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(0));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(3));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(4));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(5));

  // Switch to the first tab, which is not in the group and then make a new
  // tab, make sure it is at the end of the tab strip and it is not in the
  // group.
  controller()->SelectTab(0, dummy_event_);
  controller()->CreateNewTab();

  EXPECT_EQ(tab_strip_model()->count(), 6);
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(0));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(3));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(4));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(5));
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripControllerTestAddTabActiveGroupDisabled,
                       AddTabsWithActiveTabGroupFeatureDisabled) {
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  EXPECT_EQ(tab_strip_model()->count(), 4);

  tab_groups::TabGroupId group_id = tab_strip_model()->AddToNewGroup({1, 2});

  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));

  // Select a tab in the group.
  controller()->SelectTab(1, dummy_event_);
  ASSERT_TRUE(tabstrip()->tab_at(1)->IsActive());
  controller()->CreateNewTab();

  // Create a new tab, it should not have been added to the group even
  // though a tab in the group is selected.
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(0));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_id, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(3));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(4));
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripControllerTestAddTabActiveGroupDisabled,
                       VerifyTabMetricsFeatureDisabled1) {
  base::HistogramTester histogram_tester;

  // Make a tab, put it in group A
  controller()->CreateNewTab();
  tab_groups::TabGroupId group_a = tab_strip_model()->AddToNewGroup({1});

  // Select the tab in group A and then make two tabs. Of these two tabs,
  // group the first one to A
  controller()->SelectTab(1, dummy_event_);
  controller()->CreateNewTab();
  controller()->SelectTab(1, dummy_event_);
  controller()->CreateNewTab();
  controller()->AddTabToGroup(2, group_a);

  // Select the ungrouped tab and make a new tab. Put the new tab in
  // a new group B. Place the ungrouped tab in B.
  controller()->SelectTab(3, dummy_event_);
  controller()->CreateNewTab();
  tab_groups::TabGroupId group_b = tab_strip_model()->AddToNewGroup({4});
  controller()->AddTabToGroup(3, group_b);

  FastForwardPastDelay();
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kUngroupedToUngrouped,
      0);
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kUngroupedToGrouped, 2);
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kGroupedToUngrouped, 0);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      tabs::TabGroupingTransitionType::kGroupedToInPreviousGroup, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      tabs::TabGroupingTransitionType::kGroupedToOutsidePreviousGroup, 1);
  histogram_tester.ExpectTotalCount(kHistogramName, 4);

  EXPECT_EQ(group_a, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_a, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(group_b, tab_strip_model()->GetTabGroupForTab(3));
  EXPECT_EQ(group_b, tab_strip_model()->GetTabGroupForTab(4));

  WaitForTabSyncServiceInitialization();
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripControllerTestAddTabActiveGroupDisabled,
                       VerifyTabMetricsFeatureDisabled2) {
  base::HistogramTester histogram_tester;

  controller()->CreateNewTab();
  controller()->CreateNewTab();
  tab_groups::TabGroupId group_a = tab_strip_model()->AddToNewGroup({2});

  controller()->SelectTab(2, dummy_event_);
  controller()->CreateNewTab();

  FastForwardPastDelay();
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kUngroupedToUngrouped,
      1);
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kUngroupedToGrouped, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kGroupedToUngrouped, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      tabs::TabGroupingTransitionType::kGroupedToInPreviousGroup, 0);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      tabs::TabGroupingTransitionType::kGroupedToOutsidePreviousGroup, 0);
  histogram_tester.ExpectTotalCount(kHistogramName, 3);

  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_a, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(3));

  WaitForTabSyncServiceInitialization();
}

IN_PROC_BROWSER_TEST_F(BrowserTabStripControllerTestAddTabActiveGroupEnabled,
                       VerifyTabMetricsFeatureEnabled) {
  base::HistogramTester histogram_tester;

  // Make 5 tabs
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  controller()->CreateNewTab();
  controller()->CreateNewTab();

  // Add first, third, and fourth to groups
  tab_groups::TabGroupId group_a = tab_strip_model()->AddToNewGroup({1});
  tab_groups::TabGroupId group_b = tab_strip_model()->AddToNewGroup({3});
  tab_groups::TabGroupId group_c = tab_strip_model()->AddToNewGroup({4});

  // For each tab group, select the tab in it and make a tab
  controller()->SelectTab(1, dummy_event_);
  controller()->CreateNewTab();
  // group B
  controller()->SelectTab(4, dummy_event_);
  controller()->CreateNewTab();
  // group C
  controller()->SelectTab(6, dummy_event_);
  controller()->CreateNewTab();

  // The ungrouped tab to the right of A, group it to A.
  controller()->AddTabToGroup(3, group_a);

  // The second tab in group B, group it to C
  controller()->AddTabToGroup(5, group_c);

  // Ungroup the third tab in group C
  controller()->RemoveTabFromGroup(7);

  FastForwardPastDelay();
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kUngroupedToUngrouped,
      1);
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kUngroupedToGrouped, 4);
  histogram_tester.ExpectBucketCount(
      kHistogramName, tabs::TabGroupingTransitionType::kGroupedToUngrouped, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      tabs::TabGroupingTransitionType::kGroupedToInPreviousGroup, 1);
  histogram_tester.ExpectBucketCount(
      kHistogramName,
      tabs::TabGroupingTransitionType::kGroupedToOutsidePreviousGroup, 1);
  histogram_tester.ExpectTotalCount(kHistogramName, 8);

  EXPECT_EQ(group_a, tab_strip_model()->GetTabGroupForTab(1));
  EXPECT_EQ(group_a, tab_strip_model()->GetTabGroupForTab(2));
  EXPECT_EQ(group_a, tab_strip_model()->GetTabGroupForTab(3));
  EXPECT_EQ(group_b, tab_strip_model()->GetTabGroupForTab(4));
  EXPECT_EQ(group_c, tab_strip_model()->GetTabGroupForTab(5));
  EXPECT_EQ(group_c, tab_strip_model()->GetTabGroupForTab(6));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(7));
  EXPECT_EQ(std::nullopt, tab_strip_model()->GetTabGroupForTab(8));

  WaitForTabSyncServiceInitialization();
}
