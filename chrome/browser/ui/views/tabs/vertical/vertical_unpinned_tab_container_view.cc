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

int GetYForDraggedTab(const views::View& view, const gfx::Point& point) {
  return std::max(0.0f, point.y() - (view.height() * 0.5f));
}

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

    const bool is_child_dragging = dragging_views_.contains(child);
    if (is_child_dragging) {
      if (child->GetTransform().IsIdentity()) {
        // If a drag recently ended, the child will still be in
        // `dragging_views_` but will not have a transformation, which let's
        // this animate the tab view into its correct slot.
        bounds.set_y(GetYForDraggedTab(*child, last_drag_point_));
      } else {
        // If the tab is being dragged, then it is rendered using
        // transformations, offset from the container's origin.
        bounds.set_y(0);
      }
    } else {
      bounds.set_y(height);
    }
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

  // Used to determine whether the layout should snap into position without
  // animating at the end of this drag cycle.
  bool is_initial_drag = false;
  if (!TabDragController::IsAttachedTo(
          controller->GetDragHandler().GetDragContext())) {
    // Do nothing until the drag attaches to this window's context.
    return controller->GetDragHandler().GetDragContext();
  } else if (dragging_views_.empty()) {
    is_initial_drag = true;
    InitializeDragState(drag_controller);
  }

  gfx::Point point_in_container = point_in_screen;
  views::View::ConvertPointFromScreen(this, &point_in_container);
  last_drag_point_ = point_in_container;
  for (views::View* tab_view : dragging_views_) {
    // Use a transformation to render the dragged views, offset from the
    // container's origin.
    gfx::Transform transform;
    transform.Translate(0, GetYForDraggedTab(*tab_view, point_in_container));
    tab_view->SetTransform(transform);
    tab_view->SetClipPath(tab_view->clip_path());
  }

  // Hit-test against the target layout instead of the current one to prevent
  // bouncing between targets while mid-animation.
  views::View* view_at_point = nullptr;
  const views::ProposedLayout& target_layout = CalculateProposedLayout({});
  for (const auto& child_layout : target_layout.child_layouts) {
    if (child_layout.visible &&
        !dragging_views_.contains(child_layout.child_view) &&
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

  if (is_initial_drag) {
    // This is needed because when the drag first attaches to the tab strip, the
    // tab is inserted at an arbitrary index. Letting the first iteration of the
    // drag loop run first ensures that the we only snap to the target layout
    // *after* the correct tab strip model updates are performed.
    layout_manager_->ResetToTargetLayout();
  } else {
    InvalidateLayout();
  }

  return controller->GetDragHandler().GetDragContext();
}

void VerticalUnpinnedTabContainerView::OnTabDragExited() {
  ResetDragState();
}

void VerticalUnpinnedTabContainerView::OnTabDragEnded() {
  ResetDragState();
}

void VerticalUnpinnedTabContainerView::InitializeDragState(
    TabDragTarget::DragController& controller) {
  // Move each dragged tab to the origin position. Transformations will be used
  // to render them during the drag.
  for (TabSlotView* slot_view : controller.GetSessionData().attached_views()) {
    auto* tab_view = VerticalTabDragHandler::ViewFromTabSlot(slot_view);
    CHECK(tab_view);
    dragging_views_.insert(tab_view);
  }
}

void VerticalUnpinnedTabContainerView::ResetDragState() {
  for (auto view : dragging_views_) {
    view->SetTransform(gfx::Transform());
    view->SetClipPath(view->clip_path());
  }
  layout_manager_->ResetToTargetLayout();
  dragging_views_.clear();
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
