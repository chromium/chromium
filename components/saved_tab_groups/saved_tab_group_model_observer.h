// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_

#include "base/guid.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/tab_groups/tab_group_id.h"

// Serves to notify any SavedTabGroupModel listeners that a change has occurred
// supply the SavedTabGroup that was changed.
class SavedTabGroupModelObserver {
 public:
  SavedTabGroupModelObserver(const SavedTabGroupModelObserver&) = delete;
  SavedTabGroupModelObserver& operator=(const SavedTabGroupModelObserver&) =
      delete;

  // Called when a saved tab group is added to the backend.
  virtual void SavedTabGroupAddedLocally(const base::GUID& guid) {}

  // Called when a saved tab group will be removed from the backend.
  virtual void SavedTabGroupRemovedLocally(const SavedTabGroup* removed_group) {
  }

  // Called when the title, tabs, or color change.
  virtual void SavedTabGroupUpdatedLocally(const base::GUID& guid) {}

  // Called when the order of saved tab groups in the bookmark bar are changed.
  // TODO(crbug/1372052): Figure out if we can maintain ordering of groups and
  // tabs in sync.
  virtual void SavedTabGroupReorderedLocally() {}

  // Called when sync / ModelTypeStore updates data.
  virtual void SavedTabGroupAddedFromSync(const base::GUID& guid) {}

  // TODO(crbug/1372072): Decide if we want to also remove the tabgroup from the
  // tabstrip if it is open, or just remove it from sync.
  virtual void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) {}

  virtual void SavedTabGroupUpdatedFromSync(const base::GUID& guid) {}

 protected:
  SavedTabGroupModelObserver() = default;
  virtual ~SavedTabGroupModelObserver() = default;
};

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_MODEL_OBSERVER_H_
