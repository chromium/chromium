// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_creation_metrics_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace tabs {

constexpr char kHistogramName[] = "Tab.GroupingTransition2";

class TabCreationMetricsTest : public InProcessBrowserTest {
 public:
  TabCreationMetricsTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {}

  void SetUpOnMainThread() override {
    TabCreationMetricsController::SetTaskRunnerForTesting(task_runner_);
  }

  void TearDownOnMainThread() override {
    TabCreationMetricsController::SetTaskRunnerForTesting(nullptr);
  }

 protected:
  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }
  int TabCount() { return browser()->tab_strip_model()->count(); }

  void AppendTab() {
    chrome::AddTabAt(browser(), GURL(), -1, /*foreground=*/true);
  }

  void AppendTabToEndOfGroup(tab_groups::TabGroupId group_id) {
    const auto tabs =
        tab_strip_model()->group_model()->GetTabGroup(group_id)->ListTabs();
    tab_strip_model()->delegate()->AddTabAt(GURL(), /*index=*/tabs.end(),
                                            /*foreground=*/true, group_id);
    tab_strip_model()->ActivateTabAt(tabs.end());
  }

  tab_groups::TabGroupId AddTabToNewGroup(int tab_index) {
    tab_groups::TabGroupId group_id =
        tab_strip_model()->AddToNewGroup({tab_index});
    tab_strip_model()->ActivateTabAt(tab_index);
    return group_id;
  }

  void AddTabToExistingGroup(int tab_index, tab_groups::TabGroupId group_id) {
    tab_strip_model()->AddToExistingGroup({tab_index}, group_id);
    tab_strip_model()->ActivateTabAt(tab_index);
  }

  void UngroupTab(int tab_index) {
    tab_strip_model()->RemoveFromGroup({tab_index});
    tab_strip_model()->ActivateTabAt(tab_index);
  }

  void FastForwardPastDelay() {
    task_runner_->FastForwardBy(TabCreationMetricsController::kDelay +
                                base::Seconds(1));
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

IN_PROC_BROWSER_TEST_F(TabCreationMetricsTest, UngroupedToUngrouped) {
  base::HistogramTester histogram_tester;

  AppendTab();
  FastForwardPastDelay();

  histogram_tester.ExpectBucketCount(
      kHistogramName, TabGroupingTransitionType::kUngroupedToUngrouped, 1);
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(TabCreationMetricsTest, UngroupedToGrouped) {
  base::HistogramTester histogram_tester;

  AppendTab();
  // Group the newly created tab.
  AddTabToNewGroup(TabCount() - 1);
  FastForwardPastDelay();

  histogram_tester.ExpectBucketCount(
      kHistogramName, TabGroupingTransitionType::kUngroupedToGrouped, 1);
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(TabCreationMetricsTest, GroupedToUngrouped) {
  // Create a tab group with a tab.
  AppendTab();
  tab_groups::TabGroupId active_group_id = AddTabToNewGroup(TabCount() - 1);

  FastForwardPastDelay();
  base::HistogramTester histogram_tester;

  // Create a new tab in the active group.
  AppendTabToEndOfGroup(active_group_id);
  // Ungroup the newly created tab from the active group.
  UngroupTab(TabCount() - 1);
  FastForwardPastDelay();

  histogram_tester.ExpectBucketCount(
      kHistogramName, TabGroupingTransitionType::kGroupedToUngrouped, 1);
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(TabCreationMetricsTest, GroupedToInPreviousGroup) {
  // Create a tab group with a tab.
  AppendTab();
  tab_groups::TabGroupId group_id = AddTabToNewGroup(TabCount() - 1);

  FastForwardPastDelay();
  base::HistogramTester histogram_tester;

  // Create a new tab in the active group.
  AppendTabToEndOfGroup(group_id);
  FastForwardPastDelay();

  histogram_tester.ExpectBucketCount(
      kHistogramName, TabGroupingTransitionType::kGroupedToInPreviousGroup, 1);
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(TabCreationMetricsTest, GroupedToOutsidePreviousGroup) {
  // Create group A.
  AppendTab();
  tab_groups::TabGroupId group_id_a = AddTabToNewGroup(TabCount() - 1);

  // Create group B.
  AppendTab();
  tab_groups::TabGroupId group_id_b = AddTabToNewGroup(TabCount() - 1);

  FastForwardPastDelay();
  base::HistogramTester histogram_tester;

  // Add tab to group A then move to group B.
  AppendTabToEndOfGroup(group_id_a);
  AddTabToExistingGroup(1, group_id_b);
  FastForwardPastDelay();

  histogram_tester.ExpectBucketCount(
      kHistogramName, TabGroupingTransitionType::kGroupedToOutsidePreviousGroup,
      1);
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
}

}  // namespace tabs
