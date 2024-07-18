// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_service_wrapper.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"

namespace tab_groups {

TabGroupSyncDelegateDesktop::TabGroupSyncDelegateDesktop(
    TabGroupSyncService* service,
    Profile* profile)
    : wrapper_service_(std::make_unique<TabGroupServiceWrapper>(
          service,
          /*saved_tab_group_keyed_service=*/nullptr)),
      listener_(
          std::make_unique<SavedTabGroupModelListener>(wrapper_service_.get(),
                                                       profile)) {}

TabGroupSyncDelegateDesktop::~TabGroupSyncDelegateDesktop() = default;

void TabGroupSyncDelegateDesktop::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  // TODO(b/346871861): Implement.
}

void TabGroupSyncDelegateDesktop::CreateLocalTabGroup(
    const SavedTabGroup& tab_group) {
  // TODO(b/346871861): Implement.
}

void TabGroupSyncDelegateDesktop::CloseLocalTabGroup(
    const LocalTabGroupID& local_id) {
  // TODO(b/346871861): Implement.
}

void TabGroupSyncDelegateDesktop::UpdateLocalTabGroup(
    const SavedTabGroup& group) {
  // TODO(b/346871861): Implement.
}

std::vector<LocalTabGroupID>
TabGroupSyncDelegateDesktop::GetLocalTabGroupIds() {
  // TODO(b/346871861): Implement.
  return std::vector<LocalTabGroupID>();
}

std::vector<LocalTabID> TabGroupSyncDelegateDesktop::GetLocalTabIdsForTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  // TODO(b/346871861): Implement.
  return std::vector<LocalTabID>();
}

void TabGroupSyncDelegateDesktop::CreateRemoteTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  // TODO(b/346871861): Implement.
}

}  // namespace tab_groups
