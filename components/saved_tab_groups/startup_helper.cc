// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/startup_helper.h"

#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

StartupHelper::StartupHelper(TabGroupSyncDelegate* delegate,
                             TabGroupSyncService* service)
    : platform_delegate_(delegate), service_(service) {
  CHECK(platform_delegate_);
  CHECK(service_);
}

StartupHelper::~StartupHelper() = default;

void StartupHelper::InitializeTabGroupSync() {
  CloseDeletedTabGroupsFromTabModel();
  CreateRemoteTabGroupForNewGroups();
  ReconcileGroupsToSync();
  UpdateTabIdMappings();
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

void StartupHelper::ReconcileGroupsToSync() {
  // TODO(shaktisahu): Might prefer to loop over local tab groups instead.
  for (const auto& saved_tab_group : service_->GetAllGroups()) {
    // TODO(shaktisahu): Invoke special logic for merging on startup.
    platform_delegate_->UpdateLocalTabGroup(saved_tab_group);
  }
}

void StartupHelper::UpdateTabIdMappings() {
  // TODO(shaktisahu): Might prefer to loop over local tab groups instead.
  for (const auto& saved_tab_group : service_->GetAllGroups()) {
    auto local_tab_group_id = saved_tab_group.local_group_id();
    if (!local_tab_group_id) {
      continue;
    }

    std::vector<LocalTabID> local_tab_ids =
        platform_delegate_->GetLocalTabIdsForTabGroup(*local_tab_group_id);
    // TODO(b/350622883): Reenable the CHECK after fixing.
    // CHECK_EQ(saved_tab_group.saved_tabs().size(), local_tab_ids.size());
    for (size_t i = 0;
         i < saved_tab_group.saved_tabs().size() && i < local_tab_ids.size();
         ++i) {
      const auto& saved_tab = saved_tab_group.saved_tabs()[i];
      service_->UpdateLocalTabId(*local_tab_group_id,
                                 saved_tab.saved_tab_guid(), local_tab_ids[i]);
    }
  }
}

}  // namespace tab_groups
