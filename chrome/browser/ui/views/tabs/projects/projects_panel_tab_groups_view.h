// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_

#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class ActionViewController;
class Label;
}  // namespace views

// Contains the Tab Groups inside the Projects Panel.
class ProjectsPanelTabGroupsView : public views::View {
  METADATA_HEADER(ProjectsPanelTabGroupsView, views::View)

 public:
  ProjectsPanelTabGroupsView(
      actions::ActionItem* root_action_item,
      views::ActionViewController* action_view_controller);
  ProjectsPanelTabGroupsView(const ProjectsPanelTabGroupsView&) = delete;
  ProjectsPanelTabGroupsView& operator=(const ProjectsPanelTabGroupsView&) =
      delete;
  ~ProjectsPanelTabGroupsView() override;

 private:
  raw_ptr<views::Label> title_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_TAB_GROUPS_VIEW_H_
