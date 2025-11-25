// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTabVerticalPadding = 4;
}  // namespace

VerticalPinnedTabContainerView::VerticalPinnedTabContainerView(
    TabCollectionNode* collection_node)
    : collection_node_(collection_node) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
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

  // Child width will be uniform and match the largest child's width.
  int child_width = 0;
  for (auto* child : children) {
    // TODO(corising): look into caching this value and only recomputing if the
    // children change.
    child_width = std::max(child_width, child->GetPreferredSize().width());
  }
  // If the width is bounded, calculate how many children can fit on a row.
  // Since all children are allocated the same width this will be the same for
  // every row.
  if (size_bounds.width().is_bounded()) {
    int available_width =
        size_bounds.width().value() -
        GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING);

    if (available_width > 0) {
      children_on_row = std::floor((available_width - child_width) /
                                   (child_width + kTabVerticalPadding)) +
                        1;

      // Allocate extra space to the tabs.
      available_width -= (children_on_row * child_width) +
                         (kTabVerticalPadding * (children_on_row - 1));
      child_width += std::floor(available_width / children_on_row);
    } else {
      children_on_row = 1;
    }
  }

  int row_index = 0;
  for (auto* child : children) {
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
    bounds.set_width(child_width);
    if (row_index != 0) {
      x += kTabVerticalPadding;
    }
    bounds.set_x(x);
    bounds.set_y(y);
    x += bounds.width();
    total_width = std::max(total_width, x);
    total_height = std::max(total_height, (y + bounds.height()));
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    row_index++;
    if (row_index >= children_on_row) {
      y = total_height + kTabVerticalPadding;
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
