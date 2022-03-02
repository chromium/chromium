// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_

#include <vector>

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"

#include "base/observer_list.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

class Profile;
class SavedTabGroupModelObserver;
struct SavedTabGroup;

// Serves to maintain the current state of all saved tab groups in the current
// session.
class SavedTabGroupModel {
 public:
  SavedTabGroupModel();
  explicit SavedTabGroupModel(Profile* profile);
  SavedTabGroupModel(const SavedTabGroupModel&) = delete;
  SavedTabGroupModel& operator=(const SavedTabGroupModel& other) = delete;
  ~SavedTabGroupModel();

  const std::vector<SavedTabGroup>& saved_tab_groups() {
    return saved_tab_groups_;
  }

  bool Contains(const tab_groups::TabGroupId& tab_group_id) {
    return GetIndexOf(tab_group_id) >= 0;
  }

  int GetIndexOf(const tab_groups::TabGroupId tab_group_id);
  int Count() { return saved_tab_groups_.size(); }
  bool IsEmpty() { return Count() <= 0; }
  const SavedTabGroup* Get(const tab_groups::TabGroupId tab_group_id);
  Profile* profile() { return profile_; }

  // Updates a single tab groups visual data (title, color).
  void Update(const tab_groups::TabGroupId tab_group_id,
              const tab_groups::TabGroupVisualData* visual_data);

  // Add / Remove a single tab group from the model.
  void Remove(const tab_groups::TabGroupId tab_group_id);
  void Add(const SavedTabGroup& saved_group);

  void AddObserver(SavedTabGroupModelObserver* observer);
  void RemoveObserver(SavedTabGroupModelObserver* observer);

 private:
  // The observers.
  base::ObserverList<SavedTabGroupModelObserver>::Unchecked observers_;
  std::vector<SavedTabGroup> saved_tab_groups_;
  Profile* profile_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_H_
