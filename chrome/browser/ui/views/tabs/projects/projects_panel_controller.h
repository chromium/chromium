// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLLER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {
class SavedTabGroup;
}

class BrowserWindowInterface;

// Controller for the projects panel view. Handles fetching, resuming, and
// activating tab groups and recent chat threads.
class ProjectsPanelController
    : tab_groups::TabGroupSyncService::Observer,
      contextual_tasks::ContextualTasksService::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTabGroupsInitialized(
        const std::vector<tab_groups::SavedTabGroup>& tab_groups) = 0;
    virtual void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                                 int index) = 0;
    virtual void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group) = 0;
    virtual void OnTabGroupRemoved(const base::Uuid& sync_id,
                                   int old_index) = 0;
    virtual void OnTabGroupsReordered(
        const std::vector<tab_groups::SavedTabGroup>& tab_groups) = 0;
    virtual void OnThreadsInitialized(
        const std::vector<contextual_tasks::Thread>& threads) = 0;
  };

  ProjectsPanelController(
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      contextual_tasks::ContextualTasksService* contextual_tasks_service);
  ProjectsPanelController(const ProjectsPanelController&) = delete;
  ProjectsPanelController& operator=(const ProjectsPanelController&) = delete;
  ~ProjectsPanelController() override;

  // Returns all tab groups.
  const std::vector<tab_groups::SavedTabGroup>& GetTabGroups();

  // Opens the tab group.
  void OpenTabGroup(const base::Uuid& group_guid,
                    BrowserWindowInterface* browser);

  // Moves the tab group to the new index.
  void MoveTabGroup(const base::Uuid& group_guid, int new_index);

  // Add and remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const std::vector<contextual_tasks::Thread>& GetThreads();

  // tab_groups::TabGroupSyncService::Observer:
  void OnInitialized() override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupLocalIdChanged(
      const base::Uuid& sync_id,
      const std::optional<tab_groups::LocalTabGroupID>& local_id) override;
  void OnTabGroupsReordered(tab_groups::TriggerSource source) override;

  // ContextualTasksService::Observer
  void OnContextualTasksServiceInitialized() override;

 private:
  void SortTabGroups();

  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  const raw_ptr<contextual_tasks::ContextualTasksService>
      contextual_tasks_service_;
  std::vector<tab_groups::SavedTabGroup> tab_groups_;
  std::vector<contextual_tasks::Thread> threads_;

  base::ObserverList<Observer> observers_;
  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_service_observer_{this};
  base::ScopedObservation<contextual_tasks::ContextualTasksService,
                          contextual_tasks::ContextualTasksService::Observer>
      contextual_tasks_service_observer_{this};
  base::WeakPtrFactory<ProjectsPanelController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLLER_H_
