// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
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
    : collection_node_(collection_node) {
  SetLayoutManager(std::make_unique<TabCollectionAnimatingLayoutManager>(
      std::make_unique<views::DelegatingLayoutManager>(this)));
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
  int child_width =
      GetLayoutConstant(VERTICAL_TAB_MIN_WIDTH) * (contains_split ? 2 : 1);

  // If the width is bounded, calculate how many children can fit on a row.
  // Since all children are allocated the same width this will be the same for
  // every row.
  if (size_bounds.width().is_bounded() && size_bounds.width().value() > 0) {
    int available_width =
        size_bounds.width().value() -
        GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING);

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
    if (row_index != 0) {
      x += kTabPadding;
    }
    bounds.set_x(x);
    bounds.set_y(y);
    x += bounds.width();
    total_width = std::max(total_width, x);
    total_height = std::max(total_height, (y + bounds.height()));
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    row_index++;
    if (row_index >= children_on_row) {
      y = total_height + kTabPadding;
      row_index = 0;
      x = 0;
    }
  }
  layouts.host_size = gfx::Size(total_width, total_height);
  return layouts;
}

void VerticalPinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

BEGIN_METADATA(VerticalPinnedTabContainerView)
END_METADATA
