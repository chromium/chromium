// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

#include <algorithm>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"

ProjectsPanelController::ProjectsPanelController(
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    contextual_tasks::ContextualTasksService* contextual_tasks_service)
    : tab_group_sync_service_(tab_group_sync_service),
      contextual_tasks_service_(contextual_tasks_service) {
  tab_group_sync_service_observer_.Observe(tab_group_sync_service);

  if (contextual_tasks_service) {
    contextual_tasks_service_observer_.Observe(contextual_tasks_service);
  }
}

ProjectsPanelController::~ProjectsPanelController() = default;

const std::vector<tab_groups::SavedTabGroup>&
ProjectsPanelController::GetTabGroups() {
  return tab_groups_;
}

void ProjectsPanelController::OpenTabGroup(const base::Uuid& group_guid,
                                           BrowserWindowInterface* browser) {
  tab_groups::SavedTabGroupUtils::OpenSavedTabGroup(
      browser, group_guid, tab_groups::OpeningSource::kOpenedFromProjectsPanel,
      tab_group_sync_service_);
}

void ProjectsPanelController::MoveTabGroup(const base::Uuid& group_guid,
                                           int new_index) {
  tab_group_sync_service_->UpdateGroupPosition(group_guid, std::nullopt,
                                               new_index);
}

void ProjectsPanelController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProjectsPanelController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const std::vector<contextual_tasks::Thread>&
ProjectsPanelController::GetThreads() {
  return threads_;
}

void ProjectsPanelController::OnInitialized() {
  tab_groups_ = tab_group_sync_service_->GetAllGroups();

  for (auto& observer : observers_) {
    observer.OnTabGroupsInitialized(tab_groups_);
  }
}

void ProjectsPanelController::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  const int index =
      std::min(static_cast<int>(tab_groups_.size()),
               static_cast<int>(group.position().value_or(tab_groups_.size())));
  tab_groups_.insert(tab_groups_.begin() + index, group);

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

  *existing_group = group;

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(group);
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

  existing_group->SetLocalGroupId(local_id);

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*existing_group);
  }
}

void ProjectsPanelController::OnTabGroupsReordered(
    tab_groups::TriggerSource source) {
  tab_groups_ = tab_group_sync_service_->GetAllGroups();

  for (auto& observer : observers_) {
    observer.OnTabGroupsReordered(tab_groups_);
  }
}

void ProjectsPanelController::OnContextualTasksServiceInitialized() {
  contextual_tasks_service_->GetTasks(base::BindOnce(
      [](base::WeakPtr<ProjectsPanelController> weak_this,
         std::vector<contextual_tasks::ContextualTask> tasks) {
        if (!weak_this) {
          return;
        }
        weak_this->threads_ = std::vector<contextual_tasks::Thread>();
        for (auto& task : tasks) {
          if (task.GetThread().has_value()) {
            weak_this->threads_.push_back(task.GetThread().value());
          }
        }

        for (auto& observer : weak_this->observers_) {
          observer.OnThreadsInitialized(weak_this->threads_);
        }
      },
      weak_ptr_factory_.GetWeakPtr()));
}
