// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTabVerticalPadding = 2;
}  // namespace

VerticalUnpinnedTabContainerView::VerticalUnpinnedTabContainerView(
    TabCollectionNode* collection_node)
    : VerticalDraggedTabsContainer(static_cast<views::View&>(*this)),
      collection_node_(collection_node),
      layout_manager_(*SetLayoutManager(
          std::make_unique<TabCollectionAnimatingLayoutManager>(
              std::make_unique<views::DelegatingLayoutManager>(this),
              this))) {
  collection_node->set_remove_child_from_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView,
      base::Unretained(&layout_manager_.get())));
  collection_node->set_attach_child_to_node(base::BindRepeating(
      &TabCollectionAnimatingLayoutManager::AnimateAndReparentView,
      base::Unretained(&layout_manager_.get())));

  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&VerticalUnpinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));
}

VerticalUnpinnedTabContainerView::~VerticalUnpinnedTabContainerView() = default;

views::ProposedLayout VerticalUnpinnedTabContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = 0;
  auto* controller = collection_node_->GetController();
  bool is_collapsed = controller && controller->IsCollapsed();

  const int horizontal_padding = GetLayoutConstant(
      is_collapsed ? LayoutConstant::kVerticalTabStripCollapsedPadding
                   : LayoutConstant::kVerticalTabStripUncollapsedPadding);
  const auto children = collection_node_->GetDirectChildren();

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    // The leading inset should not be applied for tab groups when the tab strip
    // is collapsed since the group color line is drawn in that space.
    int x = views::AsViewClass<VerticalTabGroupView>(child) && is_collapsed
                ? 0
                : horizontal_padding;
    views::SizeBounds child_bounds =
        views::SizeBounds(size_bounds.width().is_bounded()
                              ? (size_bounds.width() - (x + horizontal_padding))
                              : size_bounds.width(),
                          {});
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(child_bounds));
    bounds.set_x(x);

    bounds.set_y(GetYForDraggedTabBounds(*child).value_or(height));

    // If width is bounded, child views should respect the width constraints and
    // take up the available width excluding trailing horizontal padding.
    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - bounds.x() -
                       horizontal_padding);
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width() + bounds.x());
  }
  // Remove excess padding if needed.
  if (!children.empty()) {
    height -= kTabVerticalPadding;
  }

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

bool VerticalUnpinnedTabContainerView::IsViewDragging(
    const views::View& child_view) const {
  return GetDragHandler().IsViewDragging(child_view);
}

void VerticalUnpinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

VerticalTabDragHandler& VerticalUnpinnedTabContainerView::GetDragHandler() {
  CHECK(collection_node_);
  CHECK(collection_node_->GetController());
  return collection_node_->GetController()->GetDragHandler();
}

const VerticalTabDragHandler& VerticalUnpinnedTabContainerView::GetDragHandler()
    const {
  CHECK(collection_node_);
  CHECK(collection_node_->GetController());
  return collection_node_->GetController()->GetDragHandler();
}

void VerticalUnpinnedTabContainerView::UpdateLayoutForDrag() {
  layout_manager_->ResetToTargetLayout();
}

void VerticalUnpinnedTabContainerView::HandleTabDragInContainer(
    const gfx::Point point_in_container) {
  const views::ProposedLayout& target_layout = layout_manager_->target_layout();
  views::View* view_at_point =
      GetViewAtPoint(target_layout, point_in_container);
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(view_at_point)) {
    tab_view->OnTabDragOver();
  } else if (auto* group_view =
                 views::AsViewClass<VerticalTabGroupView>(view_at_point)) {
    group_view->OnTabDragOver();
  } else if (point_in_container.y() >= target_layout.host_size.height()) {
    // If the drag exceeds the bounds all the children, then let the drag
    // handler determine where to put the dragged tab(s) relative to this node.
    GetDragHandler().DraggedTabsOverNode(*collection_node_);
    // TODO(crbug.com/439963720): Consider having a maximum drag coordinate that
    // will cause the dragged tabs to detach. For now, the dragged tab will
    // remain attached as long as it falls in the bounds of this container.
  }
}

BEGIN_METADATA(VerticalUnpinnedTabContainerView)
END_METADATA
