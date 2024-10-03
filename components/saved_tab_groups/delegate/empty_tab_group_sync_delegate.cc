// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/delegate/empty_tab_group_sync_delegate.h"

#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"

namespace tab_groups {

EmptyTabGroupSyncDelegate::EmptyTabGroupSyncDelegate() = default;

EmptyTabGroupSyncDelegate::~EmptyTabGroupSyncDelegate() = default;

void EmptyTabGroupSyncDelegate::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {}

std::unique_ptr<ScopedLocalObservationPauser>
EmptyTabGroupSyncDelegate::CreateScopedLocalObserverPauser() {
  return nullptr;
}

void EmptyTabGroupSyncDelegate::CreateLocalTabGroup(
    const SavedTabGroup& tab_group) {}

void EmptyTabGroupSyncDelegate::CloseLocalTabGroup(
    const LocalTabGroupID& local_id) {}

void EmptyTabGroupSyncDelegate::DisconnectLocalTabGroup(
    const LocalTabGroupID& local_id) {}

void EmptyTabGroupSyncDelegate::UpdateLocalTabGroup(
    const SavedTabGroup& group) {}

std::vector<LocalTabGroupID> EmptyTabGroupSyncDelegate::GetLocalTabGroupIds() {
  return std::vector<LocalTabGroupID>();
}

std::vector<LocalTabID> EmptyTabGroupSyncDelegate::GetLocalTabIdsForTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  return std::vector<LocalTabID>();
}

void EmptyTabGroupSyncDelegate::CreateRemoteTabGroup(
    const LocalTabGroupID& local_tab_group_id) {}

}  // namespace tab_groups
