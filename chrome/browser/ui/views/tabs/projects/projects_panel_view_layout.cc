// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_view_layout.h"

#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"

ProjectsPanelViewLayout::ProjectsPanelViewLayout(views::View* controls_view)
    : controls_view_(controls_view) {}

ProjectsPanelViewLayout::~ProjectsPanelViewLayout() = default;

views::ProposedLayout ProjectsPanelViewLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;

  // Determine the host size.
  int host_width =
      size_bounds.width().value_or(projects_panel::kProjectsPanelMinWidth);

  int x = projects_panel::kProjectsPanelRegionInteriorMargins.left();
  int y = projects_panel::kProjectsPanelRegionInteriorMargins.top();
  int width = std::max(
      0,
      host_width - projects_panel::kProjectsPanelRegionInteriorMargins.width());

  int controls_height = 0;
  if (controls_view_ && controls_view_->GetVisible()) {
    controls_height = controls_view_->GetPreferredSize().height();
    layout.child_layouts.emplace_back(controls_view_.get(),
                                      controls_view_->GetVisible(),
                                      gfx::Rect(x, y, width, controls_height));
  }

  int total_height =
      y + controls_height +
      projects_panel::kProjectsPanelRegionInteriorMargins.bottom();

  int host_height = size_bounds.height().value_or(total_height);
  layout.host_size = gfx::Size(host_width, host_height);

  return layout;
}
