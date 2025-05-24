// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/startup_helper.h"

#include "base/uuid.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

StartupHelper::StartupHelper(TabGroupSyncDelegate* delegate,
                             TabGroupSyncService* service,
                             PrefService* pref_service)
    : platform_delegate_(delegate),
      service_(service),
      pref_service_(pref_service) {
  CHECK(platform_delegate_);
  CHECK(service_);
}

StartupHelper::~StartupHelper() = default;

void StartupHelper::CloseDeletedTabGroupsFromTabModel() {
  auto deleted_group_ids = service_->GetDeletedGroupIds();
  // TODO(shaktisahu): Maybe check if the tab group exists in the current
  // window.
  for (const auto& group_id : deleted_group_ids) {
    platform_delegate_->CloseLocalTabGroup(group_id);
  }
}

void StartupHelper::HandleUnsavedLocalTabGroups() {
  // Find the local tab groups that don't have a corresponding saved tab group.
  std::vector<LocalTabGroupID> tab_groups_not_known_to_sync;
  for (const auto& group_id : platform_delegate_->GetLocalTabGroupIds()) {
    if (!service_->GetGroup(group_id).has_value()) {
      tab_groups_not_known_to_sync.emplace_back(group_id);
    }
  }

  bool did_sync_tab_groups_in_last_session =
      pref_service_->GetBoolean(prefs::kDidSyncTabGroupsInLastSession);
  pref_service_->SetBoolean(prefs::kDidSyncTabGroupsInLastSession, true);
  for (const auto& group_id : tab_groups_not_known_to_sync) {
    if (did_sync_tab_groups_in_last_session) {
      // This is a local group not known to sync and we have passed through the
      // one time migration. Clearly something wrong (e.g. a shared tab group
      // left over previous session after the shared tab group feature
      // rollback). Close it.
      platform_delegate_->CloseLocalTabGroup(group_id);
    } else {
      // This is the first time feature launch for tab group sync. Add the group
      // to sync DB.
      auto saved_tab_group =
          platform_delegate_->CreateSavedTabGroupFromLocalGroup(group_id);
      if (saved_tab_group) {
        service_->AddGroup(std::move(*saved_tab_group));
      }
    }
  }
}

}  // namespace tab_groups
