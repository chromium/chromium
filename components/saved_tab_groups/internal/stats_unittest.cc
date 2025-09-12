// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups::stats {

class SavedTabGroupStatsTest : public testing::Test {
 public:
  void SetUp() override {
    // Advance time by a little bit to ensure that the creation time is not
    // zero.
    task_environment_.FastForwardBy(base::Minutes(1));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

TEST_F(SavedTabGroupStatsTest, RecordSavedTabGroupMetrics) {
  SavedTabGroupModel model;
  base::Uuid group_id = base::Uuid::GenerateRandomV4();
  SavedTabGroup group(u"Title", tab_groups::TabGroupColorId::kBlue, {},
                      std::nullopt, group_id);
  group.SetPinned(true);
  group.SetLocalGroupId(test::GenerateRandomTabGroupID());
  model.AddedLocally(group);

  SavedTabGroupTab tab(GURL("about:blank"), u"Title", group_id, 0);
  model.AddTabToGroupLocally(group_id, tab);

  RecordSavedTabGroupMetrics(&model);

  histogram_tester_.ExpectUniqueSample("TabGroups.SavedTabGroupCount", 1, 1);
  histogram_tester_.ExpectUniqueSample("TabGroups.SavedTabGroupTabCount", 1, 1);
  histogram_tester_.ExpectUniqueSample("TabGroups.SavedTabGroupPinnedCount", 1,
                                       1);
  histogram_tester_.ExpectUniqueSample("TabGroups.SavedTabGroupUnpinnedCount",
                                       0, 1);
  histogram_tester_.ExpectUniqueSample("TabGroups.SavedTabGroupActiveCount", 1,
                                       1);
  histogram_tester_.ExpectUniqueSample("TabGroups.SavedTabGroupOpenCount", 1,
                                       1);
  histogram_tester_.ExpectTotalCount("TabGroups.SavedTabGroupAge", 1);
  histogram_tester_.ExpectTotalCount(
      "TabGroups.SavedTabGroupTimeSinceModification", 1);
  histogram_tester_.ExpectTotalCount(
      "TabGroups.SavedTabGroupTabTimeSinceModification", 1);
}

TEST_F(SavedTabGroupStatsTest, RecordTabCountMismatchOnConnect) {
  RecordTabCountMismatchOnConnect(5, 10);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.SavedTabGroups.TabCountDifference.Positive", 5, 1);

  RecordTabCountMismatchOnConnect(10, 5);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.SavedTabGroups.TabCountDifference.Negative", 5, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordMigrationResult) {
  RecordMigrationResult(MigrationResult::kSpecificsToDataMigrationSuccess);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.SavedTabGroupSyncBridge.MigrationResult",
      MigrationResult::kSpecificsToDataMigrationSuccess, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordSpecificsParseFailureCount) {
  RecordSpecificsParseFailureCount(5, 10);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.SpecificsToDataMigration.ParseFailurePercentage", 50, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordParsedSavedTabGroupDataCount) {
  RecordParsedSavedTabGroupDataCount(5, 10);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.ParseSavedTabGroupDataEntries.ParseFailurePercentage", 50, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordTabGroupVisualsMetrics) {
  tab_groups::TabGroupVisualData visual_data(u"Title",
                                             tab_groups::TabGroupColorId::kBlue,
                                             /*is_collapsed=*/false);
  RecordTabGroupVisualsMetrics(&visual_data);
  histogram_tester_.ExpectUniqueSample("TabGroups.Sync.TabGroupTitleLength", 5,
                                       1);
}

TEST_F(SavedTabGroupStatsTest, RecordSharedTabGroupDataLoadFromDiskResult) {
  RecordSharedTabGroupDataLoadFromDiskResult(
      SharedTabGroupDataLoadFromDiskResult::kSuccess);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.Shared.LoadFromDiskResult",
      SharedTabGroupDataLoadFromDiskResult::kSuccess, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordEmptyGroupsMetricsOnLoad) {
  std::vector<SavedTabGroup> all_groups;
  std::vector<SavedTabGroupTab> all_tabs;
  std::unordered_set<base::Uuid, base::UuidHash> groups_with_filtered_tabs;

  // Not empty group.
  base::Uuid group_id_1 = base::Uuid::GenerateRandomV4();
  all_groups.emplace_back(u"Title", tab_groups::TabGroupColorId::kBlue,
                          std::vector<SavedTabGroupTab>(), std::nullopt,
                          group_id_1);
  all_tabs.emplace_back(GURL("about:blank"), u"Title", group_id_1, 0);

  // Already empty group.
  base::Uuid group_id_2 = base::Uuid::GenerateRandomV4();
  all_groups.emplace_back(u"Title", tab_groups::TabGroupColorId::kBlue,
                          std::vector<SavedTabGroupTab>(), std::nullopt,
                          group_id_2);

  // Became empty after removing duplicates.
  base::Uuid group_id_3 = base::Uuid::GenerateRandomV4();
  all_groups.emplace_back(u"Title", tab_groups::TabGroupColorId::kBlue,
                          std::vector<SavedTabGroupTab>(), std::nullopt,
                          group_id_3);
  groups_with_filtered_tabs.insert(group_id_3);

  RecordEmptyGroupsMetricsOnLoad(all_groups, all_tabs,
                                 groups_with_filtered_tabs);
  histogram_tester_.ExpectBucketCount(
      "TabGroups.SavedTabGroups.TabGroupLoadedEmptiness", 0, 1);
  histogram_tester_.ExpectBucketCount(
      "TabGroups.SavedTabGroups.TabGroupLoadedEmptiness", 1, 1);
  histogram_tester_.ExpectBucketCount(
      "TabGroups.SavedTabGroups.TabGroupLoadedEmptiness", 2, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordEmptyGroupsMetricsOnGroupAddedLocally) {
  SavedTabGroup group(u"Title", tab_groups::TabGroupColorId::kBlue, {},
                      std::nullopt);
  RecordEmptyGroupsMetricsOnGroupAddedLocally(group, true);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.Sync.AddedGroupIsEmptyLocally", true, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordEmptyGroupsMetricsOnGroupAddedFromSync) {
  SavedTabGroup group(u"Title", tab_groups::TabGroupColorId::kBlue, {},
                      std::nullopt);
  RecordEmptyGroupsMetricsOnGroupAddedFromSync(group, true);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.Sync.AddedGroupIsEmptyFromSync", true, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordEmptyGroupsMetricsOnTabAddedLocally) {
  SavedTabGroup group(u"Title", tab_groups::TabGroupColorId::kBlue, {},
                      std::nullopt);
  SavedTabGroupTab tab(GURL("about:blank"), u"Title", group.saved_guid(), 0);
  RecordEmptyGroupsMetricsOnTabAddedLocally(group, tab, true);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.Sync.TabAddedToEmptyGroupLocally", true, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordEmptyGroupsMetricsOnTabAddedFromSync) {
  SavedTabGroup group(u"Title", tab_groups::TabGroupColorId::kBlue, {},
                      std::nullopt);
  SavedTabGroupTab tab(GURL("about:blank"), u"Title", group.saved_guid(), 0);
  RecordEmptyGroupsMetricsOnTabAddedFromSync(group, tab, true);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.Sync.TabAddedToEmptyGroupFromSync", true, 1);
}

TEST_F(SavedTabGroupStatsTest, RecordSharedGroupTitleSanitization) {
  RecordSharedGroupTitleSanitization(true, TitleSanitizationType::kAddTab);
  histogram_tester_.ExpectUniqueSample("TabGroups.Shared.UseUrlForTitle.AddTab",
                                       true, 1);

  RecordSharedGroupTitleSanitization(false,
                                     TitleSanitizationType::kNavigateTab);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.Shared.UseUrlForTitle.NavigateTab", false, 1);

  RecordSharedGroupTitleSanitization(true,
                                     TitleSanitizationType::kShareTabGroup);
  histogram_tester_.ExpectUniqueSample(
      "TabGroups.Shared.UseUrlForTitle.ShareTabGroup", true, 1);
}

}  // namespace tab_groups::stats
