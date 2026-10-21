// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view_horizontal_layout.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_layout_utils.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_closing_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"

UnpinnedTabContainerViewHorizontalLayout::
    UnpinnedTabContainerViewHorizontalLayout() = default;
UnpinnedTabContainerViewHorizontalLayout::
    ~UnpinnedTabContainerViewHorizontalLayout() = default;

int UnpinnedTabContainerViewHorizontalLayout::GetUnconstrainedPreferredWidth()
    const {
  if (!UnpinnedTabContainerView().collection_node_) {
    return 0;
  }
  std::optional<tab_groups::TabGroupId> focused_group_id = GetFocusedGroupId();
  const auto children =
      UnpinnedTabContainerView().collection_node_->GetDirectChildren();
  if (children.empty()) {
    return 0;
  }
  const int container_height = TabStyle::Get()->GetStandardHeight();
  TabStripCollectionLayoutInfo collection = CollectVisibleChildLayoutInfo(
      children, container_height,
      base::BindRepeating(
          &UnpinnedTabContainerViewHorizontalLayout::IsChildVisibleInContainer,
          base::Unretained(this), focused_group_id));
  if (collection.visible_children.empty()) {
    return 0;
  }
  int unconstrained_width = collection.leading_padding +
                            collection.total_preferred_width -
                            collection.overlap_total;
  // If a tab is being dragged, expand the unconstrained preferred width to
  // accommodate the dragged tab's position. This allows TabStripView and parent
  // layouts to allocate space for the drag.
  if (UnpinnedTabContainerView().IsHandlingDrag()) {
    const gfx::Rect dragging_bounds =
        UnpinnedTabContainerView().GetDraggingViewsBounds();
    const int dragged_view_right =
        UnpinnedTabContainerView().GetMirroredXForRect(dragging_bounds) +
        dragging_bounds.width();
    unconstrained_width = std::max(unconstrained_width, dragged_view_right);
  }
  return unconstrained_width;
}

gfx::Size UnpinnedTabContainerViewHorizontalLayout::GetMinimumSize(
    const views::View* host) const {
  if (!UnpinnedTabContainerView().collection_node_ ||
      UnpinnedTabContainerView()
          .collection_node_->GetDirectChildren()
          .empty()) {
    return gfx::Size();
  }

  if (std::optional<int> override_width = GetClosingModeOverrideWidth()) {
    return gfx::Size(*override_width, TabStyle::Get()->GetStandardHeight());
  }

  int min_width = 0;
  std::vector<const views::View*> visible_children;
  std::optional<tab_groups::TabGroupId> focused_group_id = GetFocusedGroupId();

  for (const auto* child :
       UnpinnedTabContainerView().collection_node_->GetDirectChildren()) {
    if (IsChildVisibleInContainer(focused_group_id, child)) {
      min_width += child->GetMinimumSize().width();
      visible_children.push_back(child);
    }
  }
  if (visible_children.empty()) {
    return gfx::Size();
  }
  int overlap_total = 0;
  for (size_t i = 0; i + 1 < visible_children.size(); ++i) {
    overlap_total +=
        GetChildOverlap(visible_children[i], visible_children[i + 1]);
  }
  const int leading_padding = GetLeadingChildPadding(visible_children.front());
  min_width = std::max(0, leading_padding + min_width - overlap_total);

  return gfx::Size(min_width, TabStyle::Get()->GetStandardHeight());
}

views::ProposedLayout
UnpinnedTabContainerViewHorizontalLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!UnpinnedTabContainerView().collection_node_) {
    return layouts;
  }

  std::optional<tab_groups::TabGroupId> focused_group_id = GetFocusedGroupId();

  const auto children =
      UnpinnedTabContainerView().collection_node_->GetDirectChildren();
  if (children.empty()) {
    return layouts;
  }

  const int container_height = TabStyle::Get()->GetStandardHeight();

  TabStripCollectionLayoutInfo collection = CollectVisibleChildLayoutInfo(
      children, container_height,
      base::BindRepeating(
          &UnpinnedTabContainerViewHorizontalLayout::IsChildVisibleInContainer,
          base::Unretained(this), focused_group_id));

  if (collection.visible_children.empty()) {
    return layouts;
  }

  const int net_preferred_width = collection.leading_padding +
                                  collection.total_preferred_width -
                                  collection.overlap_total;
  int available_width = net_preferred_width;
  // If in tab closing mode, constrain the available width to the locked
  // override so remaining tabs do not expand under the cursor.
  if (std::optional<int> override_width = GetClosingModeOverrideWidth()) {
    available_width = std::min(*override_width, available_width);
  }
  if (size_bounds.width().is_bounded()) {
    available_width = std::min(size_bounds.width().value(), available_width);
  }

  const int min_layout_width = collection.leading_padding +
                               collection.total_min_width -
                               collection.overlap_total;
  int computed_width = net_preferred_width;
  if (available_width > 0) {
    computed_width =
        std::clamp(available_width, min_layout_width, net_preferred_width);
  }

  int available_for_allocation =
      std::max(0, computed_width - collection.leading_padding) +
      collection.overlap_total;
  std::vector<int> allocated_widths = CalculateProportionalChildWidths(
      available_for_allocation, collection.preferred_widths,
      collection.crossover_widths, collection.min_widths,
      collection.total_preferred_width, collection.total_crossover_width,
      collection.total_min_width);

  // If a tab is being dragged, expand the host size to accommodate the dragged
  // tab's position, clamped to the available bounded width.
  int dragged_view_right = 0;
  if (UnpinnedTabContainerView().IsHandlingDrag()) {
    const gfx::Rect dragging_bounds =
        UnpinnedTabContainerView().GetDraggingViewsBounds();
    dragged_view_right =
        UnpinnedTabContainerView().GetMirroredXForRect(dragging_bounds) +
        dragging_bounds.width();
    if (size_bounds.width().is_bounded()) {
      dragged_view_right =
          std::min(dragged_view_right, size_bounds.width().value());
    }
  }

  const int host_width = std::max(computed_width, dragged_view_right);
  int x = collection.leading_padding;
  size_t visible_index = 0;

  for (views::View* child : children) {
    auto drag_data =
        UnpinnedTabContainerView().GetVisualDataForDraggedView(*child);
    bool should_show_child = IsChildVisibleInContainer(focused_group_id, child);

    if (!should_show_child) {
      layouts.child_layouts.emplace_back(
          child, false,
          gfx::Rect(drag_data ? drag_data->offset.x() : x, 0, 0,
                    container_height));
      continue;
    }

    int child_width = allocated_widths[visible_index];
    gfx::Rect bounds(drag_data ? drag_data->offset.x() : x, 0, child_width,
                     container_height);

    layouts.child_layouts.emplace_back(child, true, bounds);

    if (visible_index < collection.visible_children.size() - 1) {
      x += bounds.width() -
           GetChildOverlap(collection.visible_children[visible_index].view,
                           collection.visible_children[visible_index + 1].view);
    } else {
      x += bounds.width();
    }
    visible_index++;
  }

  layouts.host_size = gfx::Size(host_width, container_height);
  return layouts;
}

std::optional<int>
UnpinnedTabContainerViewHorizontalLayout::GetClosingModeOverrideWidth() const {
  const TabStripCollectionController* controller =
      UnpinnedTabContainerView().collection_node_
          ? UnpinnedTabContainerView().collection_node_->GetController()
          : nullptr;
  if (controller && controller->tab_closing_helper()) {
    return controller->tab_closing_helper()
        ->override_available_width_for_tabs();
  }
  return std::nullopt;
}
