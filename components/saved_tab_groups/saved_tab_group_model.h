// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Accessor for the underlying storage vector.
  const std::vector<SavedTabGroup>& saved_tab_groups() const {
    return saved_tab_groups_;
  }
  std::vector<SavedTabGroup> saved_tab_groups() { return saved_tab_groups_; }

  bool is_loaded() { return is_loaded_; }

  // Returns the index of the SavedTabGroup if it exists in the vector. Else
  // absl::nullopt.
  absl::optional<int> GetIndexOf(
      const tab_groups::TabGroupId local_group_id) const;
  absl::optional<int> GetIndexOf(const base::Uuid& id) const;

  // Get a pointer to the SavedTabGroup from an ID. Returns nullptr if not in
  // vector.
  const SavedTabGroup* Get(const tab_groups::TabGroupId local_group_id) const;
  const SavedTabGroup* Get(const base::Uuid& id) const;

  // Methods for checking if a group is in the SavedTabGroupModel.
  bool Contains(const tab_groups::TabGroupId& local_group_id) const {
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
  void Remove(const tab_groups::TabGroupId local_group_id);
  void Remove(const base::Uuid& id);
  void UpdateVisualData(const tab_groups::TabGroupId local_group_id,
                        const tab_groups::TabGroupVisualData* visual_data);
  void UpdateVisualData(const base::Uuid& id,
                        const tab_groups::TabGroupVisualData* visual_data);

  // Similar to the Add/Remove/Update but originate from sync. As such, these
  // function do not notify sync observers of these changes to avoid looping
  // calls.
  void AddedFromSync(SavedTabGroup saved_group);
  void RemovedFromSync(const tab_groups::TabGroupId local_group_id);
  void RemovedFromSync(const base::Uuid& id);
  void UpdatedVisualDataFromSync(
      const tab_groups::TabGroupId local_group_id,
      const tab_groups::TabGroupVisualData* visual_data);
  void UpdatedVisualDataFromSync(
      const base::Uuid& id,
      const tab_groups::TabGroupVisualData* visual_data);

  SavedTabGroup* GetGroupContainingTab(const base::Uuid& saved_tab_guid);
  SavedTabGroup* GetGroupContainingTab(const base::Token& local_tab_id);

  // Adds a saved tab to `index` in the specified group denoted by `group_id` if
  // it exists. If `update_tab_positions` is true, update the positions of all
  // tabs in the group.
  void AddTabToGroup(const base::Uuid& group_id,
                     SavedTabGroupTab tab,
                     bool update_tab_positions = false);

  // Calls the UpdateTab method on a group found by group id in the model.
  // Calls the observer function SavedTabGroupUpdatedLocally.
  void UpdateTabInGroup(const base::Uuid& group_id, SavedTabGroupTab tab);

  // Removes saved tab `tab_id` in the specified group denoted by
  // `group_id` if it exists. We delete the group instead if the last tab is
  // removed from it. If `update_tab_positions` is true, update the positions of
  // all tabs in the group and notify sync of the changes.
  void RemoveTabFromGroup(const base::Uuid& group_id,
                          const base::Uuid& tab_id,
                          bool update_tab_positions = false);

  // Replaces a saved tab `tab_id` in the specified group denoted by
  // `group_id` if it exists with `new_tab`.
  void ReplaceTabInGroupAt(const base::Uuid& group_id,
                           const base::Uuid& tab_id,
                           SavedTabGroupTab new_tab);

  // Moves a saved tab from its current position to `index` in the specified
  // group denoted by `group_id` if it exists.
  void MoveTabInGroupTo(const base::Uuid& group_id,
                        const base::Uuid& tab_id,
                        int index);

  // Attempts to merge the sync_specific with the local object that holds the
  // same guid.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> MergeGroup(
      const sync_pb::SavedTabGroupSpecifics& sync_specific);
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> MergeTab(
      const sync_pb::SavedTabGroupSpecifics& sync_specific);

  // Changes the index of a given tab group by id. The new index provided is the
  // expected index after the group is removed.
  void Reorder(const base::Uuid& id, int new_index);

  // Loads the entries (a sync_pb::SavedTabGroupSpecifics can be a group or a
  // tab) saved locally in the model type store (local storage) and attempts to
  // reconstruct the model by matching groups with their tabs using their
  // `group_id`'s. Note: Any tabs that do not have a matching group, will be
  // returned to the bridge to keep track of.
  std::vector<sync_pb::SavedTabGroupSpecifics> LoadStoredEntries(
      std::vector<sync_pb::SavedTabGroupSpecifics> entries);

  // Functions that should be called when a SavedTabGroup's corresponding
  // TabGroup is closed or opened.
  void OnGroupOpenedInTabStrip(const base::Uuid& id,
                               const tab_groups::TabGroupId& local_group_id);
  void OnGroupClosedInTabStrip(const tab_groups::TabGroupId& local_group_id);

  // Add/Remove observers for this model.
  void AddObserver(SavedTabGroupModelObserver* observer);
  void RemoveObserver(SavedTabGroupModelObserver* observer);

 private:
  // Updates all group positions to match the index they are currently stored
  // at.
  void UpdateGroupPositionsImpl();

  // Insert `group` into sorted order based on its position compared to already
  // stored groups in `saved_tab_groups_`. It should be noted that
  // `saved_tab_groups` must already be in sorted order for this function to
  // work as intended. To do this, UpdatePositionsImpl() can be called.
  void InsertGroupImpl(const SavedTabGroup& group);

  // Implementations of CRUD operations.
  std::unique_ptr<SavedTabGroup> RemoveImpl(int index);
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

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_
