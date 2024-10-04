// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SAVED_TAB_GROUP_MODEL_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SAVED_TAB_GROUP_MODEL_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace tab_groups {

class SavedTabGroupModelObserver;
class SavedTabGroup;

// Serves to maintain the current state of all saved tab groups in the current
// session.
class SavedTabGroupModel {
 public:
  SavedTabGroupModel();
  SavedTabGroupModel(const SavedTabGroupModel&) = delete;
  SavedTabGroupModel& operator=(const SavedTabGroupModel& other) = delete;
  ~SavedTabGroupModel();

  // Accessor for the underlying storage vector. Prefer the methods below to
  // distinguish between the saved and shared tab groups.
  const std::vector<SavedTabGroup>& saved_tab_groups() const {
    return saved_tab_groups_;
  }

  // Returns saved tab groups (which are not shared). The returned pointers can
  // be invalidated on any model's mutation.
  std::vector<const SavedTabGroup*> GetSavedTabGroupsOnly() const;

  // Returns shared tab groups. The returned pointers can be invalidated on any
  // model's mutation.
  std::vector<const SavedTabGroup*> GetSharedTabGroupsOnly() const;

  bool is_loaded() { return is_loaded_; }

  // Returns the index of the SavedTabGroup if it exists in the vector. Else
  // std::nullopt.
  std::optional<int> GetIndexOf(const LocalTabGroupID local_group_id) const;
  std::optional<int> GetIndexOf(const base::Uuid& id) const;

  // Return weather the group is pinned if it exists in the vector. Else
  // std::nullopt.
  std::optional<bool> IsGroupPinned(const base::Uuid& id) const;

  // Get a pointer to the SavedTabGroup from an ID. Returns nullptr if not in
  // vector.
  const SavedTabGroup* Get(const LocalTabGroupID local_group_id) const;
  const SavedTabGroup* Get(const base::Uuid& id) const;

  // Methods for checking if a group is in the SavedTabGroupModel.
  bool Contains(const LocalTabGroupID& local_group_id) const {
    return GetIndexOf(local_group_id).has_value();
  }
  bool Contains(const base::Uuid& id) const {
    return GetIndexOf(id).has_value();
  }

  // Helper for getting number of SavedTabGroups in the vector.
  int Count() const { return saved_tab_groups_.size(); }

  // Helper for getting empty state of the SavedTabGroup vector.
  bool IsEmpty() const { return Count() <= 0; }

  // Add / Remove / Update a single tab group from the model.
  void Add(SavedTabGroup saved_group);
  void Remove(const LocalTabGroupID local_group_id);
  void Remove(const base::Uuid& id);
  void UpdateVisualData(const LocalTabGroupID local_group_id,
                        const tab_groups::TabGroupVisualData* visual_data);

  // Make the tab group shared and associate it with the `collaboration_id`. The
  // tab group must exist and must not be shared.
  void MakeTabGroupShared(const LocalTabGroupID& local_group_id,
                          std::string collaboration_id);

  // Pin SavedTabGroup if it's unpinned. Unpin SavedTabGroup if it's pinned.
  void TogglePinState(base::Uuid id);

  // Similar to the Add/Remove/Update but originate from sync. As such, these
  // function do not notify sync observers of these changes to avoid looping
  // calls.
  void AddedFromSync(SavedTabGroup saved_group);
  void RemovedFromSync(const LocalTabGroupID local_group_id);
  void RemovedFromSync(const base::Uuid& id);
  void UpdatedVisualDataFromSync(
      const LocalTabGroupID local_group_id,
      const tab_groups::TabGroupVisualData* visual_data);
  void UpdatedVisualDataFromSync(
      const base::Uuid& id,
      const tab_groups::TabGroupVisualData* visual_data);

  const SavedTabGroup* GetGroupContainingTab(
      const base::Uuid& saved_tab_guid) const;
  const SavedTabGroup* GetGroupContainingTab(
      const LocalTabID& local_tab_id) const;

  // Adds a saved tab to `index` in the specified group denoted by `group_id` if
  // it exists. Notify local observers if the tab was added locally, and sync
  // observers if it was added from sync.
  void AddTabToGroupLocally(const base::Uuid& group_id, SavedTabGroupTab tab);
  void AddTabToGroupFromSync(const base::Uuid& group_id, SavedTabGroupTab tab);

  // Calls the UpdateTab method on a group found by group id in the model.
  // Calls the observer function SavedTabGroupUpdatedLocally.
  void UpdateTabInGroup(const base::Uuid& group_id, SavedTabGroupTab tab);

  // Updates `tab` with a new `local_id`. Unlike `UpdateTabInGroup`, this method
  // does not notify observers, as this is not a change we want to sync.
  void UpdateLocalTabId(const base::Uuid& group_id,
                        SavedTabGroupTab tab,
                        std::optional<LocalTabID> local_id);

  // Removes saved tab `tab_id` in the specified group denoted by `group_id` if
  // it exists. The group is deleted if the last tab is removed from it.
  // Notifies observers if the tab was removed locally.
  void RemoveTabFromGroupLocally(const base::Uuid& group_id,
                                 const base::Uuid& tab_id);

  // Similar to above but the group with `group_id` must exist. Notifies
  // observers that the tab was removed from sync.
  void RemoveTabFromGroupFromSync(const base::Uuid& group_id,
                                  const base::Uuid& tab_id);

  // Moves a saved tab from its current position to `index` in the specified
  // group denoted by `group_id` if it exists.
  void MoveTabInGroupTo(const base::Uuid& group_id,
                        const base::Uuid& tab_id,
                        int index);

  // Attempts to merge the remote group metadata or tab with the local object
  // that holds the same `guid`.
  const SavedTabGroup* MergeRemoteGroupMetadata(
      const base::Uuid& guid,
      const std::u16string& title,
      TabGroupColorId color,
      std::optional<size_t> position,
      std::optional<std::string> creator_cache_guid,
      std::optional<std::string> last_updater_cache_guid,
      base::Time update_time);
  const SavedTabGroupTab* MergeRemoteTab(const SavedTabGroupTab& remote_tab);

  // Changes the index of a given tab group by id. The new index provided is the
  // expected index after the group is removed. Notify local observers if the
  // group was reordered locally, and sync observers if the group was reordered
  // from sync.
  void ReorderGroupLocally(const base::Uuid& id, int new_index);
  void ReorderGroupFromSync(const base::Uuid& id, int new_index);

  // Update the creator cache guid for all saved groups that have
  // `old_cache_guid`, to `new_cache_guid`.
  std::pair<std::set<base::Uuid>, std::set<base::Uuid>> UpdateLocalCacheGuid(
      std::optional<std::string> old_cache_guid,
      std::optional<std::string> new_cache_guid);

  // Update the last interaction time with the group.
  void UpdateLastUserInteractionTimeLocally(
      const LocalTabGroupID& local_group_id);

  // Update the last updater cache guid for a give group and optionally a tab.
  void UpdateLastUpdaterCacheGuidForGroup(
      const std::optional<std::string>& cache_guid,
      const LocalTabGroupID& group_id,
      const std::optional<LocalTabID>& tab_id);

  // Loads the model from the storage. `tabs` must have a corresponding group in
  // `groups`.
  void LoadStoredEntries(std::vector<SavedTabGroup> groups,
                         std::vector<SavedTabGroupTab> tabs);

  // Functions that should be called when a SavedTabGroup's corresponding
  // TabGroup is closed or opened.
  void OnGroupOpenedInTabStrip(const base::Uuid& id,
                               const LocalTabGroupID& local_group_id);
  void OnGroupClosedInTabStrip(const LocalTabGroupID& local_group_id);

  // Add/Remove observers for this model.
  void AddObserver(SavedTabGroupModelObserver* observer);
  void RemoveObserver(SavedTabGroupModelObserver* observer);

  // One time migration of saved tab groups from v1 to v2.
  void MigrateTabGroupSavesUIUpdate();

 private:
  // Returns mutable group containing tab with ID `saved_tab_guid`, otherwise
  // returns null.
  SavedTabGroup* MutableGroupContainingTab(const base::Uuid& saved_tab_guid);
  SavedTabGroup* GetMutableGroup(const LocalTabGroupID& local_group_id);
  SavedTabGroup* GetMutableGroup(const base::Uuid& id);

  // Moves the group denoted by `id` to the position `new_index`.
  void ReorderGroupImpl(const base::Uuid& id, int new_index);

  // Updates all group positions to match the index they are currently stored
  // at.
  void UpdateGroupPositionsImpl();

  // Insert `group` into sorted order based on its position compared to already
  // stored groups in `saved_tab_groups_`. It should be noted that
  // `saved_tab_groups` must already be in sorted order for this function to
  // work as intended. To do this, UpdatePositionsImpl() can be called.
  void InsertGroupImpl(SavedTabGroup group);

  // Implementations of CRUD operations.
  SavedTabGroup RemoveImpl(size_t index);
  void UpdateVisualDataImpl(int index,
                            const tab_groups::TabGroupVisualData* visual_data);

  // Obsevers of the model.
  base::ObserverList<SavedTabGroupModelObserver>::Unchecked observers_;

  // True when SavedTabGroupModel::LoadStoredEntries has finished, false
  // otherwise.
  bool is_loaded_ = false;

  // Storage of all saved tab groups in the order they are displayed. The
  // position of the groups must maintain sorted order as sync may not propagate
  // an entire update completely leaving us with missing groups / gaps between
  // the positions.
  std::vector<SavedTabGroup> saved_tab_groups_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SAVED_TAB_GROUP_MODEL_H_
