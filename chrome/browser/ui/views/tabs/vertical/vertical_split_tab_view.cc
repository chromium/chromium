// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"

#include <numeric>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

VerticalSplitTabView::VerticalSplitTabView(TabCollectionNode* collection_node)
    : collection_node_(collection_node) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalSplitTabView::ResetCollectionNode, base::Unretained(this)));
}

VerticalSplitTabView::~VerticalSplitTabView() = default;

views::ProposedLayout VerticalSplitTabView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = 0;

  const auto children = collection_node_->GetDirectChildren();
  CHECK(children.size() == 2);

  // Layout children in order. Children will have their preferred height and
  // fill available width. If unbounded or both children fit on one row they
  // will share it, otherwise they will be stacked vertically.
  if (!size_bounds.width().is_bounded() ||
      size_bounds.width().value() >=
          std::accumulate(children.begin(), children.end(), 0,
                          [](int total, const views::View* view) {
                            return total + view->GetMinimumSize().width();
                          })) {
    int x = 0;
    for (auto* child : children) {
      gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
      bounds.set_x(x);
      // Fill available width if bounded.
      if (size_bounds.width().is_bounded()) {
        bounds.set_width(std::floor(size_bounds.width().value() / 2));
      }
      height = std::max(height, bounds.height());
      x += bounds.width();
      layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    }
    width = x;
  } else {
    int y = 0;
    for (auto* child : children) {
      gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
      bounds.set_y(y);
      bounds.set_width(size_bounds.width().value());
      y += bounds.height();
      layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    }
    width = size_bounds.width().value();
    height = y;
  }
  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

void VerticalSplitTabView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

BEGIN_METADATA(VerticalSplitTabView)
END_METADATA
