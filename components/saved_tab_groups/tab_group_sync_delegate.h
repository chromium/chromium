// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_H_

#include <memory>

#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

// Home for platform specific logic for tab group sync. Contains helper methods
// for applying incoming remote updates to the local tab model, which platforms
// need to implement. Ideally should own a local mutation helper that will apply
// the mutation. Also should own the local tab model observer which will observe
// the local tab model and invoke TabGroupSyncService directly or via a remote
// mutation helper that will propagate the changes to sync.
class TabGroupSyncDelegate {
 public:
  virtual ~TabGroupSyncDelegate() = default;

  // Called to open a given saved tab group in the local tab model.
  // The `context` can be used to specify the browser window in which the tab
  // group should be opened.
  virtual void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) = 0;

  // Called to create a local tab group for the given sync representation.
  virtual void CreateLocalTabGroup(const SavedTabGroup& tab_group) = 0;

  // Called to close the specified local tab group.
  virtual void CloseLocalTabGroup(const LocalTabGroupID& local_id) = 0;

  // Called to update a given tab group to match its sync representation.
  virtual void UpdateLocalTabGroup(const SavedTabGroup& group) = 0;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_SYNC_DELEGATE_H_
