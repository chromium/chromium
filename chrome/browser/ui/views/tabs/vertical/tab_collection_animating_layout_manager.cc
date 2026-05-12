// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"

#include <algorithm>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(
    TabCollectionAnimatingLayoutManager::SourceLayoutInfo*)

namespace {

// Views of removed TabCollectionNodes may temporarily remain in the View tree
// to allow them to animate-out. This property is used to ensure these views
// are removed from the View tree once they are no longer required.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPendingDeletion, false)

// Stores the bounds in screen coordinates of the associated View prior to being
// removed from its host TabCollectionNode. Used for collection move animations.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kPreviousCollectionBounds)

// Stores optional source layout information of the associated View. Used for
// collection move animations.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(
    TabCollectionAnimatingLayoutManager::SourceLayoutInfo,
    kSourceLayoutInfo)

}  // namespace

bool TabCollectionAnimatingLayoutManager::Delegate::IsDragging() const {
  return false;
}

bool TabCollectionAnimatingLayoutManager::Delegate::IsViewDragging(
    const views::View& child_view) const {
  return false;
}

bool TabCollectionAnimatingLayoutManager::Delegate::ShouldSnapToTarget(
    const views::View& child_view) const {
  return false;
}

bool TabCollectionAnimatingLayoutManager::Delegate::
    ShouldAnimateOpacityForAddAndRemove(const views::View& child_view) const {
  return false;
}

void TabCollectionAnimatingLayoutManager::Delegate::OnAnimationEnded() {}

TabCollectionAnimatingLayoutManager::TabCollectionAnimatingLayoutManager(
    std::unique_ptr<LayoutManagerBase> target_layout_manager,
    Delegate& delegate,
    AnimationAxis animation_axis,
    bool animate_host_size)
    : views::AnimationDelegateViews(target_layout_manager->host_view()),
      target_layout_manager_(
          CHECK_DEREF(AddOwnedLayout(std::move(target_layout_manager)))),
      animation_(this),
      delegate_(delegate),
      animation_axis_(animation_axis),
      animate_host_size_(animate_host_size) {
  animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(base::Milliseconds(200)));
  animation_.SetTweenType(gfx::Tween::EASE_IN_OUT);
}

TabCollectionAnimatingLayoutManager::~TabCollectionAnimatingLayoutManager() =
    default;

bool TabCollectionAnimatingLayoutManager::OnViewAdded(views::View* host,
                                                      views::View* view) {
  // Do not attempt to animate opacity for views reparented from another
  // collection.
  if (!view->GetProperty(kPreviousCollectionBounds) &&
      !delegate_->IsViewDragging(*view) &&
      delegate_->ShouldAnimateOpacityForAddAndRemove(*view)) {
    // Added views should animate opacity from invisible to fully opaque.
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
    view->layer()->SetOpacity(0.0f);
  }
  return LayoutManagerBase::OnViewAdded(host, view);
}

bool TabCollectionAnimatingLayoutManager::OnViewRemoved(views::View* host,
                                                        views::View* view) {
  ClearViewAnimationMetadataForView(view);
  return LayoutManagerBase::OnViewRemoved(host, view);
}

gfx::Size TabCollectionAnimatingLayoutManager::GetPreferredSize(
    const views::View* host) const {
  // While animating have preferred size reflect the actual computed height of
  // the current layout. Do so to ensure animations are not clipped when
  // animating-out a view at the bottom of the scroll view - and to ensure
  // adjacent children in parent views sit flush with this view while animating
  // if they do not employ their own animating layout manager.
  gfx::Size target_preferred_size =
      target_layout_manager_->GetPreferredSize(host);
  if (animate_host_size_ && animation_.is_animating() &&
      !delegate_->IsDragging()) {
    target_preferred_size.set_height(current_layout_content_height_);
  }
  return target_preferred_size;
}

gfx::Size TabCollectionAnimatingLayoutManager::GetPreferredSize(
    const views::View* host,
    const views::SizeBounds& available_size) const {
  // While animating have preferred size reflect the actual computed height of
  // the current layout. Do so to ensure animations are not clipped when
  // animating-out a view at the bottom of the scroll view - and to ensure
  // adjacent children in parent views sit flush with this view while animating
  // if they do not employ their own animating layout manager.
  gfx::Size target_preferred_size =
      target_layout_manager_->GetPreferredSize(host, available_size);
  if (animate_host_size_ && animation_.is_animating() &&
      !delegate_->IsDragging()) {
    target_preferred_size.set_height(current_layout_content_height_);
  }
  return target_preferred_size;
}

gfx::Size TabCollectionAnimatingLayoutManager::GetMinimumSize(
    const views::View* host) const {
  return target_layout_manager_->GetMinimumSize(host);
}

int TabCollectionAnimatingLayoutManager::GetPreferredHeightForWidth(
    const views::View* host,
    int width) const {
  return target_layout_manager_->GetPreferredHeightForWidth(host, width);
}

void TabCollectionAnimatingLayoutManager::OnLayoutChanged() {
  // If a new layout target was computed ensure we recalculate and update the
  // current layout to ensure layout metadata is available for preferred size
  // calculations. Running this once to compute metadata is more efficient than
  // having preferred size methods interpolating layout on each function call.
  if (RecalculateTarget()) {
    UpdateCurrentLayout();
  }
  LayoutManagerBase::OnLayoutChanged();
}

void TabCollectionAnimatingLayoutManager::AnimationProgressed(
    const gfx::Animation* animation) {
  if (current_offset_ == animation->GetCurrentValue()) {
    return;
  }

  // Pre-calculate the interpolated `current_layout_` and content height for
  // this frame so that `GetPreferredSize()` returns the correct bounds during
  // animation when queried by the parent.
  UpdateCurrentLayout();

  // Do not invalidate the target layout as the animation progresses, only the
  // animating layout manager requires invalidation.
  InvalidateHost(/*mark_layouts_changed=*/false);
}

void TabCollectionAnimatingLayoutManager::AnimationEnded(
    const gfx::Animation* animation) {
  // Do not invalidate the target layout as the animation progresses, only the
  // animating layout manager requires invalidation.
  InvalidateHost(/*mark_layouts_changed=*/false);

  // Clear any View-specific metadata and state no longer needed once the most
  // recent animation has finished.
  ClearViewAnimationMetadata();
  delegate_->OnAnimationEnded();
}

// static.
void TabCollectionAnimatingLayoutManager::SetSourceLayoutInfo(
    views::View* view_to_reparent,
    std::unique_ptr<SourceLayoutInfo> source_layout_info) {
  view_to_reparent->SetProperty(kSourceLayoutInfo,
                                std::move(source_layout_info));
}

views::ProposedLayout
TabCollectionAnimatingLayoutManager::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // If we are animating, return the current interpolated state. Otherwise,
  // return the target state.
  return animation_.is_animating() ? current_layout_
                                   : target_layout_manager_->GetProposedLayout(
                                         size_bounds, PassKey());
}

void TabCollectionAnimatingLayoutManager::LayoutImpl() {
  // TODO(crbug.com/490964477): It should not be necessary to call this here
  // given it is called in `OnLayoutChange()`, however there is a tab-dragging
  // dependency on this behavior. Once this has been resolved remove this call.
  RecalculateTarget();

  if (animation_.is_animating()) {
    UpdateCurrentLayout();
    ApplyLayout(current_layout_);
  } else {
    // Ensure we are snapped to target.
    current_offset_ = 1.0;
    starting_offset_ = 0.0;
    current_layout_ = target_layout_;
    SetStartingLayout(target_layout_);
    ApplyLayout(target_layout_);
    RemoveNonAnimatingPendingDeleteViews();
    ClearViewAnimationMetadata();
  }
}

void TabCollectionAnimatingLayoutManager::OnInstalled(views::View* host) {
  LayoutManagerBase::OnInstalled(host);
  RecalculateTarget();
}

void TabCollectionAnimatingLayoutManager::SetStartingLayout(
    const views::ProposedLayout& starting_layout) {
  // Create a set of current child views for fast lookup. This is necessary
  // as `starting_layout` may contain Views already removed from the View tree
  // and destroyed.
  std::vector<const views::View*> child_views;
  child_views.reserve(host_view()->children().size());
  std::ranges::transform(
      host_view()->children(), std::back_inserter(child_views),
      [](const auto& child_view) { return child_view.get(); });
  base::flat_set<const views::View*> child_view_set(std::move(child_views));

  // Map view pointers to their starting layouts.
  std::vector<ChildViewLayoutMap::value_type> start_bounds_pairs;
  start_bounds_pairs.reserve(starting_layout.child_layouts.size());
  for (const views::ChildLayout& layout : starting_layout.child_layouts) {
    if (child_view_set.contains(layout.child_view.get())) {
      start_bounds_pairs.emplace_back(layout.child_view.get(), layout);
    }
  }
  start_view_layout_map_ = ChildViewLayoutMap(std::move(start_bounds_pairs));

  starting_layout_ = starting_layout;
}

void TabCollectionAnimatingLayoutManager::SetTargetLayout(
    const views::ProposedLayout& target_layout) {
  // Map view pointers to their target layouts.
  std::vector<ChildViewLayoutMap::value_type> target_bounds_pairs;
  target_bounds_pairs.reserve(target_layout.child_layouts.size());
  for (const views::ChildLayout& layout : target_layout.child_layouts) {
    views::View* child_view = layout.child_view.get();
    target_bounds_pairs.emplace_back(child_view, layout);
  }
  target_view_layout_map_ = ChildViewLayoutMap(std::move(target_bounds_pairs));

  target_layout_ = target_layout;
}

void TabCollectionAnimatingLayoutManager::UpdateCurrentLayout() {
  current_offset_ = animation_.GetCurrentValue();
  double denominator = 1.0 - starting_offset_;
  double percent = (current_offset_ - starting_offset_) / denominator;
  percent = std::clamp(percent, 0.0, 1.0);
  current_layout_ = InterpolateLayout(percent);
}

bool TabCollectionAnimatingLayoutManager::RecalculateTarget() {
  if (!host_view()) {
    return false;
  }

  // Calculate the target layout with unbounded height and the width given to
  // the host by its parent view.
  views::ProposedLayout new_target = target_layout_manager_->GetProposedLayout(
      views::SizeBounds(host_view()->width(), {}), PassKey());

  // If the layout hasn't changed, we are done.
  if (new_target == target_layout_) {
    return false;
  }

  // Animating horizontal bounds is not supported and layout should immediately
  // snap to target for horizontal bounds changes.
  if (!current_layout_.host_size.IsEmpty() &&
      (current_layout_.host_size.width() != new_target.host_size.width())) {
    current_layout_ = new_target;
    SetStartingLayout(new_target);
    SetTargetLayout(new_target);
    starting_offset_ = 0.0;
    current_offset_ = 1.0;
    return true;
  }

  SetTargetLayout(new_target);

  // If we haven't actually rendered a frame yet keep the original
  // starting_layout.
  if (animation_.is_animating() && current_offset_ == starting_offset_) {
    return true;
  }

  constexpr double kResetAnimationThreshold = 0.8;

  if (current_offset_ > kResetAnimationThreshold) {
    // We are far enough along that we should start a "fresh" animation
    // from 0% to avoid awkward slow-downs at the end of the curve.
    SetStartingLayout(current_layout_);
    starting_offset_ = 0.0;
    current_offset_ = 0.0;
    animation_.Reset(0.0);
  } else {
    // We are still early in the animation. Simply update the starting offset.
    // The timer remains running and we just calculate a new slope in
    // LayoutImpl.
    SetStartingLayout(current_layout_);
    starting_offset_ = current_offset_;
  }

  if (!animation_.is_animating()) {
    animation_.Show();
  }
  return true;
}

void TabCollectionAnimatingLayoutManager::ResetViewsToTargetLayout(
    const std::vector<const views::View*>& views_to_snap) {
  RecalculateTarget();
  for (const auto* view : views_to_snap) {
    const auto* target_child_layout = target_layout_.GetLayoutFor(view);
    if (!target_child_layout) {
      continue;
    }
    auto* starting_child_layout = starting_layout_.GetLayoutFor(view);
    if (starting_child_layout) {
      starting_child_layout->bounds = target_child_layout->bounds;
    }
  }
  SetStartingLayout(starting_layout_);
  SetTargetLayout(target_layout_);
  // Call `InterpolateLayout()` to ensure layout metadata is available for
  // preferred size calculations
  InterpolateLayout(0.0);
  InvalidateHost(/*mark_layouts_changed=*/true);
}

void TabCollectionAnimatingLayoutManager::AnimateAndDestroyChildView(
    views::View* child_view) {
  DCHECK(std::ranges::contains(host_view()->children(), child_view));
  child_view->SetCanProcessEventsWithinSubtree(false);
  child_view->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  child_view->SetProperty(kPendingDeletion, true);

  if (delegate_->ShouldAnimateOpacityForAddAndRemove(*child_view)) {
    // Removed views should animate opacity from fully opaque to invisible.
    child_view->SetPaintToLayer();
    child_view->layer()->SetFillsBoundsOpaquely(false);
    child_view->layer()->SetOpacity(1.0f);
  }

  InvalidateHost(/*mark_layouts_changed=*/true);
}

void TabCollectionAnimatingLayoutManager::AnimateAndReparentView(
    std::unique_ptr<views::View> view_to_reparent,
    const gfx::Rect& previous_bounds_in_screen) {
  // Ensure `kPreviousCollectionBounds` metadata is set before adding
  // `view_to_reparent` to ensure view-added lifecycle hooks have the necessary
  // information to determine whether the view was reparented or newly-added.
  if (!delegate_->IsViewDragging(*view_to_reparent) &&
      !previous_bounds_in_screen.IsEmpty()) {
    view_to_reparent->SetPaintToLayer();
    view_to_reparent->layer()->SetFillsBoundsOpaquely(false);
    view_to_reparent->SetProperty(kPreviousCollectionBounds,
                                  previous_bounds_in_screen);
  }
  host_view()->AddChildView(std::move(view_to_reparent));
}

views::ProposedLayout TabCollectionAnimatingLayoutManager::InterpolateLayout(
    double value) const {
  views::ProposedLayout result;

  // Assume the host size snaps to its target size for the duration of the
  // animation.
  result.host_size = target_layout_.host_size;

  // Reset the previously computed total content height.
  current_layout_content_height_ = 0;

  for (views::View* child_view : host_view()->children()) {
    auto target_it = target_view_layout_map_.find(child_view);
    if (target_it != target_view_layout_map_.end()) {
      views::ChildLayout interpolated_child = target_it->second;

      if (delegate_->IsViewDragging(*child_view)) {
        // Always use the target bounds for dragging views. The drag target
        // should handle layout and animations as needed.
        // In the case a drag starts on a view mid-animation ensure it snaps to
        // being opaque.
        if (child_view->layer() && (child_view->layer()->opacity() != 1.0f)) {
          child_view->layer()->SetOpacity(1.0f);
        }
      } else if (auto start_it = start_view_layout_map_.find(child_view);
                 start_it != start_view_layout_map_.end()) {
        // Moved child.
        // Interpolate between start and target bounds.
        interpolated_child.bounds = gfx::Tween::RectValueBetween(
            value, start_it->second.bounds, target_it->second.bounds);
        // Snap visibility to target.
        interpolated_child.visible = target_it->second.visible;
        // In the case layouts are updated mid-animation, ensure any previously
        // added views that are now treated as moved views are snapped to being
        // opaque.
        if (child_view->layer() && (child_view->layer()->opacity() != 1.0f)) {
          child_view->layer()->SetOpacity(1.0f);
        }
      } else if (!delegate_->ShouldSnapToTarget(*child_view)) {
        // Added child.
        // Animate-in new Views from empty bounds.
        gfx::Rect* previous_container_bounds =
            child_view->GetProperty(kPreviousCollectionBounds);
        if (previous_container_bounds) {
          gfx::Rect initial_bounds = views::View::ConvertRectFromScreen(
              host_view(), *previous_container_bounds);
          interpolated_child.bounds = gfx::Tween::RectValueBetween(
              value, initial_bounds, target_it->second.bounds);
        } else {
          gfx::Rect initial_bounds = target_it->second.bounds;
          if (animation_axis_ == AnimationAxis::kVertical) {
            initial_bounds.set_height(0);
          } else {
            initial_bounds.set_width(0);
          }
          interpolated_child.bounds = gfx::Tween::RectValueBetween(
              value, initial_bounds, target_it->second.bounds);
          if (child_view->layer()) {
            child_view->layer()->SetOpacity(static_cast<float>(value));
          }
        }
      } else {
        // This branch results in new children being snapped to target bounds
        // (e.g. drag-and-drop or split-tabs which explicitly requires no
        // animated transition).
      }
      if (!interpolated_child.bounds.IsEmpty()) {
        current_layout_content_height_ = std::max(
            current_layout_content_height_, interpolated_child.bounds.bottom());
      }
      result.child_layouts.push_back(interpolated_child);
      continue;
    }

    // Note: `start_view_layout_map_` will only contain start ChildLayouts for
    // the views that are still parented to `host_view()`. This is not the case
    // for `start_layout_`.
    // Animate-out only pending delete views that were present in the previous
    // layout.
    auto start_it = start_view_layout_map_.find(child_view);
    if (start_it != start_view_layout_map_.end() &&
        child_view->GetProperty(kPendingDeletion)) {
      // Removed child.
      // Pending delete Views will remain in the Views hierarchy until they are
      // no longer needed for animation (i.e. they are no longer in
      // `starting_layout_`), at which point they will be removed by
      // `RemoveNonAnimatingPendingDeleteViews()`.
      views::ChildLayout interpolated_child = start_it->second;
      gfx::Rect target_bounds = start_it->second.bounds;
      if (animation_axis_ == AnimationAxis::kVertical) {
        target_bounds.set_height(0);
      } else {
        target_bounds.set_width(0);
      }
      interpolated_child.bounds = gfx::Tween::RectValueBetween(
          value, start_it->second.bounds, target_bounds);
      if (child_view->layer()) {
        child_view->layer()->SetOpacity(static_cast<float>(1.0 - value));
      }

      if (!interpolated_child.bounds.IsEmpty()) {
        current_layout_content_height_ = std::max(
            current_layout_content_height_, interpolated_child.bounds.bottom());
      }
      result.child_layouts.push_back(interpolated_child);
      continue;
    }

    // Animate out any child views moved into `host_view()` with their
    // TabCollectioNode subsequently destroyed before a layout and animation has
    // had the chance to occur. In such cases where the child view's
    // TabCollectionNode is destroyed it will not appear in proposed layouts
    // (which are based on the current TabStripCollection model). This can occur
    // in compound model updates involving moving tabs into their parent
    // collection after which the tab and its source collection are destroyed.
    // Note: Guard against container views that update target layouts without
    // directly removing the view children from the view tree. This can happen
    // during teardown for e.g. where a container node is reset and animated
    // away before removing and/or animating out its children. The target layout
    // of the container in this case would have no view children since its
    // TabCollectionNode is destroyed, despite these views still being present
    // in the start layout.
    gfx::Rect* previous_collection_bounds =
        child_view->GetProperty(kPreviousCollectionBounds);
    DCHECK(!target_view_layout_map_.contains(child_view));
    if (previous_collection_bounds &&
        !start_view_layout_map_.contains(child_view)) {
      const gfx::Rect start_bounds = views::View::ConvertRectFromScreen(
          host_view(), *previous_collection_bounds);

      // Target bounds for the animate-out animation depends on
      // source_layout_info.
      gfx::Rect target_bounds = start_bounds;
      SourceLayoutInfo* source_layout_info =
          child_view->GetProperty(kSourceLayoutInfo);
      if (!source_layout_info) {
        if (animation_axis_ == AnimationAxis::kVertical) {
          target_bounds.set_height(0);
        } else {
          target_bounds.set_width(0);
        }
      } else if (source_layout_info->animation_axis.value_or(animation_axis_) ==
                 AnimationAxis::kVertical) {
        if (source_layout_info->animation_direction ==
            AnimationDirection::kStartToEnd) {
          target_bounds.set_y(start_bounds.bottom());
        }
        target_bounds.set_height(0);
      } else {
        if (source_layout_info->animation_direction ==
            AnimationDirection::kStartToEnd) {
          target_bounds.set_x(start_bounds.right());
        }
        target_bounds.set_width(0);
      }

      views::ChildLayout interpolated_child;
      interpolated_child.visible = child_view->GetVisible();
      interpolated_child.child_view = child_view;
      interpolated_child.bounds =
          gfx::Tween::RectValueBetween(value, start_bounds, target_bounds);

      if (!interpolated_child.bounds.IsEmpty()) {
        current_layout_content_height_ = std::max(
            current_layout_content_height_, interpolated_child.bounds.bottom());
      }
      result.child_layouts.push_back(interpolated_child);
    }
  }

  return result;
}

void TabCollectionAnimatingLayoutManager::
    RemoveNonAnimatingPendingDeleteViews() {
  // Collect all children marked as pending delete.
  std::vector<std::pair<views::View*, bool>> pending_delete_child_view_pairs;
  pending_delete_child_view_pairs.reserve(host_view()->children().size());
  for (views::View* child : host_view()->children()) {
    if (child->GetProperty(kPendingDeletion)) {
      pending_delete_child_view_pairs.emplace_back(child, true);
    }
  }

  // Early return if there are no pending-delete children to remove.
  if (pending_delete_child_view_pairs.empty()) {
    return;
  }

  // Create a map of pending delete child views, with the value indicating
  // whether the view is absent from `starting_layout_` and should be removed.
  base::flat_map<views::View*, bool> pending_delete_child_view_map(
      std::move(pending_delete_child_view_pairs));

  // Ensure that any pending delete views still in `starting_layout_` are
  // retained.
  for (const views::ChildLayout& layout : starting_layout_.child_layouts) {
    auto it = pending_delete_child_view_map.find(layout.child_view);
    if (it != pending_delete_child_view_map.end()) {
      it->second = false;
    }
  }

  // Remove any pending delete views no longer in `starting_layout_`.
  int removed_child_count = 0;
  for (auto& [child, should_remove] : pending_delete_child_view_map) {
    if (should_remove) {
      host_view()->RemoveChildViewT(child);
      removed_child_count++;
    }
  }

  // Dispatch synthesized mouse move event using the current mouse location
  // to refresh hover status.
  if (removed_child_count > 0) {
    if (views::Widget* widget = host_view()->GetWidget()) {
      widget->SynthesizeMouseMoveEvent();
    }
  }
}

void TabCollectionAnimatingLayoutManager::ClearViewAnimationMetadata() {
  for (views::View* child_view : host_view()->children()) {
    ClearViewAnimationMetadataForView(child_view);
  }
}

void TabCollectionAnimatingLayoutManager::ClearViewAnimationMetadataForView(
    views::View* view) {
  if (!delegate_->IsViewDragging(*view)) {
    view->DestroyLayer();
  }
  view->ClearProperty(kPreviousCollectionBounds);
  view->ClearProperty(kSourceLayoutInfo);
}
