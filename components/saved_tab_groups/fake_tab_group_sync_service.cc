// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/fake_tab_group_sync_service.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"

namespace tab_groups {

FakeTabGroupSyncService::FakeTabGroupSyncService() = default;

FakeTabGroupSyncService::~FakeTabGroupSyncService() = default;

void FakeTabGroupSyncService::AddGroup(SavedTabGroup group) {
  groups_.push_back(group);
}

void FakeTabGroupSyncService::RemoveGroup(const LocalTabGroupID& local_id) {
  std::erase_if(groups_, [local_id](SavedTabGroup& group) {
    return group.local_group_id() == local_id;
  });
}

void FakeTabGroupSyncService::RemoveGroup(const base::Uuid& sync_id) {
  std::erase_if(groups_, [sync_id](SavedTabGroup& group) {
    return group.saved_guid() == sync_id;
  });
}

void FakeTabGroupSyncService::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  std::optional<int> index = GetIndexOf(local_group_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  group.SetColor(visual_data->color());
  group.SetTitle(visual_data->title());
}

void FakeTabGroupSyncService::UpdateGroupPosition(
    const base::Uuid& sync_id,
    std::optional<bool> is_pinned,
    std::optional<int> new_index) {
  std::optional<int> index = GetIndexOf(sync_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  if (is_pinned) {
    group.SetPinned(is_pinned.value());
  }
  if (new_index) {
    group.SetPosition(new_index.value());
  }
}

void FakeTabGroupSyncService::AddTab(const LocalTabGroupID& group_id,
                                     const LocalTabID& tab_id,
                                     const std::u16string& title,
                                     GURL url,
                                     std::optional<size_t> position) {
  std::optional<int> index = GetIndexOf(group_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  SavedTabGroupTab tab(url, title, group.saved_guid(), position);
  group.AddTabLocally(tab);
}

void FakeTabGroupSyncService::UpdateTab(
    const LocalTabGroupID& group_id,
    const LocalTabID& tab_id,
    const SavedTabGroupTabBuilder& tab_builder) {
  std::optional<int> index = GetIndexOf(group_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  for (const auto& tab : group.saved_tabs()) {
    if (tab.local_tab_id() == tab_id) {
      SavedTabGroupTab updated_tab = tab_builder.Build(tab);
      group.UpdateTab(updated_tab);
      return;
    }
  }
}

void FakeTabGroupSyncService::RemoveTab(const LocalTabGroupID& group_id,
                                        const LocalTabID& tab_id) {
  std::optional<int> index = GetIndexOf(group_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  auto tabs = group.saved_tabs();
  std::erase_if(tabs, [tab_id](const SavedTabGroupTab& tab) {
    return tab.local_tab_id() == tab_id;
  });
}

void FakeTabGroupSyncService::MoveTab(const LocalTabGroupID& group_id,
                                      const LocalTabID& tab_id,
                                      int new_group_index) {
  std::optional<int> index = GetIndexOf(group_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  for (const auto& tab : group.saved_tabs()) {
    if (tab.local_tab_id() == tab_id) {
      group.MoveTabLocally(tab.saved_tab_guid(), new_group_index);
      return;
    }
  }
}

void FakeTabGroupSyncService::OnTabSelected(const LocalTabGroupID& group_id,
                                            const LocalTabID& tab_id) {
  // No op.
}

void FakeTabGroupSyncService::MakeTabGroupShared(
    const LocalTabGroupID& local_group_id,
    std::string_view collaboration_id) {
  // No op.
}

std::vector<SavedTabGroup> FakeTabGroupSyncService::GetAllGroups() {
  std::vector<SavedTabGroup> groups;
  for (const SavedTabGroup& group : groups_) {
    groups.push_back(group);
  }
  return groups;
}

std::optional<SavedTabGroup> FakeTabGroupSyncService::GetGroup(
    const base::Uuid& guid) {
  std::optional<int> index = GetIndexOf(guid);
  if (!index.has_value()) {
    return std::nullopt;
  }
  return std::make_optional(groups_[index.value()]);
}

std::optional<SavedTabGroup> FakeTabGroupSyncService::GetGroup(
    const LocalTabGroupID& local_id) {
  std::optional<int> index = GetIndexOf(local_id);
  if (!index.has_value()) {
    return std::nullopt;
  }
  return std::make_optional(groups_[index.value()]);
}

std::vector<LocalTabGroupID> FakeTabGroupSyncService::GetDeletedGroupIds() {
  std::vector<LocalTabGroupID> deleted_group_ids;
  return deleted_group_ids;
}

void FakeTabGroupSyncService::OpenTabGroup(
    const base::Uuid& sync_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  // No op.
}

void FakeTabGroupSyncService::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  std::optional<int> index = GetIndexOf(local_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  group.SetLocalGroupId(local_id);
}

void FakeTabGroupSyncService::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id) {
  std::optional<int> index = GetIndexOf(local_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  group.SetLocalGroupId(std::nullopt);
  for (auto& tab : group.saved_tabs()) {
    tab.SetLocalTabID(std::nullopt);
  }
}

void FakeTabGroupSyncService::UpdateLocalTabId(
    const LocalTabGroupID& local_group_id,
    const base::Uuid& sync_tab_id,
    const LocalTabID& local_tab_id) {
  std::optional<int> index = GetIndexOf(local_group_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  auto tabs = group.saved_tabs();
  for (auto& tab : tabs) {
    if (tab.saved_tab_guid() == sync_tab_id) {
      tab.SetLocalTabID(local_tab_id);
    }
  }
}

void FakeTabGroupSyncService::ConnectLocalTabGroup(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id) {
  std::optional<int> index = GetIndexOf(sync_id);
  if (!index.has_value()) {
    return;
  }
  SavedTabGroup& group = groups_[index.value()];
  group.SetLocalGroupId(local_id);
}

bool FakeTabGroupSyncService::IsRemoteDevice(
    const std::optional<std::string>& cache_guid) const {
  return false;
}

void FakeTabGroupSyncService::RecordTabGroupEvent(
    const EventDetails& event_details) {
  // No op.
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FakeTabGroupSyncService::GetSavedTabGroupControllerDelegate() {
  return base::WeakPtr<syncer::DataTypeControllerDelegate>();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FakeTabGroupSyncService::GetSharedTabGroupControllerDelegate() {
  return base::WeakPtr<syncer::DataTypeControllerDelegate>();
}

std::unique_ptr<ScopedLocalObservationPauser>
FakeTabGroupSyncService::CreateScopedLocalObserverPauser() {
  return std::make_unique<ScopedLocalObservationPauser>();
}

void FakeTabGroupSyncService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeTabGroupSyncService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<int> FakeTabGroupSyncService::GetIndexOf(const base::Uuid& guid) {
  for (size_t i = 0; i < groups_.size(); i++) {
    if (groups_[i].saved_guid() == guid) {
      return i;
    }
  }

  return std::nullopt;
}

std::optional<int> FakeTabGroupSyncService::GetIndexOf(
    const LocalTabGroupID& local_id) {
  for (size_t i = 0; i < groups_.size(); i++) {
    if (groups_[i].local_group_id() == local_id) {
      return i;
    }
  }

  return std::nullopt;
}

}  // namespace tab_groups
