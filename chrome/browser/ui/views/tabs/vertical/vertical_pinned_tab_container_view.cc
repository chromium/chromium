// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTabPadding = 4;
}  // namespace

VerticalPinnedTabContainerView::VerticalPinnedTabContainerView(
    TabCollectionNode* collection_node)
    : VerticalDraggedTabsContainer(static_cast<views::View&>(*this),
                                   collection_node,
                                   DragAxes::kBoth,
                                   DragLayout::kSquash),
      collection_node_(collection_node),
      layout_manager_(*SetLayoutManager(std::make_unique<
                                        TabCollectionAnimatingLayoutManager>(
          std::make_unique<views::DelegatingLayoutManager>(this),
          /*delegate=*/nullptr,
          TabCollectionAnimatingLayoutManager::AnimationAxis::kHorizontal))) {
  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(base::to_address(layout_manager_))));

  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&VerticalPinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));
}

VerticalPinnedTabContainerView::~VerticalPinnedTabContainerView() = default;

views::ProposedLayout VerticalPinnedTabContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int total_width = 0;
  int total_height = 0;

  const auto children = collection_node_->GetDirectChildren();

  int x = 0;
  int y = 0;
  int children_on_row = children.size();

  if (children_on_row == 0) {
    layouts.host_size = gfx::Size(0, 0);
    return layouts;
  }

  // Child width will be uniform and match the largest child's width.
  bool contains_split = false;
  for (const auto& i : collection_node_->children()) {
    if (i->type() == TabCollectionNode::Type::SPLIT) {
      contains_split = true;
    }
  }
  int child_width = GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth) *
                    (contains_split ? 2 : 1);

  // If the width is bounded, calculate how many children can fit on a row.
  // Since all children are allocated the same width this will be the same for
  // every row.
  if (size_bounds.width().is_bounded() && size_bounds.width().value() > 0) {
    bool is_collapsed = IsTabStripCollapsed();
    const int region_horizontal_padding = GetLayoutConstant(
        is_collapsed ? LayoutConstant::kVerticalTabStripCollapsedPadding
                     : LayoutConstant::kVerticalTabStripUncollapsedPadding);
    int available_width =
        size_bounds.width().value() - region_horizontal_padding;

    children_on_row =
        std::min(children_on_row,
                 static_cast<int>(std::floor((available_width - child_width) /
                                             (child_width + kTabPadding)) +
                                  1));

    // Allocate extra space to the tabs.
    available_width -=
        (children_on_row * child_width) + (kTabPadding * (children_on_row - 1));
    child_width += std::floor(available_width / children_on_row);
  }

  int row_index = 0;
  for (auto* child : children) {
    gfx::Rect bounds =
        gfx::Rect(child->GetPreferredSize(views::SizeBounds(child_width, {})));
    bounds.set_width(child_width);

    auto drag_data = GetVisualDataForDraggedView(*child);
    bounds.set_y(drag_data ? drag_data->offset.y() : y);
    bounds.set_x(drag_data ? drag_data->offset.x() : x);

    const bool should_show_child =
        drag_data.has_value() ? !drag_data->should_hide : true;
    if (should_show_child) {
      if (row_index != 0) {
        x += kTabPadding;
        bounds.set_x(bounds.x() + kTabPadding);
      }
      x += bounds.width();
      total_width = std::max(total_width, x);
      total_height = std::max(total_height, (y + bounds.height()));

      row_index++;
      if (row_index >= children_on_row) {
        y = total_height + kTabPadding;
        row_index = 0;
        x = 0;
      }
    }

    layouts.child_layouts.emplace_back(child, should_show_child, bounds);
  }
  layouts.host_size = gfx::Size(total_width, total_height);
  return layouts;
}

gfx::Size VerticalPinnedTabContainerView::GetMinimumSize() const {
  // The minimum size should be enough to show a row and a half, if needed.
  const int num_children = collection_node_->GetDirectChildren().size();
  const float min_rows = std::min((IsTabStripCollapsed() ? 1.5f : 1.0f),
                                  static_cast<float>(num_children));
  const int min_height =
      base::ClampCeil(GetLayoutConstant(LayoutConstant::kVerticalTabHeight) *
                      min_rows) +
      (num_children > 1 ? kTabPadding : 0);
  return gfx::Size(GetLayoutConstant(LayoutConstant::kVerticalTabMinWidth),
                   min_height);
}

void VerticalPinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

bool VerticalPinnedTabContainerView::IsTabStripCollapsed() const {
  const auto* controller =
      collection_node_ ? collection_node_->GetController() : nullptr;
  return controller && controller->IsCollapsed();
}

views::ScrollView* VerticalPinnedTabContainerView::GetScrollViewForContainer()
    const {
  return views::ScrollView::GetScrollViewForContents(
      const_cast<VerticalPinnedTabContainerView*>(this));
}

void VerticalPinnedTabContainerView::UpdateLayoutForDrag() {
  layout_manager_->ResetToTargetLayout();
}

const views::ProposedLayout& VerticalPinnedTabContainerView::GetLayoutForDrag()
    const {
  return layout_manager_->target_layout();
}

void VerticalPinnedTabContainerView::HandleTabDragInContainer(
    const gfx::Rect& dragged_tab_bounds) {
  const views::ProposedLayout& target_layout = layout_manager_->target_layout();
  views::View* view_at_point =
      GetViewForDragBounds(target_layout, dragged_tab_bounds);
  const TabCollectionNode* node = nullptr;
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(view_at_point)) {
    node = tab_view->collection_node();
  } else if (auto* split_tab_view =
                 views::AsViewClass<VerticalSplitTabView>(view_at_point)) {
    node = split_tab_view->collection_node();
  }
  if (node) {
    GetDragHandler().HandleDraggedTabsOverNode(*node, std::nullopt);
    // Synchronously force a layout here to update the target layout. Since all
    // the calculations are based off on target layout, we need to ensure it is
    // updated where there are model change.
    DeprecatedLayoutImmediately();
  }
}

BEGIN_METADATA(VerticalPinnedTabContainerView)
END_METADATA
