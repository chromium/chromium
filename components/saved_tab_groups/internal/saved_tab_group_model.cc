// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/saved_tab_group_model.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/internal/stats.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "google_apis/gaia/gaia_id.h"

namespace tab_groups {
namespace {

void RecordGroupDeletedMetric(const SavedTabGroup& removed_group) {
  const base::TimeDelta duration_saved =
      base::Time::Now() - removed_group.creation_time();

  base::UmaHistogramCounts1M("TabGroups.SavedTabGroupLifespan",
                             duration_saved.InMinutes());

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Deleted"));

  if (removed_group.is_shared_tab_group()) {
    base::UmaHistogramBoolean("TabGroups.Shared.GroupDeleted", true);
  }
}

// Compare function for 2 SavedTabGroup.
// SaveTabGroup with position set is always placed before the one without
// position set. If both have position set, the one with lower position number
// should place before. If both positions are the same or both are not set, the
// one with more recent update time should place before.
bool ShouldPlaceBefore(const SavedTabGroup& group1,
                       const SavedTabGroup& group2) {
  std::optional<size_t> position1 = group1.position();
  std::optional<size_t> position2 = group2.position();
  if (position1.has_value() && position2.has_value()) {
    if (position1.value() != position2.value()) {
      return position1.value() < position2.value();
    } else {
      return group1.update_time() >= group2.update_time();
    }
  } else if (position1.has_value() && !position2.has_value()) {
    return true;
  } else if (!position1.has_value() && position2.has_value()) {
    return false;
  } else {
    return group1.update_time() >= group2.update_time();
  }
}

// URL and title used for pending NTP.
const char kPendingNtpURL[] = "chrome://newtab/";
const char16_t kPendingNtpTitle[] = u"New tab";

}  // anonymous namespace

SavedTabGroupModel::SavedTabGroupModel() = default;
SavedTabGroupModel::~SavedTabGroupModel() = default;

std::vector<const SavedTabGroup*> SavedTabGroupModel::GetSavedTabGroupsOnly()
    const {
  std::vector<const SavedTabGroup*> saved_tab_groups;
  for (const SavedTabGroup& group : saved_tab_groups_) {
    if (!group.is_shared_tab_group()) {
      saved_tab_groups.push_back(&group);
    }
  }
  return saved_tab_groups;
}

std::vector<const SavedTabGroup*> SavedTabGroupModel::GetSharedTabGroupsOnly()
    const {
  std::vector<const SavedTabGroup*> shared_tab_groups;
  for (const SavedTabGroup& group : saved_tab_groups_) {
    if (group.is_shared_tab_group()) {
      shared_tab_groups.push_back(&group);
    }
  }
  return shared_tab_groups;
}

std::optional<int> SavedTabGroupModel::GetIndexOf(
    LocalTabGroupID tab_group_id) const {
  for (size_t i = 0; i < saved_tab_groups_.size(); i++) {
    if (saved_tab_groups_[i].local_group_id() == tab_group_id) {
      return i;
    }
  }

  return std::nullopt;
}

std::optional<int> SavedTabGroupModel::GetIndexOf(const base::Uuid& id) const {
  for (size_t i = 0; i < saved_tab_groups_.size(); i++) {
    if (saved_tab_groups_[i].saved_guid() == id) {
      return i;
    }
  }

  return std::nullopt;
}

std::optional<bool> SavedTabGroupModel::IsGroupPinned(
    const base::Uuid& id) const {
  std::optional<int> index = GetIndexOf(id);
  if (index.has_value()) {
    return saved_tab_groups_[index.value()].is_pinned();
  } else {
    return std::nullopt;
  }
}

const SavedTabGroup* SavedTabGroupModel::Get(const base::Uuid& id) const {
  std::optional<int> index = GetIndexOf(id);
  if (!index.has_value()) {
    return nullptr;
  }

  return &saved_tab_groups_[index.value()];
}

const SavedTabGroup* SavedTabGroupModel::Get(
    const LocalTabGroupID local_group_id) const {
  std::optional<int> index = GetIndexOf(local_group_id);
  if (!index.has_value()) {
    return nullptr;
  }

  return &saved_tab_groups_[index.value()];
}

void SavedTabGroupModel::AddedLocally(SavedTabGroup saved_group) {
  base::Uuid group_guid = saved_group.saved_guid();
  CHECK(!Contains(group_guid));

  InsertGroupImpl(std::move(saved_group));

  for (auto& observer : observers_) {
    observer.SavedTabGroupAddedLocally(group_guid);
  }
}

void SavedTabGroupModel::RemovedLocally(const LocalTabGroupID tab_group_id) {
  if (!Contains(tab_group_id)) {
    return;
  }

  const int index = GetIndexOf(tab_group_id).value();
  SavedTabGroup removed_group = RemoveImpl(index);

  UpdateGroupPositionsImpl();
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedLocally(removed_group);
  }

  RecordGroupDeletedMetric(removed_group);
}

void SavedTabGroupModel::RemovedLocally(const base::Uuid& id) {
  if (!Contains(id)) {
    return;
  }

  const int index = GetIndexOf(id).value();
  SavedTabGroup removed_group = RemoveImpl(index);

  UpdateGroupPositionsImpl();
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedLocally(removed_group);
  }

  RecordGroupDeletedMetric(removed_group);
}

void SavedTabGroupModel::UpdateVisualDataLocally(
    LocalTabGroupID tab_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(tab_group_id)) {
    return;
  }

  const std::optional<int> index = GetIndexOf(tab_group_id);
  UpdateVisualDataImpl(index.value(), visual_data);
  base::Uuid updated_guid = Get(tab_group_id)->saved_guid();
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(updated_guid,
                                         /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::MakeTabGroupSharedForTesting(
    const LocalTabGroupID& local_group_id,
    syncer::CollaborationId collaboration_id) {
  SavedTabGroup* const group = GetMutableGroup(local_group_id);
  group->SetCollaborationId(std::move(collaboration_id));
}

void SavedTabGroupModel::MakeTabGroupUnsharedForTesting(
    const LocalTabGroupID& local_group_id) {
  SavedTabGroup* const group = GetMutableGroup(local_group_id);
  group->SetCollaborationId(std::nullopt);
}

void SavedTabGroupModel::SetIsTransitioningToSaved(
    const LocalTabGroupID& local_group_id,
    bool is_transitioning_to_saved) {
  SavedTabGroup* const group = GetMutableGroup(local_group_id);
  group->SetIsTransitioningToSaved(is_transitioning_to_saved);
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group->saved_guid(),
                                         /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::AddedFromSync(SavedTabGroup saved_group) {
  base::Uuid group_guid = saved_group.saved_guid();
  if (Contains(group_guid)) {
    return;
  }

  stats::RecordEmptyGroupsMetricsOnGroupAddedFromSync(saved_group, is_loaded_);

  InsertGroupImpl(std::move(saved_group));

  // TODO(crbug.com/375636822): Doing this before `is_loaded_ == true` is
  // problematic.
  for (auto& observer : observers_) {
    observer.SavedTabGroupAddedFromSync(Get(group_guid)->saved_guid());
  }
}

void SavedTabGroupModel::RemovedFromSync(const LocalTabGroupID tab_group_id) {
  if (!Contains(tab_group_id)) {
    return;
  }

  const std::optional<int> index = GetIndexOf(tab_group_id);
  HandleTabGroupRemovedFromSync(index.value());
}

void SavedTabGroupModel::RemovedFromSync(const base::Uuid& id) {
  if (!Contains(id)) {
    return;
  }

  const std::optional<int> index = GetIndexOf(id);
  HandleTabGroupRemovedFromSync(index.value());
}

void SavedTabGroupModel::UpdatedVisualDataFromSync(
    LocalTabGroupID tab_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(tab_group_id)) {
    return;
  }

  const std::optional<int> index = GetIndexOf(tab_group_id);
  UpdateVisualDataImpl(index.value(), visual_data);
  base::Uuid updated_guid = Get(tab_group_id)->saved_guid();
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(updated_guid,
                                          /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::UpdatedVisualDataFromSync(
    const base::Uuid& id,
    const tab_groups::TabGroupVisualData* visual_data) {
  if (!Contains(id)) {
    return;
  }

  const std::optional<int> index = GetIndexOf(id);
  UpdateVisualDataImpl(index.value(), visual_data);
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(id, /*tab_guid=*/std::nullopt);
  }
}

const SavedTabGroup* SavedTabGroupModel::GetGroupContainingTab(
    const base::Uuid& saved_tab_guid) const {
  for (auto& saved_group : saved_tab_groups_) {
    if (saved_group.ContainsTab(saved_tab_guid)) {
      return &saved_group;
    }
  }

  return nullptr;
}

const SavedTabGroup* SavedTabGroupModel::GetGroupContainingTab(
    const LocalTabID& local_tab_id) const {
  for (auto& saved_group : saved_tab_groups_) {
    if (saved_group.ContainsTab(local_tab_id)) {
      return &saved_group;
    }
  }

  return nullptr;
}

void SavedTabGroupModel::AddTabToGroupLocally(const base::Uuid& group_id,
                                              SavedTabGroupTab tab) {
  if (!Contains(group_id)) {
    return;
  }

  const base::Uuid tab_id = tab.saved_tab_guid();
  SavedTabGroup& group = saved_tab_groups_[GetIndexOf(group_id).value()];

  stats::RecordEmptyGroupsMetricsOnTabAddedLocally(group, tab, is_loaded_);

  group.AddTabLocally(std::move(tab));

  // When adding a tab locally, we should also check for any pending NTP
  // and start syncing them.
  StartSyncingPendingNtpIfAny(group);

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group_id, tab_id);
  }

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_TabAdded"));
}

void SavedTabGroupModel::AddTabToGroupFromSync(const base::Uuid& group_id,
                                               SavedTabGroupTab tab) {
  if (!Contains(group_id)) {
    return;
  }

  const base::Uuid tab_id = tab.saved_tab_guid();
  SavedTabGroup& group = saved_tab_groups_[GetIndexOf(group_id).value()];

  if (group.ContainsTab(tab_id)) {
    // This can happen when an out of sync SavedTabGroup sends a tab update.
    group.ReplaceTabAt(tab_id, std::move(tab));
  } else {
    stats::RecordEmptyGroupsMetricsOnTabAddedFromSync(group, tab, is_loaded_);

    group.AddTabFromSync(std::move(tab));

    // If there is a pending NTP in this group, merge it with the incoming tab.
    MergePendingNtpWithIncomingTabIfAny(group, tab_id);
  }

  // TODO(crbug.com/375636822): Doing this before `is_loaded_ == true` is
  // problematic.
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_id, tab_id);
  }
}

void SavedTabGroupModel::UpdateTabInGroup(const base::Uuid& group_id,
                                          SavedTabGroupTab tab,
                                          bool notify_observers) {
  SavedTabGroup* group = GetMutableGroup(group_id);
  CHECK(group);

  if (group->GetTab(tab.saved_tab_guid())->url() != tab.url()) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_SavedTabGroups_TabNavigated"));
  }

  // Make a copy before moving the `tab`.
  const base::Uuid tab_guid_copy = tab.saved_tab_guid();

  if (notify_observers) {
    // This is a locally generated navigation event. Update the navigation
    // timestamp of the SavedTabGroupTab since we will not get the tab
    // modification time back from sync in the standard way due to reflection
    // blocking.
    tab.SetNavigationTime(base::Time::Now());
  }

  group->UpdateTab(std::move(tab));

  if (!notify_observers) {
    return;
  }

  // Since the group has at least one synced tab now, start syncing any pending
  // NTP.
  StartSyncingPendingNtpIfAny(*group);

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group_id, tab_guid_copy);
  }
}

void SavedTabGroupModel::UpdateLocalTabId(const base::Uuid& group_id,
                                          SavedTabGroupTab tab,
                                          std::optional<LocalTabID> local_id) {
  std::optional<int> group_index = GetIndexOf(group_id);
  CHECK(group_index.has_value());
  tab.SetLocalTabID(local_id);
  saved_tab_groups_[group_index.value()].UpdateTab(tab);
}

void SavedTabGroupModel::RemoveTabFromGroupLocally(
    const base::Uuid& group_id,
    const base::Uuid& tab_id,
    std::optional<GaiaId> local_gaia_id) {
  if (!Contains(group_id)) {
    return;
  }

  std::optional<int> index = GetIndexOf(group_id);
  const SavedTabGroup& group = saved_tab_groups_[index.value()];

  if (!group.ContainsTab(tab_id)) {
    return;
  }

  // Remove the group from the model if the last tab will be removed from it.
  if (group.saved_tabs().size() == 1) {
    if (group.is_shared_tab_group()) {
      base::UmaHistogramBoolean("TabGroups.Shared.LastTabClosed2", true);
    }
    RemovedLocally(group_id);
    return;
  }

  // TODO(crbug.com/40062298): Convert all methods to pass ids by value to
  // prevent UAFs. Also removes the need for a separate copy variable.
  const base::Uuid copy_tab_id = tab_id;
  saved_tab_groups_[index.value()].RemoveTabLocally(tab_id,
                                                    std::move(local_gaia_id));

  // TODO(dljames): Update to use SavedTabGroupRemoveLocally and update the API
  // to pass a group_id and an optional tab_id.
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group_id, copy_tab_id);
  }

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_TabRemoved"));
}

void SavedTabGroupModel::RemoveTabFromGroupFromSync(
    const base::Uuid& group_id,
    const base::Uuid& tab_id,
    GaiaId removed_by,
    bool prevent_group_destruction_for_testing) {
  std::optional<int> index = GetIndexOf(group_id);
  CHECK(index.has_value());
  SavedTabGroup& group = saved_tab_groups_[index.value()];

  if (!group.ContainsTab(tab_id)) {
    return;
  }

  const base::Uuid copy_tab_id = tab_id;
  saved_tab_groups_[index.value()].RemoveTabFromSync(
      tab_id, std::move(removed_by), prevent_group_destruction_for_testing);

  // The group became empty because of last tab deletion from sync. It could be
  // a transient state. Create a pending NTP since UI can't handle empty
  // groups. Any subsequent navigation or tab addition from locally or from
  // sync will commit this pending NTP.
  if (group.saved_tabs().empty()) {
    CreatePendingNtp(group);
    // Update local observers so that the pending NTP is written to storage.
    SavedTabGroupTab* pending_ntp = FindPendingNtpInGroup(group);
    for (SavedTabGroupModelObserver& observer : observers_) {
      observer.SavedTabGroupUpdatedLocally(group_id,
                                           pending_ntp->saved_tab_guid());
    }
  }

  // TODO(dljames): Update to use SavedTabGroupRemoveFromSync and update the API
  // to pass a group_id and an optional tab_id.
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_id, copy_tab_id);
  }
}

void SavedTabGroupModel::MoveTabInGroupTo(const base::Uuid& group_id,
                                          const base::Uuid& tab_id,
                                          int new_index) {
  if (!Contains(group_id)) {
    return;
  }

  // Copy `tab_id` to prevent uaf when ungrouping a saved tab: crbug/1401965.
  const base::Uuid copy_tab_id = tab_id;
  std::optional<int> index = GetIndexOf(group_id);
  saved_tab_groups_[index.value()].MoveTabLocally(tab_id, new_index);

  for (SavedTabGroupModelObserver& observer : observers_) {
    // TODO(crbug.com/40919583): Consider further optimizations.
    observer.SavedTabGroupTabMovedLocally(group_id, copy_tab_id);
  }
}

void SavedTabGroupModel::UpdateLastUserInteractionTimeLocally(
    const LocalTabGroupID& local_group_id) {
  SavedTabGroup* group = GetMutableGroup(local_group_id);
  CHECK(group);

  group->SetLastUserInteractionTime(base::Time::Now());

  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupLastUserInteractionTimeUpdated(group->saved_guid());
  }
}

void SavedTabGroupModel::UpdateTabLastSeenTimeFromSync(
    const base::Uuid& group_id,
    const base::Uuid& tab_id,
    base::Time time) {
  SavedTabGroup* group = GetMutableGroup(group_id);
  CHECK(group);

  if (!group->is_shared_tab_group()) {
    return;
  }

  SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  // Only accept the incoming last seen time from sync if it is newer than what
  // we have locally.
  if (tab->last_seen_time().has_value() &&
      tab->last_seen_time().value() >= time) {
    return;
  }

  tab->SetLastSeenTime(time);

  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupTabLastSeenTimeUpdated(tab_id, TriggerSource::REMOTE);
  }
}

void SavedTabGroupModel::UpdateTabLastSeenTimeFromLocal(
    const base::Uuid& group_id,
    const base::Uuid& tab_id) {
  SavedTabGroup* group = GetMutableGroup(group_id);
  CHECK(group);

  if (!group->is_shared_tab_group()) {
    return;
  }

  SavedTabGroupTab* tab = group->GetTab(tab_id);
  CHECK(tab);

  // Only update the last seen time if the navigation time of the tab is newer.
  if (tab->last_seen_time().has_value() &&
      tab->last_seen_time() >= tab->navigation_time()) {
    return;
  }

  tab->SetLastSeenTime(tab->navigation_time());

  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupTabLastSeenTimeUpdated(tab_id, TriggerSource::LOCAL);
  }
}

void SavedTabGroupModel::UpdatePositionForSharedGroupFromSync(
    const base::Uuid& group_id,
    std::optional<size_t> position) {
  const SavedTabGroup* group = Get(group_id);
  if (!group || !group->is_shared_tab_group() ||
      group->position() == position) {
    return;
  }

  // Remove the tab group, set position and reinsert.
  const int index = GetIndexOf(group_id).value();
  SavedTabGroup saved_group = RemoveImpl(index);
  if (position.has_value()) {
    saved_group.SetPosition(position.value());
  } else {
    saved_group.SetPinned(false);
  }
  InsertGroupImpl(std::move(saved_group));

  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_id, /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::UpdateLastUpdaterCacheGuidForGroup(
    const std::optional<std::string>& cache_guid,
    const LocalTabGroupID& group_id,
    const std::optional<LocalTabID>& tab_id) {
  const std::optional<int> index = GetIndexOf(group_id);
  if (!index.has_value()) {
    return;
  }

  SavedTabGroup& group = saved_tab_groups_[index.value()];
  group.SetLastUpdaterCacheGuid(cache_guid);

  if (!tab_id.has_value()) {
    return;
  }

  auto* tab = group.GetTab(tab_id.value());
  if (tab) {
    tab->SetLastUpdaterCacheGuid(cache_guid);
  }
}

void SavedTabGroupModel::UpdateSharedAttribution(
    const LocalTabGroupID& group_id,
    const std::optional<LocalTabID>& tab_id,
    GaiaId updated_by) {
  SavedTabGroup* group = GetMutableGroup(group_id);
  CHECK(group);
  if (!tab_id.has_value()) {
    group->SetUpdatedByAttribution(std::move(updated_by));
    return;
  }

  SavedTabGroupTab* tab = group->GetTab(tab_id.value());
  if (tab) {
    tab->SetUpdatedByAttribution(std::move(updated_by));
  }

  // Do not notify observers to avoid having too many updates because this
  // method is called quite extensively and in most cases duplicates the other
  // updates.
}

const SavedTabGroup* SavedTabGroupModel::MergeRemoteGroupMetadata(
    const base::Uuid& guid,
    const std::u16string& title,
    TabGroupColorId color,
    std::optional<size_t> position,
    std::optional<std::string> creator_cache_guid,
    std::optional<std::string> last_updater_cache_guid,
    base::Time update_time,
    const GaiaId& updated_by) {
  CHECK(Contains(guid));

  // For unpinned groups, `pinned_index` should be std::nullopt since its
  // position doesn't matter.
  const int index = GetIndexOf(guid).value();
  const std::optional<size_t> pinned_index =
      saved_tab_groups_[index].is_pinned() ? std::optional<size_t>(index)
                                           : std::nullopt;

  // Merge group and get `preferred_pinned_index`.
  saved_tab_groups_[index].MergeRemoteGroupMetadata(
      title, color, position, creator_cache_guid, last_updater_cache_guid,
      update_time);
  if (saved_tab_groups_[index].is_shared_tab_group()) {
    saved_tab_groups_[index].SetUpdatedByAttribution(updated_by);
  }
  std::optional<size_t> preferred_pinned_index =
      saved_tab_groups_[index].position();

  if (pinned_index != preferred_pinned_index) {
    int new_index = 0;
    if (preferred_pinned_index.has_value()) {
      // If the group is pinned, find the pinned position to insert.
      new_index = preferred_pinned_index.value();
    } else {
      // If the group is unpinned, find the first unpinned group index to
      // insert.
      for (const SavedTabGroup& group : saved_tab_groups_) {
        if (group.is_pinned()) {
          ++new_index;
        }
      }
    }

    ReorderGroupFromSync(guid, std::min(std::max(new_index, 0), Count() - 1));
  }

  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(guid, /*tab_guid=*/std::nullopt);
  }

  // Note that `index` can't be used anymore because groups could be re-ordered.
  return Get(guid);
}

const SavedTabGroupTab* SavedTabGroupModel::MergeRemoteTab(
    const SavedTabGroupTab& remote_tab) {
  const base::Uuid& group_guid = remote_tab.saved_group_guid();
  const base::Uuid& tab_guid = remote_tab.saved_tab_guid();
  SavedTabGroup* const group = MutableGroupContainingTab(tab_guid);
  CHECK(group);
  CHECK_EQ(group->saved_guid(), group_guid);

  const std::optional<int> index = group->GetIndexOfTab(tab_guid);

  // TODO(crbug.com/370714643): check that remote tab always contains position.
  const int preferred_index = remote_tab.position().value_or(0);

  group->GetTab(tab_guid)->MergeRemoteTab(remote_tab);

  if (index != preferred_index) {
    const int num_tabs = group->saved_tabs().size();
    const int new_index =
        preferred_index < num_tabs ? preferred_index : num_tabs - 1;
    group->MoveTabFromSync(tab_guid, std::max(new_index, 0));
  }

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_guid, tab_guid);
  }

  return group->GetTab(tab_guid);
}

void SavedTabGroupModel::ReorderGroupLocally(const base::Uuid& id,
                                             int new_index) {
  ReorderGroupImpl(id, new_index);
  UpdateGroupPositionsImpl();
  for (auto& observer : observers_) {
    observer.SavedTabGroupReorderedLocally();
  }
}

void SavedTabGroupModel::ReorderGroupFromSync(const base::Uuid& id,
                                              int new_index) {
  ReorderGroupImpl(id, new_index);
  for (auto& observer : observers_) {
    observer.SavedTabGroupReorderedFromSync();
  }
}

std::pair<std::set<base::Uuid>, std::set<base::Uuid>>
SavedTabGroupModel::UpdateLocalCacheGuid(
    std::optional<std::string> old_cache_guid,
    std::optional<std::string> new_cache_guid) {
  std::set<base::Uuid> updated_group_ids;
  std::set<base::Uuid> updated_tab_ids;
  // Update the group cache guids.
  for (SavedTabGroup& saved_group : saved_tab_groups_) {
    // Only saved tab groups use creator cache GUID.
    if (saved_group.is_shared_tab_group() ||
        saved_group.creator_cache_guid() != old_cache_guid) {
      continue;
    }

    saved_group.SetCreatorCacheGuid(new_cache_guid);
    updated_group_ids.insert(saved_group.saved_guid());
  }

  for (SavedTabGroup& saved_group : saved_tab_groups_) {
    // Only saved tab groups use creator cache GUID.
    if (saved_group.is_shared_tab_group()) {
      continue;
    }

    // Update the tabs in the group with the new cache guid.
    for (SavedTabGroupTab& saved_tab : saved_group.saved_tabs()) {
      if (saved_tab.creator_cache_guid() != old_cache_guid) {
        continue;
      }

      saved_tab.SetCreatorCacheGuid(new_cache_guid);
      updated_tab_ids.insert(saved_tab.saved_tab_guid());
    }
  }

  return std::make_pair(std::move(updated_group_ids),
                        std::move(updated_tab_ids));
}

void SavedTabGroupModel::CreatePendingNtp(SavedTabGroup& group) {
  CHECK(group.saved_tabs().empty());

  SavedTabGroupTab pending_ntp(GURL(kPendingNtpURL), kPendingNtpTitle,
                               group.saved_guid(), /*position=*/std::nullopt);
  pending_ntp.SetIsPendingNtp(true);
  group.AddTabLocally(std::move(pending_ntp));
}

void SavedTabGroupModel::StartSyncingPendingNtpIfAny(SavedTabGroup& group) {
  SavedTabGroupTab* pending_ntp = FindPendingNtpInGroup(group);
  if (pending_ntp) {
    pending_ntp->SetIsPendingNtp(false);
  }
}

void SavedTabGroupModel::MergePendingNtpWithIncomingTabIfAny(
    SavedTabGroup& group,
    const base::Uuid& tab_id) {
  SavedTabGroupTab* tab = group.GetTab(tab_id);
  CHECK(tab);

  SavedTabGroupTab* pending_ntp = FindPendingNtpInGroup(group);
  if (!pending_ntp) {
    return;
  }

  // Copy over local tab ID of the pending NTP to the incoming sync tab and then
  // delete it from the group.
  tab->SetLocalTabID(pending_ntp->local_tab_id());
  group.RemoveTabFromSync(pending_ntp->saved_tab_guid(),
                          /*removed_by=*/GaiaId());
}

SavedTabGroupTab* SavedTabGroupModel::FindPendingNtpInGroup(
    SavedTabGroup& group) {
  SavedTabGroupTab* pending_ntp = nullptr;
  size_t pending_ntp_count = 0;
  for (SavedTabGroupTab& saved_tab : group.saved_tabs()) {
    if (saved_tab.is_pending_ntp()) {
      pending_ntp = &saved_tab;
      pending_ntp_count++;
    }
  }

  // There should never be more than one pending NTP in a group.
  CHECK_LE(pending_ntp_count, 1u);
  return pending_ntp;
}

void SavedTabGroupModel::LoadStoredEntries(std::vector<SavedTabGroup> groups,
                                           std::vector<SavedTabGroupTab> tabs) {
  // `entries` is not ordered such that groups are guaranteed to be
  // at the front of the vector. As such, we can run into the case where we
  // try to add a tab to a group that does not exist for us yet.
  for (SavedTabGroup& group : groups) {
    // TODO(crbug.com/375636822): AddedLocally doesn't make sense here. This
    // should probably use a separate path that doesn't notify observers.
    AddedLocally(std::move(group));
  }
  UpdateGroupPositionsImpl();

  for (SavedTabGroupTab& tab : tabs) {
    base::Uuid group_id = tab.saved_group_guid();
    // The caller must guarantee that all `tabs` have a corresponding group.
    CHECK(Contains(group_id));
    AddTabToGroupFromSync(group_id, std::move(tab));
  }

  is_loaded_ = true;

  for (auto& observer : observers_) {
    observer.SavedTabGroupModelLoaded();
  }
}

void SavedTabGroupModel::OnGroupClosedInTabStrip(
    const LocalTabGroupID& tab_group_id) {
  const std::optional<int> index = GetIndexOf(tab_group_id);
  if (!index.has_value()) {
    return;
  }

  SavedTabGroup& saved_group = saved_tab_groups_[index.value()];
  saved_group.SetLocalGroupId(std::nullopt);

  // Remove the ID mappings from the tabs as well, since the group is closed.
  for (SavedTabGroupTab& saved_tab : saved_group.saved_tabs()) {
    saved_tab.SetLocalTabID(std::nullopt);
  }

  for (auto& observer : observers_) {
    observer.SavedTabGroupLocalIdChanged(saved_group.saved_guid());
  }

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Closed"));
}

void SavedTabGroupModel::OnGroupOpenedInTabStrip(
    const base::Uuid& id,
    const LocalTabGroupID& tab_group_id) {
  const std::optional<int> index = GetIndexOf(id);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0);

  SavedTabGroup& saved_group = saved_tab_groups_[index.value()];
  saved_group.SetLocalGroupId(tab_group_id);

  for (auto& observer : observers_) {
    observer.SavedTabGroupLocalIdChanged(saved_group.saved_guid());
  }
}

void SavedTabGroupModel::AddObserver(SavedTabGroupModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SavedTabGroupModel::RemoveObserver(SavedTabGroupModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SavedTabGroupModel::MigrateTabGroupSavesUIUpdate() {
  constexpr size_t kMaxNumberOfGroupToPin = 4;
  // Pin the first 4 saved tab groups from V1.
  for (size_t i = 0;
       i < std::min(saved_tab_groups_.size(), kMaxNumberOfGroupToPin); ++i) {
    saved_tab_groups_[i].SetPosition(i);
    for (auto& observer : observers_) {
      observer.SavedTabGroupUpdatedLocally(saved_tab_groups_[i].saved_guid(),
                                           /*tab_guid=*/std::nullopt);
    }
  }
}

void SavedTabGroupModel::MarkTransitionedToShared(
    const base::Uuid& shared_group_id) {
  SavedTabGroup* group = GetMutableGroup(shared_group_id);
  CHECK(group);
  group->MarkTransitionedToShared();
  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group->saved_guid(),
                                         /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::SetGroupHidden(
    const base::Uuid& originating_group_id) {
  SavedTabGroup* group = GetMutableGroup(originating_group_id);
  CHECK(group);
  group->SetIsHidden(true);
  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group->saved_guid(),
                                         /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::RestoreHiddenGroupFromSync(
    const base::Uuid& group_id) {
  SavedTabGroup* group = GetMutableGroup(group_id);
  CHECK(group);
  group->SetIsHidden(false);
  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group->saved_guid(),
                                          /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::OnSyncBridgeUpdateTypeChanged(
    SyncBridgeUpdateType sync_bridge_update_type) {
  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.OnSyncBridgeUpdateTypeChanged(sync_bridge_update_type);
  }
}

SavedTabGroup* SavedTabGroupModel::MutableGroupContainingTab(
    const base::Uuid& saved_tab_guid) {
  return const_cast<SavedTabGroup*>(GetGroupContainingTab(saved_tab_guid));
}

SavedTabGroup* SavedTabGroupModel::GetMutableGroup(
    const LocalTabGroupID& local_group_id) {
  return const_cast<SavedTabGroup*>(Get(local_group_id));
}

SavedTabGroup* SavedTabGroupModel::GetMutableGroup(const base::Uuid& id) {
  return const_cast<SavedTabGroup*>(Get(id));
}

void SavedTabGroupModel::ReorderGroupImpl(const base::Uuid& id, int new_index) {
  CHECK_GE(new_index, 0);
  CHECK_LT(new_index, Count());

  std::optional<int> index = GetIndexOf(id);
  CHECK(index.has_value());
  CHECK_GE(index.value(), 0);

  SavedTabGroup group = std::move(saved_tab_groups_[index.value()]);

  saved_tab_groups_.erase(saved_tab_groups_.begin() + index.value());
  saved_tab_groups_.emplace(saved_tab_groups_.begin() + new_index,
                            std::move(group));
}

void SavedTabGroupModel::UpdateGroupPositionsImpl() {
  for (size_t i = 0; i < saved_tab_groups_.size(); ++i) {
    //  Only update position for tab groups for which position is set.
    if (saved_tab_groups_[i].position().has_value()) {
      saved_tab_groups_[i].SetPosition(i);
    }
  }
}

void SavedTabGroupModel::InsertGroupImpl(SavedTabGroup group) {
  size_t index;
  for (index = 0; index < saved_tab_groups_.size(); ++index) {
    const SavedTabGroup& curr_group = saved_tab_groups_[index];

    if (ShouldPlaceBefore(group, curr_group)) {
      break;
    }
  }
  saved_tab_groups_.insert(saved_tab_groups_.begin() + index, std::move(group));
}

SavedTabGroup SavedTabGroupModel::RemoveImpl(size_t index) {
  CHECK_LT(index, saved_tab_groups_.size());
  SavedTabGroup removed_group = std::move(saved_tab_groups_[index]);
  saved_tab_groups_.erase(saved_tab_groups_.begin() + index);
  return removed_group;
}

void SavedTabGroupModel::UpdateVisualDataImpl(
    int index,
    const tab_groups::TabGroupVisualData* visual_data) {
  SavedTabGroup& saved_group = saved_tab_groups_[index];
  if (saved_group.title() == visual_data->title() &&
      saved_group.color() == visual_data->color()) {
    return;
  }

  saved_group.SetTitle(visual_data->title());
  saved_group.SetColor(visual_data->color());
}

void SavedTabGroupModel::TogglePinState(base::Uuid id) {
  if (!Contains(id)) {
    return;
  }
  const int index = GetIndexOf(id).value();
  SavedTabGroup saved_group = RemoveImpl(index);
  saved_group.SetPinned(!saved_group.is_pinned());
  InsertGroupImpl(std::move(saved_group));
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(id, /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::UpdateArchivalStatus(const base::Uuid& id,
                                              bool archival_status) {
  SavedTabGroup* const group = GetMutableGroup(id);
  CHECK(group);
  std::optional<base::Time> archival_time;
  if (archival_status) {
    archival_time = base::Time::Now();
  }
  group->SetArchivalTime(archival_time);

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(id, /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::UpdateBookmarkNodeId(
    const base::Uuid& id,
    const std::optional<base::Uuid>& bookmark_node_id) {
  SavedTabGroup* const group = GetMutableGroup(id);
  CHECK(group);
  group->SetBookmarkNodeId(bookmark_node_id);

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(id, /*tab_guid=*/std::nullopt);
  }
}

void SavedTabGroupModel::HandleTabGroupRemovedFromSync(int index) {
  // If this is a shared group that is transitioning to saved, make the
  // transition complete and that will delete the shared group during the
  // process.
  SavedTabGroup* group = &saved_tab_groups_[index];
  if (group->is_shared_tab_group() && group->is_transitioning_to_saved()) {
    for (auto& observer : observers_) {
      observer.TabGroupTransitioningToSavedRemovedFromSync(group->saved_guid());
    }
    return;
  }
  SavedTabGroup removed_group = RemoveImpl(index);
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedFromSync(removed_group);
  }
}
}  // namespace tab_groups
