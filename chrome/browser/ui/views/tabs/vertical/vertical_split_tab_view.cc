// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"

#include <numeric>
#include <vector>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/border.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
// The minimum width of children tabs which is used to calculate when a split
// tab should line wrap.
constexpr int kVerticalTabMinWidth = 38;
}  // namespace

VerticalSplitTabView::VerticalSplitTabView(TabCollectionNode* collection_node)
    : collection_node_(collection_node) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalSplitTabView::ResetCollectionNode, base::Unretained(this)));
  data_changed_subscription_ =
      collection_node_->RegisterDataChangedCallback(base::BindRepeating(
          &VerticalSplitTabView::OnDataChanged, base::Unretained(this)));

  OnDataChanged();
}

VerticalSplitTabView::~VerticalSplitTabView() = default;

void VerticalSplitTabView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateBorder();
}

void VerticalSplitTabView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalSplitTabView::UpdateBorder, base::Unretained(this)));
}

void VerticalSplitTabView::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

views::ProposedLayout VerticalSplitTabView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = 0;

  const auto children = collection_node_->GetDirectChildren();
  if (children.size() != 2) {
    layouts.host_size = gfx::Size(0, 0);
    return layouts;
  }

  // Layout children in order. Children will have their preferred height and
  // fill available width. If unbounded or both children fit on one row they
  // will share it, otherwise they will be stacked vertically.
  if (!size_bounds.width().is_bounded() ||
      size_bounds.width().value() >=
          static_cast<int>(kVerticalTabMinWidth * children.size())) {
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

void VerticalSplitTabView::OnDataChanged() {
  UpdateBorder();
}

void VerticalSplitTabView::UpdateBorder() {
  const tabs::TabCollection* tab_collection =
      std::get<const tabs::TabCollection*>(collection_node_->GetNodeData());
  const std::vector<tabs::TabInterface*> tabs =
      tab_collection->GetTabsRecursive();
  if (tabs[0]->IsPinned()) {
    const bool is_frame_active =
        GetWidget() ? GetWidget()->ShouldPaintAsActive() : true;
    SetBorder(views::CreateRoundedRectBorder(
        GetLayoutConstant(VERTICAL_TAB_PINNED_BORDER_THICKNESS),
        GetLayoutConstant(VERTICAL_TAB_CORNER_RADIUS),
        is_frame_active ? kColorTabDividerFrameActive
                        : kColorTabDividerFrameInactive));
  } else {
    SetBorder(nullptr);
  }
}

BEGIN_METADATA(VerticalSplitTabView)
END_METADATA
