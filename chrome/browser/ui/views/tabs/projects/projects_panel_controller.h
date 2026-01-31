// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {
class SavedTabGroup;
}

// Controller for the projects panel view. Handles fetching, resuming, and
// activating tab groups and recent chat threads.
class ProjectsPanelController : tab_groups::TabGroupSyncService::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTabGroupsInitialized(
        const std::vector<tab_groups::SavedTabGroup>& tab_groups) = 0;
    virtual void OnTabGroupAdded(const tab_groups::SavedTabGroup& group) = 0;
    virtual void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group) = 0;
    virtual void OnTabGroupRemoved(const base::Uuid& sync_id) = 0;
  };

  explicit ProjectsPanelController(
      tab_groups::TabGroupSyncService* tab_group_sync_service);
  ProjectsPanelController(const ProjectsPanelController&) = delete;
  ProjectsPanelController& operator=(const ProjectsPanelController&) = delete;
  ~ProjectsPanelController() override;

  // Returns all tab groups.
  const std::vector<tab_groups::SavedTabGroup>& GetTabGroups();

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

 private:
  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  std::vector<tab_groups::SavedTabGroup> tab_groups_;

  base::ObserverList<Observer> observers_;
  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_service_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLLER_H_
