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
#include "base/uuid.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace tab_groups {
namespace {

void RecordGroupDeletedMetric(const SavedTabGroup& removed_group) {
  const base::TimeDelta duration_saved =
      base::Time::Now() - removed_group.creation_time_windows_epoch_micros();

  base::UmaHistogramCounts1M("TabGroups.SavedTabGroupLifespan",
                             duration_saved.InMinutes());

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Deleted"));
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
      return group1.update_time_windows_epoch_micros() >=
             group2.update_time_windows_epoch_micros();
    }
  } else if (position1.has_value() && !position2.has_value()) {
    return true;
  } else if (!position1.has_value() && position2.has_value()) {
    return false;
  } else {
    return group1.update_time_windows_epoch_micros() >=
           group2.update_time_windows_epoch_micros();
  }
}

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

void SavedTabGroupModel::Add(SavedTabGroup saved_group) {
  base::Uuid group_guid = saved_group.saved_guid();
  CHECK(!Contains(group_guid));

  // In V1, give a default position to groups if it is not already set.
  // In V2, do nothing because unpinned saved tab groups don't have position
  // set.
  // Shared tab groups don't support positions.
  if (!IsTabGroupsSaveUIUpdateEnabled() && !saved_group.is_shared_tab_group() &&
      !saved_group.position().has_value()) {
    saved_group.SetPosition(Count());
  }

  InsertGroupImpl(std::move(saved_group));

  for (auto& observer : observers_) {
    observer.SavedTabGroupAddedLocally(Get(group_guid)->saved_guid());
  }
}

void SavedTabGroupModel::Remove(const LocalTabGroupID tab_group_id) {
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

void SavedTabGroupModel::Remove(const base::Uuid& id) {
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

void SavedTabGroupModel::UpdateVisualData(
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

void SavedTabGroupModel::MakeTabGroupShared(
    const LocalTabGroupID& local_group_id,
    std::string collaboration_id) {
  const SavedTabGroup* group = Get(local_group_id);
  CHECK(group);
  CHECK(!group->is_shared_tab_group());

  // Make a deep copy of the group without fields which are not used in shared
  // tab groups. Create a new group and new tabs to generate new UUIDs. Note
  // that the new group will have the same local ID as the original group.
  SavedTabGroup shared_group =
      group->CloneAsSharedTabGroup(std::move(collaboration_id));

  // `local_group_id` needs to be associated with the new shared tab group.
  // First, clear the local ID from the old group. This will store the tab on
  // the disk, and in case of crash, the tab group will be duplicated on browser
  // restart. It's safer than resolving duplicate local group IDs on browser
  // startup.
  // The order is important here because `OnGroupClosedInTabStrip` will remove
  // all associated local tab IDs.
  OnGroupClosedInTabStrip(local_group_id);

  // Add the new shared group to the model and associate it with the same local
  // ID.
  Add(std::move(shared_group));

  // No additional observers are notified because all mutations are done using
  // the existing methods which should notify observers.
}

void SavedTabGroupModel::AddedFromSync(SavedTabGroup saved_group) {
  base::Uuid group_guid = saved_group.saved_guid();
  if (Contains(group_guid)) {
    return;
  }

  InsertGroupImpl(std::move(saved_group));

  for (auto& observer : observers_) {
    observer.SavedTabGroupAddedFromSync(Get(group_guid)->saved_guid());
  }
}

void SavedTabGroupModel::RemovedFromSync(const LocalTabGroupID tab_group_id) {
  if (!Contains(tab_group_id)) {
    return;
  }

  const std::optional<int> index = GetIndexOf(tab_group_id);
  SavedTabGroup removed_group = RemoveImpl(index.value());
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedFromSync(removed_group);
  }
}

void SavedTabGroupModel::RemovedFromSync(const base::Uuid& id) {
  if (!Contains(id)) {
    return;
  }

  const std::optional<int> index = GetIndexOf(id);
  SavedTabGroup removed_group = RemoveImpl(index.value());
  for (auto& observer : observers_) {
    observer.SavedTabGroupRemovedFromSync(removed_group);
  }
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
  std::optional<int> group_index = GetIndexOf(group_id);
  saved_tab_groups_[group_index.value()].AddTabLocally(tab);

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
  std::optional<int> group_index = GetIndexOf(group_id);

  if (saved_tab_groups_[group_index.value()].ContainsTab(tab_id)) {
    // This can happen when an out of sync SavedTabGroup sends a tab update.
    saved_tab_groups_[group_index.value()].ReplaceTabAt(tab_id, tab);
  } else {
    saved_tab_groups_[group_index.value()].AddTabFromSync(tab);
  }

  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedFromSync(group_id, tab_id);
  }
}

void SavedTabGroupModel::UpdateTabInGroup(const base::Uuid& group_id,
                                          SavedTabGroupTab tab) {
  SavedTabGroup* group = GetMutableGroup(group_id);
  CHECK(group);

  if (group->GetTab(tab.saved_tab_guid())->url() != tab.url()) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_SavedTabGroups_TabNavigated"));
  }

  // Make a copy before moving the `tab`.
  const base::Uuid tab_guid_copy = tab.saved_tab_guid();
  group->UpdateTab(std::move(tab));

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

void SavedTabGroupModel::RemoveTabFromGroupLocally(const base::Uuid& group_id,
                                                   const base::Uuid& tab_id) {
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
    Remove(group_id);
    return;
  }

  // TODO(crbug.com/40062298): Convert all methods to pass ids by value to
  // prevent UAFs. Also removes the need for a separate copy variable.
  const base::Uuid copy_tab_id = tab_id;
  saved_tab_groups_[index.value()].RemoveTabLocally(tab_id);

  // TODO(dljames): Update to use SavedTabGroupRemoveLocally and update the API
  // to pass a group_id and an optional tab_id.
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(group_id, copy_tab_id);
  }

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_TabRemoved"));
}

void SavedTabGroupModel::RemoveTabFromGroupFromSync(const base::Uuid& group_id,
                                                    const base::Uuid& tab_id) {
  std::optional<int> index = GetIndexOf(group_id);
  CHECK(index.has_value());
  const SavedTabGroup& group = saved_tab_groups_[index.value()];

  if (!group.ContainsTab(tab_id)) {
    return;
  }

  // Remove the group from the model if the last tab will be removed from it.
  if (group.saved_tabs().size() == 1) {
    RemovedFromSync(group_id);
    return;
  }

  // Copy `tab_id` to prevent uaf when ungrouping a saved tab: crbug/1401965.
  const base::Uuid copy_tab_id = tab_id;
  saved_tab_groups_[index.value()].RemoveTabFromSync(tab_id);

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

  if (!base::FeatureList::IsEnabled(
          kSavedTabGroupNotifyOnInteractionTimeChanged)) {
    return;
  }

  for (SavedTabGroupModelObserver& observer : observers_) {
    observer.SavedTabGroupLastUserInteractionTimeUpdated(group->saved_guid());
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

const SavedTabGroup* SavedTabGroupModel::MergeRemoteGroupMetadata(
    const base::Uuid& guid,
    const std::u16string& title,
    TabGroupColorId color,
    std::optional<size_t> position,
    std::optional<std::string> creator_cache_guid,
    std::optional<std::string> last_updater_cache_guid,
    base::Time update_time) {
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
      for (auto& group : saved_tab_groups_) {
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

void SavedTabGroupModel::LoadStoredEntries(std::vector<SavedTabGroup> groups,
                                           std::vector<SavedTabGroupTab> tabs) {
  // `entries` is not ordered such that groups are guaranteed to be
  // at the front of the vector. As such, we can run into the case where we
  // try to add a tab to a group that does not exist for us yet.
  for (SavedTabGroup& group : groups) {
    Add(std::move(group));
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
  CHECK(IsTabGroupsSaveUIUpdateEnabled());
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
  CHECK(IsTabGroupsSaveUIUpdateEnabled());
  if (!Contains(id)) {
    return;
  }
  const int index = GetIndexOf(id).value();
  SavedTabGroup saved_group = RemoveImpl(index);
  bool was_pinned = saved_group.is_pinned();
  saved_group.SetPinned(!saved_group.is_pinned());
  InsertGroupImpl(std::move(saved_group));
  for (auto& observer : observers_) {
    observer.SavedTabGroupUpdatedLocally(id, /*tab_guid=*/std::nullopt);
  }

  if (was_pinned) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_SavedTabGroups_Unpinned"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_SavedTabGroups_Pinned"));
  }
}

}  // namespace tab_groups
