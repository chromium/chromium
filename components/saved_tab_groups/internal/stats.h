// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STATS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STATS_H_

#include <stddef.h>

#include <unordered_set>

#include "base/uuid.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

class SavedTabGroupModel;
class SavedTabGroup;
class SavedTabGroupTab;
class TabGroupVisualData;

namespace stats {

// Please keep in sync with "SavedTabGroupSyncBridge.MigrationResult" in
// tools/metrics/histograms/metadata/tab/enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
//
// LINT.IfChange(MigrationResult)
enum class MigrationResult {
  kStoreCreateFailed = 0,
  kStoreLoadStarted = 1,
  kStoreLoadFailed = 2,
  kStoreLoadCompleted = 3,
  kSpecificsToDataMigrationStarted = 4,
  kSpecificsToDataMigrationParseFailedAtLeastOnce = 5,
  kSpecificsToDataMigrationWriteFailed = 6,
  kSpecificsToDataMigrationSuccess = 7,
  kSpecificsToDataMigrationAlreadyComplete = 8,
  kSharedPrefMigrationStarted = 9,
  kSharedPrefMigrationParseFailedAtLeastOnce = 10,
  kSharedPrefMigrationAtLeastOneEntryMigrated = 11,
  kSharedPrefMigrationWriteFailed = 12,
  kSharedPrefMigrationSuccess = 13,
  kReadAllMetadataFailed = 14,
  kReadAllMetadataSuccess = 15,
  kCount
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SavedTabGroupSyncBridge.MigrationResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SharedTabGroupDataLoadFromDiskResult)
enum class SharedTabGroupDataLoadFromDiskResult {
  kSuccess = 0,
  kFailedToParse = 1,
  kUnexpectedGuid = 2,
  kMissingCollaborationId = 3,
  kMissingGroupAndTab = 4,

  kMaxValue = kMissingGroupAndTab,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:SharedTabGroupDataLoadFromDiskResult)

// Records metrics about the state of model such as the number of saved groups,
// the number of tabs in each group, and more.
// Only used for desktop code that uses SavedTabGroupKeyedService. Soon to be
// deprecated.
void RecordSavedTabGroupMetrics(SavedTabGroupModel* model);

// Records the difference in the number of tabs between local group and the
// synced version when the local tab group is connected with the synced one.
void RecordTabCountMismatchOnConnect(size_t tabs_in_saved_group,
                                     size_t tabs_in_group);

// Records various migration step outcomes during initialization.
void RecordMigrationResult(MigrationResult migration_result);

// Records the number of entries in DataTypeStore that failed to parse as a
// specific during migration.
void RecordSpecificsParseFailureCount(int parse_failure_count,
                                      int total_entries);

// Records the number of entries in DataTypeStore that failed to parse as a
// SavedTabGroupData in the final stage of startup.
void RecordParsedSavedTabGroupDataCount(int parsed_entries_count,
                                        int total_entries);

// Records metrics related to tab group creation dialog.
void RecordTabGroupVisualsMetrics(
    const tab_groups::TabGroupVisualData* visual_data);

// Records the result of loading SharedTabGroupData from disk.
void RecordSharedTabGroupDataLoadFromDiskResult(
    SharedTabGroupDataLoadFromDiskResult result);

void RecordEmptyGroupsMetricsOnLoad(
    const std::vector<SavedTabGroup>& all_groups,
    const std::vector<SavedTabGroupTab>& all_tabs,
    const std::unordered_set<base::Uuid, base::UuidHash>&
        groups_with_filtered_tabs);

// Records whether the group is currently empty when it is added to the
// SavedTabGroupModel.
void RecordEmptyGroupsMetricsOnGroupAddedLocally(const SavedTabGroup& group,
                                                 bool model_is_loaded);
void RecordEmptyGroupsMetricsOnGroupAddedFromSync(const SavedTabGroup& group,
                                                  bool model_is_loaded);

// Records whether the group is currently empty, before the tab is added to the
// SavedTabGroupModel. The tab must not be in the group yet.
void RecordEmptyGroupsMetricsOnTabAddedLocally(const SavedTabGroup& group,
                                               const SavedTabGroupTab& tab,
                                               bool model_is_loaded);
void RecordEmptyGroupsMetricsOnTabAddedFromSync(const SavedTabGroup& group,
                                                const SavedTabGroupTab& tab,
                                                bool model_is_loaded);

}  // namespace stats
}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_STATS_H_
