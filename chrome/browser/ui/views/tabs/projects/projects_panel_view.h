// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_

#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class ActionViewController;
class LabelButton;
}  // namespace views

class ProjectsPanelStateController;

// The container view for the Projects Panel. This view overlays the Vertical
// Tab Strip region and is responsible for laying out the panel's exit button.
class ProjectsPanelView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(ProjectsPanelView, views::View)

 public:
  explicit ProjectsPanelView(actions::ActionItem* root_action_item);
  ProjectsPanelView(const ProjectsPanelView&) = delete;
  ProjectsPanelView& operator=(const ProjectsPanelView&) = delete;
  ~ProjectsPanelView() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // Creates a TopContainerButton based on an ActionId.
  views::LabelButton* AddChildButtonFor(actions::ActionId action_id);

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Called when the projects panel state changes. Updates the visibility and
  // tooltips to match the new state.
  void OnProjectsPanelStateChanged(
      ProjectsPanelStateController* state_controller);

 private:
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  raw_ptr<views::LabelButton> projects_button_ = nullptr;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_VIEW_H_
