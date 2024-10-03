// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_DELEGATE_EMPTY_TAB_GROUP_SYNC_DELEGATE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_DELEGATE_EMPTY_TAB_GROUP_SYNC_DELEGATE_H_

#include "base/uuid.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

// An empty implementation of the TabGroupSyncDelegate interface.
class EmptyTabGroupSyncDelegate : public TabGroupSyncDelegate {
 public:
  EmptyTabGroupSyncDelegate();
  ~EmptyTabGroupSyncDelegate() override;

  // Disallow copy/assign.
  EmptyTabGroupSyncDelegate(const EmptyTabGroupSyncDelegate&) = delete;
  EmptyTabGroupSyncDelegate& operator=(const EmptyTabGroupSyncDelegate&) =
      delete;

  // TabGroupSyncDelegate implementation.
  void HandleOpenTabGroupRequest(
      const base::Uuid& sync_tab_group_id,
      std::unique_ptr<TabGroupActionContext> context) override;
  std::unique_ptr<ScopedLocalObservationPauser>
  CreateScopedLocalObserverPauser() override;
  void CreateLocalTabGroup(const SavedTabGroup& tab_group) override;
  void CloseLocalTabGroup(const LocalTabGroupID& local_id) override;
  void DisconnectLocalTabGroup(const LocalTabGroupID& local_id) override;
  void UpdateLocalTabGroup(const SavedTabGroup& group) override;
  std::vector<LocalTabGroupID> GetLocalTabGroupIds() override;
  std::vector<LocalTabID> GetLocalTabIdsForTabGroup(
      const LocalTabGroupID& local_tab_group_id) override;
  void CreateRemoteTabGroup(const LocalTabGroupID& local_tab_group_id) override;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_DELEGATE_EMPTY_TAB_GROUP_SYNC_DELEGATE_H_
