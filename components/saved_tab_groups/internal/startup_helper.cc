// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/startup_helper.h"

#include "base/uuid.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

StartupHelper::StartupHelper(TabGroupSyncDelegate* delegate,
                             TabGroupSyncService* service)
    : service_(service), platform_delegate_(delegate) {
  CHECK(platform_delegate_);
  CHECK(service_);
}

StartupHelper::~StartupHelper() = default;

void StartupHelper::InitializeTabGroupSync() {
  CloseDeletedTabGroupsFromTabModel();
  CreateRemoteTabGroupForNewGroups();

  for (const auto& saved_tab_group : service_->GetAllGroups()) {
    auto local_tab_group_id = saved_tab_group.local_group_id();
    if (!local_tab_group_id) {
      continue;
    }

    // First update the tab ID mappings left to right.
    MapTabIdsForGroup(*local_tab_group_id, saved_tab_group);

    // Update the local to group to match sync. As the group was modified, query
    // it again to have the updated one.
    auto updated_saved_group = service_->GetGroup(saved_tab_group.saved_guid());
    if (updated_saved_group) {
      platform_delegate_->UpdateLocalTabGroup(updated_saved_group.value());
    }
  }
}

void StartupHelper::CloseDeletedTabGroupsFromTabModel() {
  auto deleted_group_ids = service_->GetDeletedGroupIds();
  // TODO(shaktisahu): Maybe check if the tab group exists in the current
  // window.
  for (const auto& group_id : deleted_group_ids) {
    platform_delegate_->CloseLocalTabGroup(group_id);
  }
}

void StartupHelper::CreateRemoteTabGroupForNewGroups() {
  auto local_group_ids = platform_delegate_->GetLocalTabGroupIds();
  for (const auto& group_id : local_group_ids) {
    if (service_->GetGroup(group_id).has_value()) {
      continue;
    }

    // This is a local group not known to sync. Add it to sync.
    platform_delegate_->CreateRemoteTabGroup(group_id);
  }
}

void StartupHelper::MapTabIdsForGroup(const LocalTabGroupID& local_tab_group_id,
                                      const SavedTabGroup& saved_tab_group) {
  std::vector<LocalTabID> local_tab_ids =
      platform_delegate_->GetLocalTabIdsForTabGroup(local_tab_group_id);
  // Since we haven't run UpdateTabGroup yet, the number of tabs might be
  // different between local and sync versions of the tab group.
  // Regardless, update the in-memory tab ID mappings left to right.
  // The mismatch in number of tabs will be handled in the subsequent call to
  // UpdateLocalTabGroup.
  for (size_t i = 0;
       i < saved_tab_group.saved_tabs().size() && i < local_tab_ids.size();
       ++i) {
    const auto& saved_tab = saved_tab_group.saved_tabs()[i];
    service_->UpdateLocalTabId(local_tab_group_id, saved_tab.saved_tab_guid(),
                               local_tab_ids[i]);
  }
}

}  // namespace tab_groups
