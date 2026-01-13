// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_

#include "chrome/browser/ui/views/tabs/projects/projects_panel_controls_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class ActionViewController;
}  // namespace views

class ProjectsPanelStateController;

// Parent view of the Projects Panel - holds together the views
// hierarchy including Tab Groups and AI threads.
class ProjectsPanelView : public views::View {
  METADATA_HEADER(ProjectsPanelView, views::View)

 public:
  explicit ProjectsPanelView(actions::ActionItem* root_action_item);
  ProjectsPanelView(const ProjectsPanelView&) = delete;
  ProjectsPanelView& operator=(const ProjectsPanelView&) = delete;
  ~ProjectsPanelView() override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Called when the projects panel state changes. Updates the visibility and
  // tooltips to match the new state.
  void OnProjectsPanelStateChanged(
      ProjectsPanelStateController* state_controller);

 private:
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<ProjectsPanelControlsView> controls_view_ = nullptr;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
