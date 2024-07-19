// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_sync_coordinator_impl.h"

#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/types.h"

namespace tab_groups {

TabGroupSyncCoordinatorImpl::TabGroupSyncCoordinatorImpl(
    std::unique_ptr<TabGroupSyncDelegate> delegate,
    TabGroupSyncService* service)
    : platform_delegate_(std::move(delegate)),
      service_(service),
      startup_helper_(platform_delegate_.get(), service_) {
  CHECK(platform_delegate_);
  CHECK(service_);
}

TabGroupSyncCoordinatorImpl::~TabGroupSyncCoordinatorImpl() {
}

void TabGroupSyncCoordinatorImpl::OnInitialized() {
  startup_helper_.InitializeTabGroupSync();
}

void TabGroupSyncCoordinatorImpl::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  platform_delegate_->HandleOpenTabGroupRequest(sync_tab_group_id,
                                                std::move(context));
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupSyncCoordinatorImpl::CreateScopedLocalObserverPauser() {
  return platform_delegate_->CreateScopedLocalObserverPauser();
}

void TabGroupSyncCoordinatorImpl::OnTabGroupAdded(const SavedTabGroup& group,
                                              TriggerSource source) {
  if (source != TriggerSource::REMOTE) {
    return;
  }
  platform_delegate_->CreateLocalTabGroup(group);
}

void TabGroupSyncCoordinatorImpl::OnTabGroupUpdated(const SavedTabGroup& group,
                                                TriggerSource source) {
  if (source != TriggerSource::REMOTE) {
    return;
  }
  platform_delegate_->UpdateLocalTabGroup(group);
}

void TabGroupSyncCoordinatorImpl::OnTabGroupRemoved(const LocalTabGroupID& local_id,
                                                TriggerSource source) {
  if (source != TriggerSource::REMOTE) {
    return;
  }

  platform_delegate_->CloseLocalTabGroup(local_id);
}

void TabGroupSyncCoordinatorImpl::OnTabGroupRemoved(const base::Uuid& sync_id,
                                                TriggerSource source) {}

}  // namespace tab_groups
