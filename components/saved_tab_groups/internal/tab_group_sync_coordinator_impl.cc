// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_coordinator_impl.h"

#include "base/uuid.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/internal/startup_helper.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

TabGroupSyncCoordinatorImpl::TabGroupSyncCoordinatorImpl(
    std::unique_ptr<TabGroupSyncDelegate> delegate,
    TabGroupSyncService* service,
    PrefService* pref_service)
    : service_(service),
      platform_delegate_(std::move(delegate)),
      startup_helper_(
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
          std::make_unique<StartupHelper>(platform_delegate_.get(),
                                          service_,
                                          pref_service)
#else
          nullptr
#endif
      ) {
  CHECK(platform_delegate_);
  CHECK(service_);
}

TabGroupSyncCoordinatorImpl::~TabGroupSyncCoordinatorImpl() = default;

void TabGroupSyncCoordinatorImpl::OnInitialized() {
  InitializeTabGroupSync();
}

void TabGroupSyncCoordinatorImpl::InitializeTabGroupSync() {
  if (startup_helper_) {
    startup_helper_->CloseDeletedTabGroupsFromTabModel();
    startup_helper_->HandleUnsavedLocalTabGroups();
  }

  auto batch_token = platform_delegate_->StartBatchOperation();
  // At this point, there should be no unsaved local groups. Update them to
  // match sync state.
  for (const SavedTabGroup* saved_tab_group : service_->ReadAllGroups()) {
    if (!saved_tab_group->local_group_id()) {
      continue;
    }

    base::Uuid sync_group_id = saved_tab_group->saved_guid();
    LocalTabGroupID local_group_id = saved_tab_group->local_group_id().value();
    ConnectLocalTabGroup(sync_group_id, local_group_id);

    // Update the local to group to match sync. As the group was modified, query
    // it again to have the updated one.
    auto updated_saved_group = service_->GetGroup(sync_group_id);
    if (updated_saved_group) {
      platform_delegate_->UpdateLocalTabGroup(updated_saved_group.value());
    }
  }
}

std::optional<LocalTabGroupID>
TabGroupSyncCoordinatorImpl::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  return platform_delegate_->HandleOpenTabGroupRequest(sync_tab_group_id,
                                                       std::move(context));
}

void TabGroupSyncCoordinatorImpl::ConnectLocalTabGroup(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  std::optional<SavedTabGroup> group = service_->GetGroup(sync_id);
  if (!group) {
    return;
  }

  std::vector<LocalTabID> local_tab_ids =
      platform_delegate_->GetLocalTabIdsForTabGroup(local_id);
  // Since we haven't run UpdateTabGroup yet, the number of tabs might be
  // different between local and sync versions of the tab group.
  // Regardless, update the in-memory tab ID mappings left to right.
  // The mismatch in number of tabs will be handled in the subsequent call to
  // UpdateLocalTabGroup.
  for (size_t i = 0; i < group->saved_tabs().size() && i < local_tab_ids.size();
       ++i) {
    const auto& saved_tab = group->saved_tabs()[i];
    service_->UpdateLocalTabId(local_id, saved_tab.saved_tab_guid(),
                               local_tab_ids[i]);
  }

  // Retrieve the group again which should have IDs mapped already. Now, update
  // the local tab URLs and group visuals to exactly match sync.
  group = service_->GetGroup(sync_id);
  platform_delegate_->ConnectLocalTabGroup(*group);
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupSyncCoordinatorImpl::CreateScopedLocalObserverPauser() {
  return platform_delegate_->CreateScopedLocalObserverPauser();
}

std::set<LocalTabID> TabGroupSyncCoordinatorImpl::GetSelectedTabs() {
  return platform_delegate_->GetSelectedTabs();
}

std::u16string TabGroupSyncCoordinatorImpl::GetTabTitle(
    const LocalTabID& local_tab_id) {
  return platform_delegate_->GetTabTitle(local_tab_id);
}

void TabGroupSyncCoordinatorImpl::DisconnectLocalTabGroup(
    const LocalTabGroupID& local_id) {
  platform_delegate_->DisconnectLocalTabGroup(local_id);
}

void TabGroupSyncCoordinatorImpl::OnTabGroupAdded(const SavedTabGroup& group,
                                                  TriggerSource source) {
  if (source != TriggerSource::REMOTE) {
    return;
  }
  // If the user has previously closed this tab group on this device, then don't
  // automatically open it again. (This can happen if the user signs out and
  // back in again.)
  if (service_->WasTabGroupClosedLocally(group.saved_guid())) {
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

void TabGroupSyncCoordinatorImpl::OnTabGroupRemoved(
    const LocalTabGroupID& local_id,
    TriggerSource source) {
  if (source != TriggerSource::REMOTE) {
    return;
  }

  platform_delegate_->CloseLocalTabGroup(local_id);
}

void TabGroupSyncCoordinatorImpl::OnTabGroupRemoved(const base::Uuid& sync_id,
                                                    TriggerSource source) {}

}  // namespace tab_groups
