// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_

#include <vector>

#include "base/memory/raw_ptr.h"

#include "base/observer_list.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

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

  // Returns the index of the SavedTabGroup if it exists in the vector. Else
  // returns -1.
  int GetIndexOf(const tab_groups::TabGroupId local_group_id) const;
  int GetIndexOf(const base::GUID& id) const;

  // Get a pointer to the SavedTabGroup from an ID. Returns nullptr if not in
  // vector.
  const SavedTabGroup* Get(const tab_groups::TabGroupId local_group_id) const;
  const SavedTabGroup* Get(const base::GUID& id) const;

  // Methods for checking if a group is in the SavedTabGroupModel.
  bool Contains(const tab_groups::TabGroupId& local_group_id) const {
    return GetIndexOf(local_group_id) >= 0;
  }
  bool Contains(const base::GUID& id) const { return GetIndexOf(id) >= 0; }

  // Helper for getting number of SavedTabGroups in the vector.
  int Count() const { return saved_tab_groups_.size(); }

  // Helper for getting empty state of the SavedTabGroup vector.
  bool IsEmpty() const { return Count() <= 0; }

  // Add / Remove a single tab group from the model.
  void Add(SavedTabGroup saved_group);
  void Remove(const tab_groups::TabGroupId local_group_id);
  void Remove(const base::GUID& id);
  void UpdateVisualData(const tab_groups::TabGroupId local_group_id,
                        const tab_groups::TabGroupVisualData* visual_data);
  void UpdateVisualData(const base::GUID& id,
                        const tab_groups::TabGroupVisualData* visual_data);

  // Changes the index of a given tab group by id. The new index provided is the
  // expected index after the group is removed.
  void Reorder(const base::GUID& id, int new_index);

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
  void RemoveImpl(int index);
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

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_
