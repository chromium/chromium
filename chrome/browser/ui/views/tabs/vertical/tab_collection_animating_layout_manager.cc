// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_animating_layout_manager.h"

#include "base/check_deref.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/view.h"

TabCollectionAnimatingLayoutManager::TabCollectionAnimatingLayoutManager(
    std::unique_ptr<LayoutManagerBase> target_layout_manager)
    : target_layout_manager_(
          CHECK_DEREF(AddOwnedLayout(std::move(target_layout_manager)))),
      animation_(this) {
  // TODO(crbug.com/459824840): Determine the appropriate animation duration.
  // Currently set to match the duration of TabContainerImpl.
  animation_.SetSlideDuration(
      gfx::Animation::RichAnimationDuration(base::Milliseconds(200)));
  animation_.SetTweenType(gfx::Tween::EASE_IN_OUT);
}

TabCollectionAnimatingLayoutManager::~TabCollectionAnimatingLayoutManager() =
    default;

gfx::Size TabCollectionAnimatingLayoutManager::GetPreferredSize(
    const views::View* host) const {
  return target_layout_manager_->GetPreferredSize(host);
}

gfx::Size TabCollectionAnimatingLayoutManager::GetPreferredSize(
    const views::View* host,
    const views::SizeBounds& available_size) const {
  return target_layout_manager_->GetPreferredSize(host, available_size);
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

void TabCollectionAnimatingLayoutManager::AnimationProgressed(
    const gfx::Animation* animation) {
  // Do not invalidate the target layout as the animation progresses, only the
  // animating layout manager requires invalidation.
  InvalidateHost(/*mark_layouts_changed=*/false);
}

void TabCollectionAnimatingLayoutManager::AnimationEnded(
    const gfx::Animation* animation) {
  // Do not invalidate the target layout as the animation progresses, only the
  // animating layout manager requires invalidation.
  InvalidateHost(/*mark_layouts_changed=*/false);
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
  RecalculateTarget();

  if (animation_.is_animating()) {
    current_layout_ = InterpolateLayout(animation_.GetCurrentValue());
    ApplyLayout(current_layout_);
  } else {
    // Ensure we are snapped to target.
    current_layout_ = target_layout_;
    ApplyLayout(target_layout_);
  }
}

void TabCollectionAnimatingLayoutManager::OnInstalled(views::View* host) {
  LayoutManagerBase::OnInstalled(host);
  RecalculateTarget();
}

void TabCollectionAnimatingLayoutManager::RecalculateTarget() {
  if (!host_view()) {
    return;
  }

  // Calculate the target layout with unbounded height and the width given to
  // the host by its parent view.
  views::ProposedLayout new_target = target_layout_manager_->GetProposedLayout(
      views::SizeBounds(host_view()->width(), {}), PassKey());

  // If the layout hasn't changed, we are done.
  if (new_target == target_layout_) {
    return;
  }

  // If this is the first layout and we are not animating, just snap to target.
  // Animating horizontal bounds is not supported and layout should immediately
  // snap to target for horizontal bounds changes.
  if ((target_layout_.child_layouts.empty() && !animation_.is_animating()) ||
      (current_layout_.host_size.width() != new_target.host_size.width())) {
    target_layout_ = new_target;
    current_layout_ = new_target;
    starting_layout_ = new_target;
    return;
  }

  // Update starting layout to the current state (current visual position). If
  // we are animating, `current_layout_` is the interpolated state. Since we are
  // not animating here, `current_layout_` is the previous target.
  starting_layout_ = current_layout_;
  target_layout_ = new_target;

  animation_.Reset(0.0);
  animation_.Show();
}

views::ProposedLayout TabCollectionAnimatingLayoutManager::InterpolateLayout(
    double value) const {
  views::ProposedLayout result;

  // Assume the host size snaps to its target size for the duration of the
  // animation.
  result.host_size = target_layout_.host_size;

  for (const auto& target_child : target_layout_.child_layouts) {
    // For the View associated with `target_child`, find its corresponding
    // starting layout - if it exists.
    const views::ChildLayout* start_child = nullptr;
    for (const auto& s : starting_layout_.child_layouts) {
      if (s.child_view == target_child.child_view) {
        start_child = &s;
        break;
      }
    }

    views::ChildLayout interpolated_child = target_child;
    if (start_child) {
      // Interpolate between start and target bounds.
      interpolated_child.bounds = gfx::Tween::RectValueBetween(
          value, start_child->bounds, target_child.bounds);
      // Snap visibility to target.
      interpolated_child.visible = target_child.visible;
    } else {
      // Animate-in new Views from empty bounds.
      // TODO(crbug.com/459824840): We may want to snap new children to target
      // bounds in the case of a tab drag-and-drop.
      gfx::Rect initial_bounds = target_child.bounds;
      initial_bounds.set_height(0);
      interpolated_child.bounds = gfx::Tween::RectValueBetween(
          value, initial_bounds, target_child.bounds);
    }
    result.child_layouts.push_back(interpolated_child);
  }
  return result;
}
