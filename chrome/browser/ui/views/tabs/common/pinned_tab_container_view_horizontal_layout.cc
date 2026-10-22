// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view_horizontal_layout.h"

#include <algorithm>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"

PinnedTabContainerViewHorizontalLayout::
    PinnedTabContainerViewHorizontalLayout() = default;
PinnedTabContainerViewHorizontalLayout::
    ~PinnedTabContainerViewHorizontalLayout() = default;

gfx::Size PinnedTabContainerViewHorizontalLayout::GetMinimumSize(
    const views::View* host) const {
  if (!PinnedTabContainerView().collection_node_ ||
      PinnedTabContainerView().collection_node_->GetDirectChildren().empty()) {
    return gfx::Size();
  }

  int min_width = 0;
  size_t count = 0;
  for (const auto* child :
       PinnedTabContainerView().collection_node_->GetDirectChildren()) {
    if (!CanBeVisible(child)) {
      continue;
    }
    auto drag_data =
        PinnedTabContainerView().GetVisualDataForDraggedView(*child);
    if (drag_data && drag_data->should_hide) {
      continue;
    }
    min_width += child->GetMinimumSize().width();
    count++;
  }
  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  if (count > 1) {
    min_width =
        std::max(0, min_width - static_cast<int>(count - 1) * tab_overlap);
  }
  return gfx::Size(min_width, TabStyle::Get()->GetStandardHeight());
}

views::ProposedLayout
PinnedTabContainerViewHorizontalLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  const auto children =
      PinnedTabContainerView().collection_node_
          ? PinnedTabContainerView().collection_node_->GetDirectChildren()
          : TabCollectionNode::ChildViews();

  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  int x = 0;
  const int container_height = TabStyle::Get()->GetStandardHeight();
  size_t visible_count = 0;

  for (auto* child : children) {
    views::SizeBounds child_size_bounds =
        views::SizeBounds({}, size_bounds.height());
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(child_size_bounds));
    bounds.set_y(0);
    bounds.set_height(container_height);

    auto drag_data =
        PinnedTabContainerView().GetVisualDataForDraggedView(*child);
    const bool should_show_child =
        CanBeVisible(child) && !(drag_data && drag_data->should_hide);
    if (!should_show_child) {
      layouts.child_layouts.emplace_back(child, false, gfx::Rect());
      continue;
    }

    bounds.set_x(drag_data ? drag_data->offset.x() : x);

    if (!drag_data || !drag_data->should_float) {
      // Advance x position by child width minus tab overlap.
      x += bounds.width() - tab_overlap;
      visible_count++;
    }

    layouts.child_layouts.emplace_back(child, true, bounds);
  }

  const int host_width = visible_count > 0 ? (x + tab_overlap) : 0;
  layouts.host_size = gfx::Size(host_width, container_height);
  return layouts;
}
