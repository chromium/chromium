// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/views/view.h"

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class ActionViewController;
class Label;
}  // namespace views

class ProjectsPanelNoTabGroupsView;
class ProjectsPanelTabGroupsItemView;

// Contains the Tab Groups inside the Projects Panel.
class ProjectsPanelTabGroupsView : public views::View {
  METADATA_HEADER(ProjectsPanelTabGroupsView, views::View)

 public:
  ProjectsPanelTabGroupsView(
      actions::ActionItem* root_action_item,
      views::ActionViewController* action_view_controller,
      ProjectsPanelTabGroupsItemView::TabGroupPressedCallback
          tab_group_button_callback = base::DoNothing(),
      ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback
          more_button_callback = base::DoNothing());
  ProjectsPanelTabGroupsView(const ProjectsPanelTabGroupsView&) = delete;
  ProjectsPanelTabGroupsView& operator=(const ProjectsPanelTabGroupsView&) =
      delete;
  ~ProjectsPanelTabGroupsView() override;

  // Sets the tab groups shown in the list.
  void SetTabGroups(const std::vector<tab_groups::SavedTabGroup>& tab_groups);

  ProjectsPanelNoTabGroupsView* no_tab_groups_view_for_testing() {
    return no_tab_groups_view_;
  }

 private:
  ProjectsPanelTabGroupsItemView::TabGroupPressedCallback
      tab_group_button_callback_;
  ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback
      more_button_callback_;
  raw_ptr<views::Label> title_ = nullptr;
  std::vector<raw_ptr<ProjectsPanelTabGroupsItemView>> item_views_;
  raw_ptr<ProjectsPanelNoTabGroupsView> no_tab_groups_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_
