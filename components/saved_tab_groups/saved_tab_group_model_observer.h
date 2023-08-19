// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_

#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Serves to notify any SavedTabGroupModel listeners that a change has occurred
// supply the SavedTabGroup that was changed.
class SavedTabGroupModelObserver {
 public:
  SavedTabGroupModelObserver(const SavedTabGroupModelObserver&) = delete;
  SavedTabGroupModelObserver& operator=(const SavedTabGroupModelObserver&) =
      delete;

  // Called when a saved tab group is added to the backend.
  virtual void SavedTabGroupAddedLocally(const base::Uuid& guid) {}

  // Called when a saved tab group will be removed from the backend.
  virtual void SavedTabGroupRemovedLocally(const SavedTabGroup* removed_group) {
  }

  // Called when the saved tab group is opened or closed locally.
  virtual void SavedTabGroupLocalIdChanged(const base::Uuid& saved_group_id) {}

  // Called when the title, tabs, or color change. `group_guid` denotes the
  // group that is currently being updated. `tab_guid` denotes if a tab in this
  // group was changed (added, removed, updated). Otherwise, only the group is
  // being changed.
  virtual void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const absl::optional<base::Uuid>& tab_guid = absl::nullopt) {}

  // Called when the order of tabs in an open saved tab group are changed in the
  // tabstrip.
  virtual void SavedTabGroupTabsReorderedLocally(const base::Uuid& group_guid) {
  }

  // Called when the order of saved tab groups in the bookmark bar are changed.
  virtual void SavedTabGroupReorderedLocally() {}

  // Happens when a group is reordered from sync.
  virtual void SavedTabGroupReorderedFromSync() {}

  // Called when sync / ModelTypeStore updates data.
  virtual void SavedTabGroupAddedFromSync(const base::Uuid& guid) {}

  // TODO(crbug/1372072): Decide if we want to also remove the tabgroup from the
  // tabstrip if it is open, or just remove it from sync.
  virtual void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) {}

  // Called when the title, tabs, or color change. `group_guid` denotes the
  // group that is currently being updated. `tab_guid` denotes if a tab in this
  // group was changed (added, removed, updated). Specifically, this is called
  // when addressing merge conflicts for duplicate groups and tabs.
  virtual void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const absl::optional<base::Uuid>& tab_guid = absl::nullopt) {}

  // Called when SavedTabGroupModel::LoadStoredEntries has finished loading.
  // This is currently used to notify the SavedTabGroupKeyedService to link any
  // tabs restore through session restore to the corresponding SavedTabGroup
  // metadata in the SavedTabGroupModel.
  virtual void SavedTabGroupModelLoaded() {}

 protected:
  SavedTabGroupModelObserver() = default;
  virtual ~SavedTabGroupModelObserver() = default;
};

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_
