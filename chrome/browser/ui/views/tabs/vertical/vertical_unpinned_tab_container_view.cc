// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
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
    : collection_node_(collection_node),
      layout_manager_(*SetLayoutManager(
          std::make_unique<TabCollectionAnimatingLayoutManager>(
              std::make_unique<views::DelegatingLayoutManager>(this)))) {
  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&VerticalUnpinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));
}

VerticalUnpinnedTabContainerView::~VerticalUnpinnedTabContainerView() {
  on_will_destroy_callback_list_.Notify();
}

views::ProposedLayout VerticalUnpinnedTabContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = 0;
  int horizontal_padding =
      GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING);

  const auto children = collection_node_->GetDirectChildren();

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    views::SizeBounds child_bounds =
        views::SizeBounds(size_bounds.width().is_bounded()
                              ? (size_bounds.width() - horizontal_padding)
                              : size_bounds.width(),
                          {});
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize(child_bounds));
    bounds.set_y(height);
    // If width is bounded, child views should respect the width constraints and
    // take up the available width excluding trailing horizontal padding.
    if (size_bounds.width().is_bounded()) {
      bounds.set_width(size_bounds.width().value() - horizontal_padding);
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width() + horizontal_padding);
  }
  // Remove excess padding if needed.
  if (!children.empty()) {
    height -= kTabVerticalPadding;
  }

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

void VerticalUnpinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

TabDragContext* VerticalUnpinnedTabContainerView::OnTabDragUpdated(
    TabDragTarget::DragController& drag_controller,
    const gfx::Point& point_in_screen) {
  CHECK(collection_node_);
  VerticalTabStripController* controller = collection_node_->GetController();
  CHECK(controller);

  gfx::Point point_in_container = point_in_screen;
  views::View::ConvertPointFromScreen(this, &point_in_container);

  std::set<views::View*> dragged_views;
  const auto& drag_data = drag_controller.GetSessionData();
  for (TabSlotView* slot_view : drag_data.attached_views()) {
    if (auto* tab_view = slot_view->parent()) {
      dragged_views.insert(tab_view);
    }
  }

  // Hit-test against the target layout instead of the current one to prevent
  // bouncing between targets while mid-animation.
  const views::ProposedLayout& target_layout = CalculateProposedLayout({});
  views::View* view_at_point = nullptr;
  for (const auto& child_layout : target_layout.child_layouts) {
    if (child_layout.visible &&
        !dragged_views.contains(child_layout.child_view) &&
        child_layout.bounds.y() < point_in_container.y() &&
        child_layout.bounds.bottom() > point_in_container.y()) {
      view_at_point = child_layout.child_view;
      break;
    }
  }

  // TODO(crbug.com/439963720): Support dragging over other types.
  if (auto* tab_view = views::AsViewClass<VerticalTabView>(view_at_point)) {
    tab_view->OnTabDragOver();
  } else if (point_in_container.y() >= target_layout.host_size.height()) {
    // If the drag exceeds the bounds all the children, then let the drag
    // handler determine where to put the dragged tab(s) relative to this node.
    controller->GetDragHandler().DraggedTabsOverNode(*collection_node_);
    // TODO(crbug.com/439963720): Consider having a maximum drag coordinate that
    // will cause the dragged tabs to detach. For now, the dragged tab will
    // remain attached as long as it falls in the bounds of this container.
  }

  InvalidateLayout();

  return controller->GetDragHandler().GetDragContext();
}

base::CallbackListSubscription
VerticalUnpinnedTabContainerView::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

bool VerticalUnpinnedTabContainerView::CanDropTab() {
  return true;
}

BEGIN_METADATA(VerticalUnpinnedTabContainerView)
END_METADATA
