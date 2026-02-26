// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_dragged_tabs_container.h"

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/dragging/drag_session_data.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_target.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

// Calculates the offset of the source dragged view (i.e. the main view being
// dragged) from the mouse.
gfx::Vector2d GetSourceViewOffsetFromMouse(
    views::View& source_dragged_view,
    const DragSessionData& session_data) {
  views::View* source_slot_view =
      session_data.source_view_drag_data()->attached_view;

  // The view that initiated the drag may not be the same as the view that
  // is being dragged (e.g. dragging a tab group header).
  gfx::Vector2d slot_view_offset_to_source =
      views::View::ConvertPointToTarget(source_slot_view, &source_dragged_view,
                                        source_slot_view->bounds().origin())
          .OffsetFromOrigin();
  gfx::Vector2d dragged_view_bounds_offset_from_bounds;
  dragged_view_bounds_offset_from_bounds -= slot_view_offset_to_source;
  dragged_view_bounds_offset_from_bounds -=
      {static_cast<int>(session_data.mouse_offset_to_size_ratios.x() *
                        source_slot_view->width()),
       static_cast<int>(session_data.mouse_offset_to_size_ratios.y() *
                        source_slot_view->height())};

  return dragged_view_bounds_offset_from_bounds;
}
}  // namespace

VerticalDraggedTabsContainer::VerticalDraggedTabsContainer(
    views::View& host_view,
    TabCollectionNode* collection_node,
    DragAxes drag_axes,
    DragLayout drag_layout)
    : host_view_(host_view),
      collection_node_(collection_node),
      drag_start_animation_(this),
      drag_axes_(drag_axes),
      drag_layout_(drag_layout) {
  host_view_observation_.Observe(&host_view);
  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&VerticalDraggedTabsContainer::ResetCollectionNode,
                     base::Unretained(this)));
}

VerticalDraggedTabsContainer::~VerticalDraggedTabsContainer() {
  on_will_destroy_callback_list_.Notify();
}

VerticalDraggedTabsContainer& VerticalDraggedTabsContainer::GetTabDragTarget(
    const gfx::Point& point_in_screen) {
  return *this;
}

TabDragContext* VerticalDraggedTabsContainer::OnTabDragUpdated(
    TabDragTarget::DragController& drag_controller,
    const gfx::Point& point_in_screen) {
  last_drag_point_in_screen_ = point_in_screen;
  if (drag_controller.GetAttachedContext() !=
      GetDragHandler().GetDragContext()) {
    // Do nothing until the drag attaches to this window's context.
    return GetDragHandler().GetDragContext();
  }

  // Used to determine whether the layout should snap into position without
  // animating at the end of this drag cycle.
  if (dragging_views_.empty()) {
    HandleTabDragEnteredContainer();
    InitializeDragState(drag_controller);
  }

  ApplyUpdatesForDragPositionChange();

  return GetDragHandler().GetDragContext();
}

void VerticalDraggedTabsContainer::ApplyUpdatesForDragPositionChange() {
  gfx::Point point_in_container = views::View::ConvertPointFromScreen(
      base::to_address(host_view_), last_drag_point_in_screen_);

  gfx::Rect dragged_bounds_in_container =
      GetDraggingViewsBoundsAtPoint(point_in_container);

  auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  scroll_handler_.OnDraggedTabPositionUpdated(
      *scroll_view, views::View::ConvertRectToTarget(
                        base::to_address(host_view_), scroll_view,
                        dragged_bounds_in_container));

  HandleTabDragInContainer(dragged_bounds_in_container);

  UpdateDraggingViewTransforms(point_in_container);
}

void VerticalDraggedTabsContainer::OnTabDragExited(
    const gfx::Point& point_in_screen) {
  ResetDragState();
  scroll_handler_.StopScrolling();
}

void VerticalDraggedTabsContainer::OnTabDragEnded() {
  ResetDragState();
  scroll_handler_.StopScrolling();
}

bool VerticalDraggedTabsContainer::CanDropTab() {
  return true;
}

void VerticalDraggedTabsContainer::HandleTabDragEnteredContainer() {
  CHECK(collection_node_);
  GetDragHandler().HandleDraggedTabsIntoNode(*collection_node_);

  // We don't need to snap any views here, since the layout manager already
  // skips animating dragged views.
  // The target layout just needs to be recalculated so that nodes added to this
  // container will haver their views tracked in the layout manager.
  UpdateTargetLayoutForDrag({});
}

base::CallbackListSubscription
VerticalDraggedTabsContainer::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

void VerticalDraggedTabsContainer::OnViewBoundsChanged(
    views::View* observed_view) {
  CHECK_EQ(observed_view, base::to_address(host_view_));
  // The transformation coordinates are relative to the host view's coordinates,
  // so they must be updated as the bounds change to ensure the dragged tabs
  // remain at the same point in the screen.
  if (IsHandlingDrag()) {
    UpdateDraggingViewTransforms(views::View::ConvertPointFromScreen(
        base::to_address(host_view_), last_drag_point_in_screen_));
  }
}

void VerticalDraggedTabsContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  CHECK_EQ(animation, &drag_start_animation_);
  UpdateDraggingViewTransforms(views::View::ConvertPointFromScreen(
      base::to_address(host_view_), last_drag_point_in_screen_));
}

void VerticalDraggedTabsContainer::AnimationEnded(
    const gfx::Animation* animation) {
  CHECK_EQ(animation, &drag_start_animation_);
  host_view_->InvalidateLayout();
}

void VerticalDraggedTabsContainer::InitializeDragState(
    TabDragTarget::DragController& controller) {
  CHECK(dragging_views_.empty());

  auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  on_scrolled_subscription_ =
      scroll_view->AddContentsScrolledCallback(base::BindRepeating(
          &VerticalDraggedTabsContainer::ApplyUpdatesForDragPositionChange,
          base::Unretained(this)));

  tab_strip_padding_ = GetLayoutConstant(
      IsTabStripCollapsed()
          ? LayoutConstant::kVerticalTabStripCollapsedPadding
          : LayoutConstant::kVerticalTabStripUncollapsedPadding);

  const auto& session_data = controller.GetSessionData();
  BuildDragLayout(session_data);

  InitializeDragStartAnimation(controller);
}

void VerticalDraggedTabsContainer::InitializeDragStartAnimation(
    const TabDragTarget::DragController& controller) {
  CHECK(animating_views_start_offsets_.empty());

  // Duration of the animation for dragged views to become contiguous.
  static constexpr base::TimeDelta kDragStartAnimationDuration =
      base::Milliseconds(200);
  static constexpr gfx::Tween::Type kStartDragAnimationTweenType =
      gfx::Tween::Type::EASE_IN_OUT;

  const auto& drag_handler = GetDragHandler();
  base::TimeDelta drag_time_elapsed =
      base::TimeTicks::Now() - controller.GetSessionData().drag_start_time;
  if (drag_time_elapsed >= kDragStartAnimationDuration) {
    return;
  }

  gfx::Point point_in_container = views::View::ConvertPointFromScreen(
      base::to_address(host_view_), last_drag_point_in_screen_);
  gfx::Rect dragged_bounds_in_container =
      GetDraggingViewsBoundsAtPointClamped(point_in_container);

  const auto* source_dragged_view = drag_handler.ViewFromTabSlot(
      controller.GetSessionData().source_view_drag_data()->attached_view);

  const auto source_view_target_position = GetDraggingViewPositionForBounds(
      source_dragged_view, dragged_bounds_in_container,
      dragging_views_.at(source_dragged_view).offset);

  // Build the map of dragging views to their offset between their position at
  // the start of the drag, and the position they're expected to be at this
  // moment.
  const bool should_compute_x_offset = IsHorizontalDragSupported();
  for (const auto& [dragging_view, visual_data] : dragging_views_) {
    auto start_offset_from_source =
        drag_handler.GetOffsetFromSourceAtDragStart(dragging_view);
    auto target_offset_from_source =
        GetDraggingViewPositionForBounds(
            dragging_view, dragged_bounds_in_container, visual_data.offset) -
        source_view_target_position;

    auto animation_offset =
        *start_offset_from_source - target_offset_from_source;
    if (!should_compute_x_offset) {
      animation_offset.set_x(0);
    }
    if (animation_offset != gfx::Vector2d()) {
      animating_views_start_offsets_.insert({dragging_view, animation_offset});
    }
  }

  // Don't start the animation if there are no offsets to animate.
  if (animating_views_start_offsets_.empty()) {
    return;
  }

  drag_start_animation_.SetTweenType(kStartDragAnimationTweenType);
  drag_start_animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(kDragStartAnimationDuration));

  // Set the animations value to be proportional according to the time that
  // elapsed since the drag started. This makes the transition between
  // containers smoother.
  drag_start_animation_.Reset(gfx::Tween::CalculateValue(
      kStartDragAnimationTweenType,
      drag_time_elapsed.InMillisecondsF() /
          kDragStartAnimationDuration.InMilliseconds()));
  drag_start_animation_.Show();
}

void VerticalDraggedTabsContainer::BuildDragLayout(
    const DragSessionData& session_data) {
  auto* source_dragged_view = GetDragHandler().ViewFromTabSlot(
      session_data.source_view_drag_data()->attached_view);
  CHECK(source_dragged_view);

  dragging_views_bounds_ = gfx::Rect();
  dragging_views_bounds_.Offset(
      GetSourceViewOffsetFromMouse(*source_dragged_view, session_data));

  const auto& target_layout = GetLayoutForDrag();
  for (auto* attached_view : session_data.attached_views()) {
    auto* dragging_view = GetDragHandler().ViewFromTabSlot(attached_view);
    CHECK(dragging_view);

    if (dragging_view->parent() != base::to_address(host_view_)) {
      continue;
    }
    if (dragging_views_.contains(dragging_view)) {
      // It's possible that multiple dragged tabs map to the same dragged view
      // (e.g., split tabs). Skip the duplicates.
      continue;
    }

    const bool is_source_view = dragging_view == source_dragged_view;
    const auto* dragging_view_layout =
        target_layout.GetLayoutFor(dragging_view);
    CHECK(dragging_view_layout);

    switch (drag_layout_) {
      case DragLayout::kVertical:
        CHECK(!IsHorizontalDragSupported());
        AddViewToVerticalDragLayout(dragging_view, dragging_view_layout->bounds,
                                    is_source_view);
        break;
      case DragLayout::kSquash:
        AddViewToSquashedDragLayout(dragging_view, dragging_view_layout->bounds,
                                    is_source_view);
        break;
      default:
        NOTREACHED();
    }
  }
}

void VerticalDraggedTabsContainer::AddViewToVerticalDragLayout(
    views::View* dragging_view,
    const gfx::Rect& view_bounds,
    bool is_source_dragged_view) {
  gfx::Rect bounds = view_bounds;
  bounds.set_y(dragging_views_bounds_.height());
  dragging_views_.insert(
      {dragging_view, {.offset = bounds.OffsetFromOrigin()}});

  static constexpr int kDraggedViewVerticalPadding = 2;
  dragging_views_bounds_.set_height(dragging_views_bounds_.height() +
                                    bounds.height() +
                                    kDraggedViewVerticalPadding);

  if (is_source_dragged_view) {
    dragging_views_bounds_.Offset({-1 * bounds.x(), -1 * bounds.y()});
  }
}

void VerticalDraggedTabsContainer::AddViewToSquashedDragLayout(
    views::View* dragging_view,
    const gfx::Rect& view_bounds,
    bool is_source_dragged_view) {
  if (is_source_dragged_view) {
    dragging_views_bounds_.set_size(view_bounds.size());
  }
  dragging_views_.insert(
      {dragging_view,
       {.offset = gfx::Vector2d(), .should_hide = !is_source_dragged_view}});
}

void VerticalDraggedTabsContainer::ResetDragState() {
  // Don't immediately clear `dragging_views_` so that the host view has a
  // chance to lay the dragged views out at their expected positions rather
  // than relying on `VerticalDraggedTabsContainer` to lay them out with
  // transforms.
  for (auto& [view, visual_data] : dragging_views_) {
    view->SetTransform(gfx::Transform());

    // The next layout update should allow the view to be shown by the host.
    visual_data.should_hide = false;
  }
  // The dragged view's bounds need to be snapped. While dragging, the bounds
  // are set to (0,0), but afterward the drag, the bounds must be updated to
  // the actual position, without animating.
  UpdateTargetLayoutForDrag(GetDraggingViews());
  dragging_views_.clear();
  animating_views_start_offsets_.clear();
  drag_start_animation_.Reset(0.0);
  dragging_views_bounds_ = gfx::Rect();

  on_scrolled_subscription_.reset();
}

void VerticalDraggedTabsContainer::UpdateDraggingViewTransforms(
    const gfx::Point& point_in_container) {
  const gfx::Rect bounding_box_for_point =
      GetDraggingViewsBoundsAtPointClamped(point_in_container);
  for (auto& [dragged_view, visual_data] : dragging_views_) {
    if (visual_data.should_hide && !drag_start_animation_.is_animating()) {
      continue;
    }
    // Use a transformation to render the dragged views, offset from the
    // container's origin.
    gfx::Transform transform;
    transform.Translate(GetDraggingViewPositionForBounds(
        dragged_view, bounding_box_for_point, visual_data.offset));

    dragged_view->SetTransform(transform);
  }
}

gfx::Rect VerticalDraggedTabsContainer::GetDraggingViewsBoundsAtPoint(
    const gfx::Point& point_in_container) const {
  gfx::Rect bounding_box_for_point = dragging_views_bounds_;
  bounding_box_for_point.Offset(point_in_container.OffsetFromOrigin());
  return bounding_box_for_point;
}

gfx::Rect VerticalDraggedTabsContainer::GetDraggingViewsBounds() const {
  gfx::Rect box_for_point = dragging_views_bounds_;
  box_for_point.Offset(
      views::View::ConvertPointFromScreen(base::to_address(host_view_),
                                          last_drag_point_in_screen_)
          .OffsetFromOrigin());
  return box_for_point;
}

gfx::Vector2d VerticalDraggedTabsContainer::GetDraggingViewPositionForBounds(
    const views::View* dragging_view,
    const gfx::Rect& dragging_views_bounding_box,
    const gfx::Vector2d& target_offset) const {
  gfx::Vector2d target(IsHorizontalDragSupported()
                           ? dragging_views_bounding_box.x() + target_offset.x()
                           : 0,
                       dragging_views_bounding_box.y() + target_offset.y());
  double value = drag_start_animation_.GetCurrentValue();
  if (drag_start_animation_.is_animating()) {
    if (auto it = animating_views_start_offsets_.find(dragging_view);
        it != animating_views_start_offsets_.end()) {
      target += {gfx::Tween::IntValueBetween(value, it->second.x(), 0),
                 gfx::Tween::IntValueBetween(value, it->second.y(), 0)};

      // If applying an offset for the drag-start animation, ensure we clamp
      // the offsets to the scroll view bounds.
      const auto* scroll_view = GetScrollViewForContainer();
      CHECK(scroll_view);
      gfx::Rect clamping_bounds = views::View::ConvertRectToTarget(
          scroll_view, base::to_address(host_view_),
          scroll_view->GetLocalBounds());
      gfx::Rect view_bounds(gfx::PointAtOffsetFromOrigin(target),
                            dragging_view->size());
      view_bounds.AdjustToFit(clamping_bounds);
      target = view_bounds.OffsetFromOrigin();
    }
  }

  return target;
}

gfx::Rect VerticalDraggedTabsContainer::GetDraggingViewsBoundsAtPointClamped(
    const gfx::Point& point_in_container) const {
  gfx::Rect bounding_box_for_point =
      GetDraggingViewsBoundsAtPoint(point_in_container);

  const auto* scroll_view = GetScrollViewForContainer();
  CHECK(scroll_view);
  gfx::Rect clamping_bounds = views::View::ConvertRectToTarget(
      scroll_view, base::to_address(host_view_), scroll_view->GetLocalBounds());
  clamping_bounds.set_width(clamping_bounds.width() - tab_strip_padding_);
  bounding_box_for_point.AdjustToFit(clamping_bounds);

  return bounding_box_for_point;
}

std::optional<VerticalDraggedTabsContainer::DraggedViewVisualData>
VerticalDraggedTabsContainer::GetVisualDataForDraggedView(
    const views::View& view) const {
  auto it = dragging_views_.find(&view);
  if (it == dragging_views_.end()) {
    return std::nullopt;
  }

  // Views that should be hidden are still shown while animating into position,
  // but are set to "float" so that surrounding views may also animate into
  // the end position.
  const bool should_hide =
      it->second.should_hide && !drag_start_animation_.is_animating();
  const bool should_float =
      it->second.should_hide && drag_start_animation_.is_animating();
  if (view.GetTransform().IsIdentity()) {
    // If a drag recently ended the child will still be in
    // `dragging_views_` but will not have a transformation, which let's
    // the tab view animate into its correct slot.
    const gfx::Point point_in_container = views::View::ConvertPointFromScreen(
        base::to_address(host_view_), last_drag_point_in_screen_);
    const gfx::Rect bounding_box_for_point =
        GetDraggingViewsBoundsAtPointClamped(point_in_container);
    return std::make_optional(DraggedViewVisualData{
        .offset = GetDraggingViewPositionForBounds(
            &view, bounding_box_for_point, it->second.offset),
        .should_hide = should_hide,
        .should_float = should_float,
    });
  }
  // If the tab is being dragged, then it is rendered using
  // transformations, offset from the container's origin.
  return DraggedViewVisualData{
      .offset = gfx::Vector2d(),
      .should_hide = should_hide,
      .should_float = should_float,
  };
}

views::View* VerticalDraggedTabsContainer::GetViewForDragBounds(
    const views::ProposedLayout& layout,
    const gfx::Rect& dragged_tab_bounds) {
  gfx::Rect logical_drag_bounds = dragged_tab_bounds;
  if (base::i18n::IsRTL()) {
    logical_drag_bounds.set_x(
        host_view_->GetMirroredXForRect(logical_drag_bounds));
  }

  for (const auto& child_layout : layout.child_layouts) {
    if (!child_layout.visible ||
        GetDragHandler().IsViewDragging(*child_layout.child_view)) {
      continue;
    }

    // The percentage overlap between dragged tabs and the view at its
    // target position to be considered the view over current drag bounds.
    constexpr float kEntryThreshold = 0.6f;

    if (HasMinimumOverlap(logical_drag_bounds, child_layout.bounds,
                          IsHorizontalDragSupported()
                              ? std::make_optional(child_layout.bounds.width() *
                                                   kEntryThreshold)
                              : std::nullopt,
                          child_layout.bounds.height() * kEntryThreshold)) {
      return child_layout.child_view;
    }
  }

  return nullptr;
}

bool VerticalDraggedTabsContainer::IsHorizontalDragSupported() const {
  return drag_axes_ != DragAxes::kVerticalOnly;
}

bool VerticalDraggedTabsContainer::HasMinimumOverlap(
    const gfx::Rect& a,
    const gfx::Rect& b,
    std::optional<int> min_x_overlap,
    std::optional<int> min_y_overlap) const {
  if (min_y_overlap) {
    gfx::RangeF vertical_overlap =
        gfx::RangeF(a.y(), a.bottom())
            .Intersect(gfx::RangeF(b.y(), b.bottom()));
    if (vertical_overlap.length() < *min_y_overlap) {
      return false;
    }
  }

  if (min_x_overlap) {
    gfx::RangeF horizontal_overlap =
        gfx::RangeF(a.x(), a.right()).Intersect(gfx::RangeF(b.x(), b.right()));
    if (horizontal_overlap.length() < *min_x_overlap) {
      return false;
    }
  }

  // Neither checks failed, so return `true`.
  return true;
}

bool VerticalDraggedTabsContainer::IsHandlingDrag() const {
  return !dragging_views_.empty();
}

VerticalTabDragHandler& VerticalDraggedTabsContainer::GetDragHandler() {
  return const_cast<VerticalTabDragHandler&>(
      std::as_const(*this).GetDragHandler());
}

const VerticalTabDragHandler& VerticalDraggedTabsContainer::GetDragHandler()
    const {
  CHECK(collection_node_);
  CHECK(collection_node_->GetController());
  return collection_node_->GetController()->GetDragHandler();
}

bool VerticalDraggedTabsContainer::IsTabStripCollapsed() const {
  CHECK(collection_node_);
  const auto* controller = collection_node_->GetController();
  return controller && controller->IsCollapsed();
}

void VerticalDraggedTabsContainer::ResetCollectionNode() {
  collection_node_ = nullptr;
}

std::vector<const views::View*> VerticalDraggedTabsContainer::GetDraggingViews()
    const {
  std::vector<const views::View*> views;
  views.reserve(dragging_views_.size());
  std::transform(dragging_views_.begin(), dragging_views_.end(),
                 std::back_inserter(views),
                 [](const auto& entry) { return entry.first; });

  return views;
}
