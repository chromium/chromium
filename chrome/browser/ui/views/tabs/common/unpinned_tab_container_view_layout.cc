// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view_layout.h"

#include <algorithm>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_layout_utils.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_closing_helper.h"
#include "components/tabs/public/tab_group.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kTabVerticalPadding = 2;
}  // namespace

UnpinnedTabContainerViewLayout::UnpinnedTabContainerViewLayout(
    TabStripOrientation orientation)
    : orientation_(orientation) {}
UnpinnedTabContainerViewLayout::~UnpinnedTabContainerViewLayout() = default;

views::ProposedLayout UnpinnedTabContainerViewLayout::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  const UnpinnedTabContainerView* tab_container_view =
      views::AsViewClass<UnpinnedTabContainerView>(host_view());
  if (!tab_container_view) {
    return views::ProposedLayout();
  }

  if (orientation_ == TabStripOrientation::kHorizontal) {
    return CalculateHorizontalLayout(tab_container_view, size_bounds);
  }
  return CalculateVerticalLayout(tab_container_view, size_bounds);
}

gfx::Size UnpinnedTabContainerViewLayout::GetMinimumSize(
    const views::View* host) const {
  const UnpinnedTabContainerView* tab_container_view =
      views::AsViewClass<UnpinnedTabContainerView>(host);
  if (!tab_container_view) {
    return gfx::Size();
  }

  if (orientation_ == TabStripOrientation::kHorizontal) {
    return CalculateHorizontalMinimumSize(tab_container_view);
  }
  return CalculateVerticalMinimumSize(tab_container_view);
}

int UnpinnedTabContainerViewLayout::GetUnconstrainedPreferredWidth(
    const UnpinnedTabContainerView* host) const {
  if (!host->collection_node_) {
    return 0;
  }
  std::optional<tab_groups::TabGroupId> focused_group_id =
      GetFocusedGroupId(host);
  const std::vector<views::View*> children =
      host->collection_node_->GetDirectChildren();
  if (children.empty()) {
    return 0;
  }
  const int container_height = TabStyle::Get()->GetStandardHeight();
  TabStripCollectionLayoutInfo collection = CollectVisibleChildLayoutInfo(
      children, container_height,
      base::BindRepeating(
          &UnpinnedTabContainerViewLayout::IsChildVisibleInContainer,
          base::Unretained(this), host, focused_group_id));
  return collection.total_preferred_width - collection.overlap_total;
}

views::ProposedLayout UnpinnedTabContainerViewLayout::CalculateHorizontalLayout(
    const UnpinnedTabContainerView* tab_container_view,
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!tab_container_view->collection_node_) {
    return layouts;
  }

  std::optional<tab_groups::TabGroupId> focused_group_id =
      GetFocusedGroupId(tab_container_view);

  const std::vector<views::View*> children =
      tab_container_view->collection_node_->GetDirectChildren();
  if (children.empty()) {
    return layouts;
  }

  const int container_height = TabStyle::Get()->GetStandardHeight();

  TabStripCollectionLayoutInfo collection = CollectVisibleChildLayoutInfo(
      children, container_height,
      base::BindRepeating(
          &UnpinnedTabContainerViewLayout::IsChildVisibleInContainer,
          base::Unretained(this), tab_container_view, focused_group_id));

  if (collection.visible_children.empty()) {
    return layouts;
  }

  int available_width =
      collection.total_preferred_width - collection.overlap_total;
  // If in tab closing mode, constrain the available width to the locked
  // override so remaining tabs do not expand under the cursor.
  if (std::optional<int> override_width =
          GetClosingModeOverrideWidth(tab_container_view)) {
    available_width = std::min(*override_width, available_width);
  }
  // Prioritize the container's space override (available viewport/strip
  // capacity) over size bounds which may reflect intermediate layout
  // measurements.
  if (const auto space_override =
          tab_container_view->GetAvailableMainAxisSpaceOverride();
      space_override.has_value() && space_override->is_bounded()) {
    available_width = std::min(space_override->value(), available_width);
  } else if (size_bounds.width().is_bounded()) {
    available_width = std::min(size_bounds.width().value(), available_width);
  }

  int computed_width =
      collection.total_preferred_width - collection.overlap_total;
  if (available_width > 0) {
    computed_width = std::clamp(
        available_width, collection.total_min_width - collection.overlap_total,
        collection.total_preferred_width - collection.overlap_total);
  }

  int available_for_allocation = computed_width + collection.overlap_total;
  std::vector<int> allocated_widths = CalculateProportionalChildWidths(
      available_for_allocation, collection.preferred_widths,
      collection.crossover_widths, collection.min_widths,
      collection.total_preferred_width, collection.total_crossover_width,
      collection.total_min_width);

  int x = 0;
  size_t visible_index = 0;

  for (views::View* child : children) {
    auto drag_data = tab_container_view->GetVisualDataForDraggedView(*child);
    bool should_show_child =
        IsChildVisibleInContainer(tab_container_view, focused_group_id, child);

    if (!should_show_child) {
      layouts.child_layouts.emplace_back(
          child, false,
          gfx::Rect(drag_data ? drag_data->offset.x() : x, 0, 0,
                    container_height));
      continue;
    }

    int child_width = allocated_widths[visible_index];
    int child_x = drag_data ? drag_data->offset.x() : x;
    gfx::Rect bounds(child_x, 0, child_width, container_height);

    layouts.child_layouts.emplace_back(child, true, bounds);

    if (auto* group_view = views::AsViewClass<TabGroupView>(child)) {
      group_view->SetAvailableSpace(views::SizeBound(child_width));
    }

    if (visible_index < collection.visible_children.size() - 1) {
      x += bounds.width() -
           GetChildOverlap(collection.visible_children[visible_index].view,
                           collection.visible_children[visible_index + 1].view);
    } else {
      x += bounds.width();
    }
    visible_index++;
  }

  layouts.host_size = gfx::Size(x, container_height);
  return layouts;
}

views::ProposedLayout UnpinnedTabContainerViewLayout::CalculateVerticalLayout(
    const UnpinnedTabContainerView* tab_container_view,
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!tab_container_view->collection_node_) {
    return layouts;
  }

  std::optional<tab_groups::TabGroupId> focused_group_id =
      GetFocusedGroupId(tab_container_view);

  const std::vector<views::View*> children =
      tab_container_view->collection_node_->GetDirectChildren();

  int width = 0;
  int height = 0;
  int dragged_view_bottom = 0;
  auto collapse_state = tab_container_view->GetTabStripCollapseState();

  const int horizontal_padding =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripHorizontalPadding);

  for (auto* child : children) {
    int x =
        views::AsViewClass<TabGroupView>(child) &&
                collapse_state != tabs::VerticalTabStripCollapseState::kExpanded
            ? 0
            : horizontal_padding;
    views::SizeBound child_height_bound = size_bounds.height();
    // In focused mode, pass the scroll viewport height constraint to the child
    // focused group if the incoming size bounds height is unbounded, so that
    // TabGroupView can clamp dragged view bounds during a drag operation.
    if (focused_group_id.has_value() && !child_height_bound.is_bounded()) {
      if (const auto* scroll_view =
              tab_container_view->GetScrollViewForContainer()) {
        child_height_bound = scroll_view->height();
      }
    }
    views::SizeBounds child_size_bounds =
        views::SizeBounds(size_bounds.width().is_bounded()
                              ? (size_bounds.width() - (x + horizontal_padding))
                              : size_bounds.width(),
                          child_height_bound);
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(child_size_bounds));
    bounds.set_x(x);

    auto drag_data = tab_container_view->GetVisualDataForDraggedView(*child);
    bool should_show_child =
        IsChildVisibleInContainer(tab_container_view, focused_group_id, child);

    if (!should_show_child) {
      layouts.child_layouts.emplace_back(
          child, false,
          gfx::Rect(x, drag_data ? drag_data->offset.y() : height,
                    bounds.width(), 0));
      continue;
    }

    bounds.set_y(drag_data ? drag_data->offset.y() : height);

    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - bounds.x() -
                       horizontal_padding);
    }
    layouts.child_layouts.emplace_back(child, true, bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width() + bounds.x());
  }

  if (height > 0) {
    height -= kTabVerticalPadding;
  }

  if (tab_container_view->IsHandlingDrag()) {
    dragged_view_bottom = tab_container_view->GetDraggingViewsBounds().bottom();
    if (size_bounds.height().is_bounded()) {
      dragged_view_bottom =
          std::min(dragged_view_bottom, size_bounds.height().value());
    }
  }
  layouts.host_size = gfx::Size(width, std::max(height, dragged_view_bottom));
  return layouts;
}

gfx::Size UnpinnedTabContainerViewLayout::CalculateHorizontalMinimumSize(
    const UnpinnedTabContainerView* tab_container_view) const {
  if (!tab_container_view->collection_node_ ||
      tab_container_view->collection_node_->GetDirectChildren().empty()) {
    return gfx::Size();
  }

  if (std::optional<int> override_width =
          GetClosingModeOverrideWidth(tab_container_view)) {
    return gfx::Size(*override_width, TabStyle::Get()->GetStandardHeight());
  }

  int min_width = 0;
  std::vector<const views::View*> visible_children;
  std::optional<tab_groups::TabGroupId> focused_group_id =
      GetFocusedGroupId(tab_container_view);

  for (const auto* child :
       tab_container_view->collection_node_->GetDirectChildren()) {
    if (IsChildVisibleInContainer(tab_container_view, focused_group_id,
                                  child)) {
      min_width += child->GetMinimumSize().width();
      visible_children.push_back(child);
    }
  }
  int overlap_total = 0;
  for (size_t i = 0; i + 1 < visible_children.size(); ++i) {
    overlap_total +=
        GetChildOverlap(visible_children[i], visible_children[i + 1]);
  }
  min_width = std::max(0, min_width - overlap_total);

  return gfx::Size(min_width, TabStyle::Get()->GetStandardHeight());
}

gfx::Size UnpinnedTabContainerViewLayout::CalculateVerticalMinimumSize(
    const UnpinnedTabContainerView* tab_container_view) const {
  if (!tab_container_view->collection_node_) {
    return gfx::Size();
  }

  std::optional<tab_groups::TabGroupId> focused_group_id =
      GetFocusedGroupId(tab_container_view);

  int num_children = 0;
  for (const auto* child :
       tab_container_view->collection_node_->GetDirectChildren()) {
    bool should_show = true;
    if (focused_group_id.has_value()) {
      std::optional<tab_groups::TabGroupId> group_id =
          GetGroupIdForChild(child);
      if (group_id != focused_group_id.value()) {
        should_show = false;
      }
    }
    if (should_show) {
      num_children++;
    }
  }

  const int min_height =
      base::ClampCeil(GetLayoutConstant(LayoutConstant::kVerticalTabHeight) *
                      std::min(1.5f, static_cast<float>(num_children))) +
      (num_children > 1 ? kTabVerticalPadding : 0);
  return gfx::Size(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                   min_height);
}

bool UnpinnedTabContainerViewLayout::IsChildVisibleInContainer(
    const UnpinnedTabContainerView* tab_container_view,
    std::optional<tab_groups::TabGroupId> focused_group_id,
    const views::View* child) const {
  if (!CanBeVisible(child)) {
    return false;
  }
  auto drag_data = tab_container_view->GetVisualDataForDraggedView(*child);
  if (drag_data && drag_data->should_hide) {
    return false;
  }
  if (focused_group_id.has_value() &&
      GetGroupIdForChild(child) != focused_group_id.value()) {
    return false;
  }
  return true;
}

std::optional<tab_groups::TabGroupId>
UnpinnedTabContainerViewLayout::GetFocusedGroupId(
    const UnpinnedTabContainerView* tab_container_view) const {
  if (!tab_container_view->collection_node_ ||
      !tab_container_view->collection_node_->GetController()) {
    return std::nullopt;
  }
  return tab_container_view->collection_node_->GetController()
      ->GetFocusedGroup();
}

std::optional<tab_groups::TabGroupId>
UnpinnedTabContainerViewLayout::GetGroupIdForChild(
    const views::View* child) const {
  if (auto* group_view = views::AsViewClass<TabGroupView>(child)) {
    return group_view->GetTabGroup().id();
  }
  return std::nullopt;
}

std::optional<int> UnpinnedTabContainerViewLayout::GetClosingModeOverrideWidth(
    const UnpinnedTabContainerView* tab_container_view) const {
  const TabStripCollectionController* controller =
      tab_container_view && tab_container_view->collection_node_
          ? tab_container_view->collection_node_->GetController()
          : nullptr;
  if (controller && controller->tab_closing_helper()) {
    return controller->tab_closing_helper()
        ->override_available_width_for_tabs();
  }
  return std::nullopt;
}
