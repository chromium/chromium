// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

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
  // Sort groups from newest to oldest creation time
  std::sort(tab_groups_.begin(), tab_groups_.end(),
            [](const tab_groups::SavedTabGroup& left,
               const tab_groups::SavedTabGroup& right) {
              return left.creation_time() > right.creation_time();
            });
  for (auto& observer : observers_) {
    observer.OnTabGroupsInitialized(tab_groups_);
  }
}

void ProjectsPanelController::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  auto insert_pos = tab_groups_.begin();
  if (source != tab_groups::TriggerSource::LOCAL) {
    // The list of tab groups is sorted from newest to oldest creation time.
    // Find the correct insertion point to maintain sort order.
    insert_pos = std::ranges::find_if(
        tab_groups_, [&group](const tab_groups::SavedTabGroup& existing_group) {
          return existing_group.creation_time() < group.creation_time();
        });
  }
  tab_groups_.insert(insert_pos, group);

  for (auto& observer : observers_) {
    observer.OnTabGroupAdded(group);
  }
}

void ProjectsPanelController::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  auto existing_group = std::ranges::find(
      tab_groups_, group.saved_guid(), &tab_groups::SavedTabGroup::saved_guid);
  if (existing_group != tab_groups_.end()) {
    *existing_group = group;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(group);
  }
}

void ProjectsPanelController::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  std::erase_if(tab_groups_, [sync_id](const tab_groups::SavedTabGroup& group) {
    return group.saved_guid() == sync_id;
  });

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(sync_id);
  }
}
