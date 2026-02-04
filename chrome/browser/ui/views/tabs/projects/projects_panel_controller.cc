// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

#include <algorithm>

#include "components/saved_tab_groups/public/saved_tab_group.h"

ProjectsPanelController::ProjectsPanelController(
    tab_groups::TabGroupSyncService* tab_group_sync_service)
    : tab_group_sync_service_(tab_group_sync_service) {
  tab_group_sync_service_observer_.Observe(tab_group_sync_service);
}

ProjectsPanelController::~ProjectsPanelController() = default;

const std::vector<tab_groups::SavedTabGroup>&
ProjectsPanelController::GetTabGroups() {
  return tab_groups_;
}

void ProjectsPanelController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProjectsPanelController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProjectsPanelController::OnInitialized() {
  tab_groups_ = tab_group_sync_service_->GetAllGroups();
  SortTabGroups();

  for (auto& observer : observers_) {
    observer.OnTabGroupsInitialized(tab_groups_);
  }
}

void ProjectsPanelController::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  tab_groups_.push_back(group);
  SortTabGroups();

  auto it = std::ranges::find(tab_groups_, group.saved_guid(),
                              &tab_groups::SavedTabGroup::saved_guid);
  int index = std::distance(tab_groups_.begin(), it);

  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(group, index);
  }
}

void ProjectsPanelController::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  auto existing_group = std::ranges::find(
      tab_groups_, group.saved_guid(), &tab_groups::SavedTabGroup::saved_guid);
  if (existing_group == tab_groups_.end()) {
    return;
  }

  int old_index = std::distance(tab_groups_.begin(), existing_group);
  // If the group's pinned status or position changes, resorting is required.
  bool needs_sorting = existing_group->is_pinned() != group.is_pinned() ||
                       existing_group->position() != group.position();
  *existing_group = group;

  std::optional<int> new_index;
  if (needs_sorting) {
    SortTabGroups();
    auto it = std::ranges::find(tab_groups_, group.saved_guid(),
                                &tab_groups::SavedTabGroup::saved_guid);
    new_index = std::distance(tab_groups_.begin(), it);
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(group, old_index, new_index);
  }
}

void ProjectsPanelController::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  auto existing_group = std::ranges::find(
      tab_groups_, sync_id, &tab_groups::SavedTabGroup::saved_guid);
  if (existing_group == tab_groups_.end()) {
    return;
  }

  int old_index = std::distance(tab_groups_.begin(), existing_group);
  tab_groups_.erase(existing_group);

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(sync_id, old_index);
  }
}

void ProjectsPanelController::OnTabGroupLocalIdChanged(
    const base::Uuid& sync_id,
    const std::optional<tab_groups::LocalTabGroupID>& local_id) {
  auto existing_group = std::ranges::find(
      tab_groups_, sync_id, &tab_groups::SavedTabGroup::saved_guid);
  if (existing_group == tab_groups_.end()) {
    return;
  }

  int index = std::distance(tab_groups_.begin(), existing_group);
  existing_group->SetLocalGroupId(local_id);

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*existing_group, index,
                               /*new_index=*/std::nullopt);
  }
}

void ProjectsPanelController::SortTabGroups() {
  std::stable_sort(tab_groups_.begin(), tab_groups_.end(),
                   [](const tab_groups::SavedTabGroup& left,
                      const tab_groups::SavedTabGroup& right) {
                     // Sort pinned groups first.
                     if (left.is_pinned() != right.is_pinned()) {
                       return left.is_pinned();
                     }
                     if (left.is_pinned()) {
                       return left.position().value() <
                              right.position().value();
                     }
                     // Sort unpinned groups by creation time (newest first).
                     return left.creation_time() > right.creation_time();
                   });
}
