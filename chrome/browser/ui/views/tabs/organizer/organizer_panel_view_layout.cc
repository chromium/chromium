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

  int total_height =
      y + controls_height +
      organizer_panel::kOrganizerPanelRegionInteriorMargins.bottom();

  int host_height = size_bounds.height().value_or(total_height);
  layout.host_size = gfx::Size(host_width, host_height);

  return layout;
}
