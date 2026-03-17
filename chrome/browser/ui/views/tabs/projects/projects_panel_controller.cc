// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"

#include <algorithm>

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/base/base_window.h"

ProjectsPanelController::ProjectsPanelController(
    BrowserWindowInterface* browser,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    contextual_tasks::ContextualTasksService* contextual_tasks_service,
    contextual_tasks::ContextualTasksUiService* contextual_tasks_ui_service)
    : browser_(browser),
      tab_group_sync_service_(tab_group_sync_service),
      contextual_tasks_service_(contextual_tasks_service),
      contextual_tasks_ui_service_(contextual_tasks_ui_service) {
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

void ProjectsPanelController::OpenTabGroup(const base::Uuid& group_guid) {
  tab_groups::SavedTabGroupUtils::OpenSavedTabGroup(
      browser_, group_guid, tab_groups::OpeningSource::kOpenedFromProjectsPanel,
      tab_group_sync_service_);
}

void ProjectsPanelController::MoveTabGroup(const base::Uuid& group_guid,
                                           int new_index) {
  if (new_index < 0 || new_index >= static_cast<int>(tab_groups_.size())) {
    return;
  }

  auto it = std::ranges::find(tab_groups_, group_guid,
                              &tab_groups::SavedTabGroup::saved_guid);
  if (it == tab_groups_.end()) {
    return;
  }

  int old_index = std::distance(tab_groups_.begin(), it);
  if (old_index == new_index) {
    return;
  }

  if (new_index < old_index) {
    // Moving up (to a lower index). Place before the group currently at
    // new_index.
    tab_group_sync_service_->ReorderGroupBefore(
        group_guid, tab_groups_[new_index].saved_guid());
  } else {
    // Moving down (to a higher index). Place after the group currently at
    // new_index.
    tab_group_sync_service_->ReorderGroupAfter(
        group_guid, tab_groups_[new_index].saved_guid());
  }
}

const std::vector<contextual_tasks::Thread>&
ProjectsPanelController::GetThreads() {
  return threads_;
}

void ProjectsPanelController::OpenThread(const std::string& thread_server_id) {
  if (!thread_server_id_to_task_id_.contains(thread_server_id)) {
    return;
  }

  const base::Uuid task_id = thread_server_id_to_task_id_[thread_server_id];
  contextual_tasks_ui_service_->GetThreadUrlFromTaskId(
      task_id, base::BindOnce(
                   [](base::WeakPtr<ProjectsPanelController> weak_this,
                      GURL thread_url) {
                     if (!weak_this) {
                       return;
                     }
                     weak_this->OnGotThreadUrlForResumption(thread_url);
                   },
                   weak_ptr_factory_.GetWeakPtr()));
}

void ProjectsPanelController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProjectsPanelController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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
  // When adding a group, we clamp the position between 0 and the size of the
  // tab groups to prevent an accidental out-of-bounds error.
  const int index = std::clamp(static_cast<int>(group.position().value_or(0)),
                               0, static_cast<int>(tab_groups_.size()));
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
        weak_this->thread_server_id_to_task_id_.clear();
        for (auto& task : tasks) {
          if (task.GetThread().has_value()) {
            contextual_tasks::Thread thread = task.GetThread().value();
            weak_this->threads_.push_back(thread);
            weak_this->thread_server_id_to_task_id_.emplace(thread.server_id,
                                                            task.GetTaskId());
          }
        }

        for (auto& observer : weak_this->observers_) {
          observer.OnThreadsInitialized(weak_this->threads_);
        }
      },
      weak_ptr_factory_.GetWeakPtr()));
}

void ProjectsPanelController::OnGotThreadUrlForResumption(GURL thread_url) {
  // TODO(crbug.com/491192199): Open threads in either the side panel or full
  // tab depending on where the last turn was taken. For now, always open the
  // thread in a new tab.
  browser_->OpenGURL(thread_url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
}
