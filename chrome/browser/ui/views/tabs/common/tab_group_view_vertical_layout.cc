// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_view_vertical_layout.h"

#include <algorithm>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_line_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_unfocus_button.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"

namespace {
constexpr int kTabVerticalPadding = 2;
constexpr int kGroupLineWidth = 2;
constexpr int kGroupLineCollapsedLeadingPadding = 6;
constexpr int kGroupHeaderHeight = 26;
constexpr int kGroupHeaderVerticalMargin = 4;
constexpr int kFocusedHeadersGap = 2;
}  // namespace

TabGroupViewVerticalLayout::TabGroupViewVerticalLayout() = default;
TabGroupViewVerticalLayout::~TabGroupViewVerticalLayout() = default;

gfx::Size TabGroupViewVerticalLayout::GetMinimumSize(
    const views::View* host) const {
  return CalculateProposedLayout(views::SizeBounds(0, 0)).host_size;
}

views::ProposedLayout TabGroupViewVerticalLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  const float focus_progress = TabGroupView().GetFocusProgress();
  // Ephemeral groups collapse their header, and the spacing around it, to zero
  // as they dissolve back into ungrouped tabs.
  const auto scale_for_ephemeral = [&](int value) {
    return TabGroupView().is_ephemeral()
               ? gfx::Tween::LinearIntValueBetween(focus_progress, 0, value)
               : value;
  };
  int width = 0;
  int height = scale_for_ephemeral(kGroupHeaderVerticalMargin);
  auto tab_strip_collapse_state = TabGroupView().GetTabStripCollapseState();

  gfx::Rect header_bounds;
  gfx::Rect group_line_bounds;
  group_line_bounds.set_width(kGroupLineWidth);

  // If the tab strip is collapsed then the group line should appear on the
  // leading side of all grouped tabs and the header.
  if (tab_strip_collapse_state !=
      tabs::VerticalTabStripCollapseState::kExpanded) {
    group_line_bounds.set_x(kGroupLineCollapsedLeadingPadding);
    group_line_bounds.set_y(height);
    header_bounds.set_x(
        GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding));
  }

  if (TabGroupView().unfocus_header_) {
    // The Unfocus button grows in above the header as the group gains focus
    // and shrinks away as it loses focus.
    gfx::Rect unfocus_bounds;
    unfocus_bounds.set_x(header_bounds.x());
    unfocus_bounds.set_y(height);
    unfocus_bounds.set_height(gfx::Tween::LinearIntValueBetween(
        focus_progress, 0, kGroupHeaderHeight));
    if (size_bounds.width().is_bounded()) {
      unfocus_bounds.set_width(size_bounds.width().value() -
                               unfocus_bounds.x());
    }
    const bool unfocus_visible = TabGroupView().ShouldShowUnfocusButton();
    layouts.child_layouts.emplace_back(
        TabGroupView().unfocus_header_.get(), unfocus_visible,
        unfocus_visible ? unfocus_bounds : gfx::Rect());
    if (unfocus_visible) {
      height +=
          unfocus_bounds.height() + gfx::Tween::LinearIntValueBetween(
                                        focus_progress, 0, kFocusedHeadersGap);
      width = std::max(width, unfocus_bounds.width());
    }
  }

  header_bounds.set_y(height);
  header_bounds.set_height(scale_for_ephemeral(kGroupHeaderHeight));
  // If width is bounded, the group header should respect the width constraints
  // and take up the available width excluding trailing horizontal padding.
  if (size_bounds.width().is_bounded()) {
    header_bounds.set_width(size_bounds.width().value() - header_bounds.x());
  }

  const bool header_visible = header_bounds.height() > 0;
  layouts.child_layouts.emplace_back(
      TabGroupView().group_header_.get(), header_visible,
      header_visible ? header_bounds : gfx::Rect());

  if (header_visible) {
    height +=
        header_bounds.height() +
        scale_for_ephemeral(kGroupHeaderVerticalMargin + kTabVerticalPadding);
    width = std::max(width, header_bounds.width() + header_bounds.x());
  }

  // When expanded, grouped tabs are inset by `kTabLeadingPadding` in the
  // unfocused state and slide to `x = 0` as the group gains focus; the group
  // line keeps its constant offset to the left of the tabs so it slides out
  // with them rather than fading over their leading edge.
  const int base_child_x =
      tab_strip_collapse_state != tabs::VerticalTabStripCollapseState::kExpanded
          ? GetLayoutConstant(
                LayoutConstant::kVerticalTabStripHorizontalPadding)
          : (TabGroupView().is_ephemeral()
                 ? 0
                 : gfx::Tween::LinearIntValueBetween(
                       focus_progress, TabGroupView::kTabLeadingPadding, 0));
  const int child_x = base_child_x + TabGroupView().GetFocusSwitchSlideOffset();

  if (tab_strip_collapse_state ==
      tabs::VerticalTabStripCollapseState::kExpanded) {
    group_line_bounds.set_x(
        base_child_x -
        (TabGroupView::kTabLeadingPadding + kGroupLineWidth) / 2);
    group_line_bounds.set_y(height);
  }

  const auto children =
      TabGroupView().collection_node_
          ? TabGroupView().collection_node_->GetDirectChildren()
          : TabCollectionNode::ChildViews();

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(size_bounds));

    auto drag_data = TabGroupView().GetVisualDataForDraggedView(*child);
    CHECK(!drag_data || !drag_data->should_hide);
    bounds.set_y(drag_data ? drag_data->offset.y() : height);
    bounds.set_x(child_x);
    // If width is bounded, child views should respect the width constraints
    // and take up the available width excluding trailing horizontal padding.
    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - base_child_x);
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width() + base_child_x);
  }
  // Remove excess padding.
  height -= kTabVerticalPadding;

  if (!children.empty()) {
    group_line_bounds.set_height(height - group_line_bounds.y());
  }
  const bool show_group_line = TabGroupView().ShouldShowGroupLine();
  if (TabGroupView().group_line_) {
    layouts.child_layouts.emplace_back(TabGroupView().group_line_.get(),
                                       show_group_line, group_line_bounds);
  }

  // Add extra padding below the group if not collapsed.
  const bool is_group_collapsed = TabGroupView().IsCollapsed();
  if (!is_group_collapsed) {
    height += scale_for_ephemeral(kTabVerticalPadding);
  }

  // If the group is focused and we are handling a drag, we need to account for
  // the dragged views' bottom bound.
  int dragged_view_bottom = 0;
  if (TabGroupView().IsGroupFocused() && TabGroupView().IsHandlingDrag()) {
    dragged_view_bottom = TabGroupView().GetDraggingViewsBounds().bottom();
    if (size_bounds.height().is_bounded()) {
      dragged_view_bottom =
          std::min(dragged_view_bottom, size_bounds.height().value());
    }
  }

  layouts.host_size = gfx::Size(
      width, is_group_collapsed
                 ? header_bounds.height() + (2 * kGroupHeaderVerticalMargin)
                 : std::max(height, dragged_view_bottom));
  return layouts;
}
