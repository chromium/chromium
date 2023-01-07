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

class Profile;
class SavedTabGroupModelObserver;
class SavedTabGroup;

// Serves to maintain the current state of all saved tab groups in the current
// session.
class SavedTabGroupModel {
 public:
  SavedTabGroupModel();
  explicit SavedTabGroupModel(Profile* profile);
  SavedTabGroupModel(const SavedTabGroupModel&) = delete;
  SavedTabGroupModel& operator=(const SavedTabGroupModel& other) = delete;
  ~SavedTabGroupModel();

  // Accessor for the underlying storage vector.
  const std::vector<SavedTabGroup>& saved_tab_groups() const {
    return saved_tab_groups_;
  }
  Profile* profile() const { return profile_; }
  std::vector<SavedTabGroup> saved_tab_groups() { return saved_tab_groups_; }

  // Returns the index of the SavedTabGroup if it exists in the vector. Else
  // absl::nullopt.
  absl::optional<int> GetIndexOf(
      const tab_groups::TabGroupId local_group_id) const;
  absl::optional<int> GetIndexOf(const base::GUID& id) const;

  // Get a pointer to the SavedTabGroup from an ID. Returns nullptr if not in
  // vector.
  const SavedTabGroup* Get(const tab_groups::TabGroupId local_group_id) const;
  const SavedTabGroup* Get(const base::GUID& id) const;
  // TODO(crbug/1372503): Remove non-const accessor functions.
  SavedTabGroup* Get(const tab_groups::TabGroupId local_group_id);
  SavedTabGroup* Get(const base::GUID& id);

  // Methods for checking if a group is in the SavedTabGroupModel.
  bool Contains(const tab_groups::TabGroupId& local_group_id) const {
    return GetIndexOf(local_group_id).has_value();
  }
  bool Contains(const base::GUID& id) const {
    return GetIndexOf(id).has_value();
  }

  // Helper for getting number of SavedTabGroups in the vector.
  int Count() const { return saved_tab_groups_.size(); }

  // Helper for getting empty state of the SavedTabGroup vector.
  bool IsEmpty() const { return Count() <= 0; }

  // Add / Remove / Update a single tab group from the model.
  void Add(SavedTabGroup saved_group);
  void Remove(const tab_groups::TabGroupId local_group_id);
  void Remove(const base::GUID& id);
  void UpdateVisualData(const tab_groups::TabGroupId local_group_id,
                        const tab_groups::TabGroupVisualData* visual_data);
  void UpdateVisualData(const base::GUID& id,
                        const tab_groups::TabGroupVisualData* visual_data);

  // Similar to the Add/Remove/Update but originate from sync. As such, these
  // function do not notify sync observers of these changes to avoid looping
  // calls.
  void AddedFromSync(SavedTabGroup saved_group);
  void RemovedFromSync(const tab_groups::TabGroupId local_group_id);
  void RemovedFromSync(const base::GUID& id);
  void UpdatedVisualDataFromSync(
      const tab_groups::TabGroupId local_group_id,
      const tab_groups::TabGroupVisualData* visual_data);
  void UpdatedVisualDataFromSync(
      const base::GUID& id,
      const tab_groups::TabGroupVisualData* visual_data);

  // Adds a saved tab to `index` in the specified group denoted by `group_id` if
  // it exists.
  void AddTabToGroup(const base::GUID& group_id,
                     SavedTabGroupTab tab,
                     int index);

  // Removes a saved tab from `index` in the specified group denoted by
  // `group_id` if it exists.
  void RemoveTabFromGroup(const base::GUID& group_id, const base::GUID& tab_id);

  // Replaces a saved tab at `index` in the specified group denoted by
  // `group_id` if it exists.
  void ReplaceTabInGroupAt(const base::GUID& group_id,
                           const base::GUID& tab_id,
                           SavedTabGroupTab new_tab);

  // Moves a saved tab from its current position to `index` in the specified
  // group denoted by `group_id` if it exists.
  void MoveTabInGroupTo(const base::GUID& group_id,
                        const base::GUID& tab_id,
                        int index);

  // Attempts to merge the sync_specific with the local object that holds the
  // same guid.
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> MergeGroup(
      std::unique_ptr<sync_pb::SavedTabGroupSpecifics> sync_specific);
  std::unique_ptr<sync_pb::SavedTabGroupSpecifics> MergeTab(
      std::unique_ptr<sync_pb::SavedTabGroupSpecifics> sync_specific);

  // Changes the index of a given tab group by id. The new index provided is the
  // expected index after the group is removed.
  void Reorder(const base::GUID& id, int new_index);

  // Loads the entries (a sync_pb::SavedTabGroupSpecifics can be a group or a
  // tab) saved locally in the model type store (local storage) and attempts to
  // reconstruct the model by matching groups with their tabs using their
  // `group_id`'s. We do this by adding the groups to the model first, then
  // populating them with their respective tabs. Note: Any tabs that do not have
  // a matching group, will be lost.
  void LoadStoredEntries(std::vector<sync_pb::SavedTabGroupSpecifics> entries);

  // Functions that should be called when a SavedTabGroup's corresponding
  // TabGroup is closed or opened.
  void OnGroupOpenedInTabStrip(const base::GUID& id,
                               const tab_groups::TabGroupId& local_group_id);
  void OnGroupClosedInTabStrip(const tab_groups::TabGroupId& local_group_id);

  // Add/Remove observers for this model.
  void AddObserver(SavedTabGroupModelObserver* observer);
  void RemoveObserver(SavedTabGroupModelObserver* observer);

 private:
  // Implementations of CRUD operations.
  std::unique_ptr<SavedTabGroup> RemoveImpl(int index);
  void UpdateVisualDataImpl(int index,
                            const tab_groups::TabGroupVisualData* visual_data);

  // Obsevers of the model.
  base::ObserverList<SavedTabGroupModelObserver>::Unchecked observers_;

  // Storage of all saved tab groups in the order they are displayed.
  std::vector<SavedTabGroup> saved_tab_groups_;

  // SavedTabGroupModels are created on a per profile basis with a keyed
  // service. Returns the Profile that made the SavedTabGroupModel
  raw_ptr<Profile> profile_ = nullptr;
};

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_
