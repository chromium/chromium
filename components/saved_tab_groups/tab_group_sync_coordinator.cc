// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_coordinator.h"

#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

TabGroupSyncCoordinator::TabGroupSyncCoordinator(
    std::unique_ptr<TabGroupSyncDelegate> delegate,
    TabGroupSyncService* service)
    : platform_delegate_(std::move(delegate)),
      service_(service),
      startup_helper_(platform_delegate_.get(), service_) {
  CHECK(platform_delegate_);
  CHECK(service_);

  service_->AddObserver(this);
}

TabGroupSyncCoordinator::~TabGroupSyncCoordinator() {
  service_->RemoveObserver(this);
}

void TabGroupSyncCoordinator::OnInitialized() {
  startup_helper_.InitializeTabGroupSync();
}

void TabGroupSyncCoordinator::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  platform_delegate_->HandleOpenTabGroupRequest(sync_tab_group_id,
                                                std::move(context));
}

void TabGroupSyncCoordinator::OnTabGroupAdded(const SavedTabGroup& group,
                                              TriggerSource source) {
  platform_delegate_->CreateLocalTabGroup(group);
}

void TabGroupSyncCoordinator::OnTabGroupUpdated(const SavedTabGroup& group,
                                                TriggerSource source) {
  platform_delegate_->UpdateLocalTabGroup(group);
}

void TabGroupSyncCoordinator::OnTabGroupRemoved(const LocalTabGroupID& local_id,
                                                TriggerSource source) {
  platform_delegate_->CloseLocalTabGroup(local_id);
}

void TabGroupSyncCoordinator::OnTabGroupRemoved(const base::Uuid& sync_id,
                                                TriggerSource source) {}

}  // namespace tab_groups
