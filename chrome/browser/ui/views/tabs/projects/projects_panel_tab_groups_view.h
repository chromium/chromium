// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_

#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class ActionViewController;
class Label;
}  // namespace views

class ProjectsPanelController;
class ProjectsPanelTabGroupsItemView;

// Contains the Tab Groups inside the Projects Panel.
class ProjectsPanelTabGroupsView : public views::View,
                                   public ProjectsPanelController::Observer {
  METADATA_HEADER(ProjectsPanelTabGroupsView, views::View)

 public:
  ProjectsPanelTabGroupsView(
      actions::ActionItem* root_action_item,
      views::ActionViewController* action_view_controller,
      ProjectsPanelController* projects_panel_controller);
  ProjectsPanelTabGroupsView(const ProjectsPanelTabGroupsView&) = delete;
  ProjectsPanelTabGroupsView& operator=(const ProjectsPanelTabGroupsView&) =
      delete;
  ~ProjectsPanelTabGroupsView() override;

  // ProjectsPanelController::Observer
  void OnTabGroupsInitialized(
      const std::vector<tab_groups::SavedTabGroup>& tab_groups) override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id) override;

 private:
  raw_ptr<views::Label> title_ = nullptr;
  std::vector<raw_ptr<ProjectsPanelTabGroupsItemView>> item_views_;
  base::ScopedObservation<ProjectsPanelController,
                          ProjectsPanelController::Observer>
      projects_panel_controller_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_
