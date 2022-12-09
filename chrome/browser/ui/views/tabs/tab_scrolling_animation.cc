// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_scrolling_animation.h"

TabScrollingAnimation::TabScrollingAnimation(
    views::View* contents_view,
    gfx::AnimationContainer* bounds_animator_container,
    base::TimeDelta duration,
    const gfx::Rect start_visible_rect,
    const gfx::Rect end_visible_rect)
    : gfx::LinearAnimation(duration,
                           gfx::LinearAnimation::kDefaultFrameRate,
                           this),
      contents_view_(contents_view),
      start_visible_rect_(start_visible_rect),
      end_visible_rect_(end_visible_rect) {
  SetContainer(bounds_animator_container);
}

void TabScrollingAnimation::AnimateToState(double state) {
  gfx::Rect intermediary_rect(
      start_visible_rect_.x() +
          (end_visible_rect_.x() - start_visible_rect_.x()) * state,
      start_visible_rect_.y(), start_visible_rect_.width(),
      start_visible_rect_.height());
  contents_view_->ScrollRectToVisible(intermediary_rect);
}

void TabScrollingAnimation::AnimationEnded(const gfx::Animation* animation) {
  contents_view_->ScrollRectToVisible(end_visible_rect_);
}
