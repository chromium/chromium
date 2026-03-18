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
class ProjectsPanelStateController;

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
      BrowserWindowInterface* browser,
      ProjectsPanelStateController* state_controller,
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      contextual_tasks::ContextualTasksService* contextual_tasks_service);
  ProjectsPanelController(const ProjectsPanelController&) = delete;
  ProjectsPanelController& operator=(const ProjectsPanelController&) = delete;
  ~ProjectsPanelController() override;

  // Returns all tab groups.
  const std::vector<tab_groups::SavedTabGroup>& GetTabGroups();

  // Opens the tab group.
  void OpenTabGroup(const base::Uuid& group_guid);

  // Moves the tab group to the new index.
  void MoveTabGroup(const base::Uuid& group_guid, int new_index);

  // Returns all threads.
  const std::vector<contextual_tasks::Thread> GetThreads();

  // Opens the thread.
  void OpenThread(const std::string& thread_server_id);

  // Add and remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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
  void OnTaskAdded(
      const contextual_tasks::ContextualTask& task,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;
  void OnTaskUpdated(
      const contextual_tasks::ContextualTask& task,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;
  void OnTaskRemoved(
      const base::Uuid& task_id,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;

 private:
  // Sorts threads from most to least recent last turn time.
  void SortThreads();

  void OnGotThreadUrlForResumption(GURL thread_url);

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<ProjectsPanelStateController> state_controller_;
  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  const raw_ptr<contextual_tasks::ContextualTasksService>
      contextual_tasks_service_;
  std::vector<tab_groups::SavedTabGroup> tab_groups_;
  std::vector<contextual_tasks::Thread> threads_;

  // Maps thread server IDs to task UUIDs for resumption.
  // TODO(crbug.com/489106220): Consider moving this mapping within
  // ContextualTasksService or having a custom type with the thread and task ID
  // for the view.
  std::map<const std::string, const base::Uuid> thread_server_id_to_task_id_;

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
