// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_DELEGATE_TAB_GROUP_SYNC_DELEGATE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_DELEGATE_TAB_GROUP_SYNC_DELEGATE_H_

#include <memory>

#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

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

  // Called to pause / resume the local observer.
  virtual std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() = 0;

  // Called to create a local tab group for the given sync representation.
  virtual void CreateLocalTabGroup(const SavedTabGroup& tab_group) = 0;

  // Called to close the specified local tab group.
  virtual void CloseLocalTabGroup(const LocalTabGroupID& local_id) = 0;

  // Called to stop listening for changes to a local group.
  virtual void DisconnectLocalTabGroup(const LocalTabGroupID& local_id) = 0;

  // Called to update a given tab group to match its sync representation.
  virtual void UpdateLocalTabGroup(const SavedTabGroup& group) = 0;

  // Called to get all the local tab group IDs across all local tab models.
  virtual std::vector<LocalTabGroupID> GetLocalTabGroupIds() = 0;

  // Called to get the local tab IDs associated with a given tab group.
  virtual std::vector<LocalTabID> GetLocalTabIdsForTabGroup(
      const LocalTabGroupID& local_tab_group_id) = 0;

  // Local To Remote mutation methods.

  // Called to create a remote tab group for the given local tab group ID.
  // Called on startup for any unsynced local tab groups and as well as meant to
  // be called from the local tab model observer when a new group is created.
  virtual void CreateRemoteTabGroup(
      const LocalTabGroupID& local_tab_group_id) = 0;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_DELEGATE_TAB_GROUP_SYNC_DELEGATE_H_
