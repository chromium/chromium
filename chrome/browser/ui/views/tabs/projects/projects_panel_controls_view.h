// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLS_VIEW_H_

#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class LabelButton;
}  // namespace views

// Contains the controls for the projects panel, including the
// button to close the panel.
class ProjectsPanelControlsView : public views::View,
                                  public views::LayoutDelegate {
  METADATA_HEADER(ProjectsPanelControlsView, views::View)

 public:
  explicit ProjectsPanelControlsView(actions::ActionItem* root_action_item);
  ProjectsPanelControlsView(const ProjectsPanelControlsView&) = delete;
  ProjectsPanelControlsView& operator=(const ProjectsPanelControlsView&) =
      delete;
  ~ProjectsPanelControlsView() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Should be called when the text of the action item for toggling the panel
  // state changes.
  void UpdateTooltipText();

  // Sets the opacity of any buttons in this view.
  void SetButtonOpacity(float opacity);

  views::LabelButton* projects_button_for_testing() { return projects_button_; }

 private:
  void OnCloseButtonPressed();

  raw_ptr<views::LabelButton> projects_button_ = nullptr;
  raw_ptr<actions::ActionItem> toggle_projects_panel_action_item_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_PROJECTS_PANEL_CONTROLS_VIEW_H_
