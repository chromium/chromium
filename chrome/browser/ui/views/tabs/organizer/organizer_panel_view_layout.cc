// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view_layout.h"

#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "ui/views/view.h"

OrganizerPanelViewLayout::OrganizerPanelViewLayout(views::View* controls_view)
    : controls_view_(controls_view) {}

OrganizerPanelViewLayout::~OrganizerPanelViewLayout() = default;

views::ProposedLayout OrganizerPanelViewLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layout;

  // Determine the host size.
  int host_width =
      size_bounds.width().value_or(organizer_panel::kOrganizerPanelMinWidth);

  int x = organizer_panel::kOrganizerPanelRegionInteriorMargins.left();
  int y = organizer_panel::kOrganizerPanelRegionInteriorMargins.top();
  int width = std::max(
      0, host_width -
             organizer_panel::kOrganizerPanelRegionInteriorMargins.width());

  int controls_height = 0;
  if (controls_view_ && controls_view_->GetVisible()) {
    controls_height = controls_view_->GetPreferredSize().height();
    layout.child_layouts.emplace_back(controls_view_.get(),
                                      controls_view_->GetVisible(),
                                      gfx::Rect(x, y, width, controls_height));
  }

  int current_y = y + controls_height;

  if (host_view()) {
    for (views::View* child : host_view()->children()) {
      if (child == controls_view_) {
        continue;
      }
      if (!child->GetVisible()) {
        continue;
      }
      int child_height =
          child->GetPreferredSize(views::SizeBounds(width, {})).height();
      if (size_bounds.height().is_bounded()) {
        int remaining =
            size_bounds.height().value() - current_y -
            organizer_panel::kOrganizerPanelRegionInteriorMargins.bottom();
        if (remaining > 0) {
          child_height = remaining;
        }
      }
      layout.child_layouts.emplace_back(
          child, child->GetVisible(),
          gfx::Rect(x, current_y, width, child_height));
      current_y += child_height;
    }
  }

  int total_height =
      current_y +
      organizer_panel::kOrganizerPanelRegionInteriorMargins.bottom();

  int host_height = size_bounds.height().value_or(total_height);
  layout.host_size = gfx::Size(host_width, host_height);

  return layout;
}
