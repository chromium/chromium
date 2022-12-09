// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SCROLLING_ANIMATION_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SCROLLING_ANIMATION_H_

#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

// Helper class that manages the tab scrolling animation.
class TabScrollingAnimation : public gfx::LinearAnimation,
                              public gfx::AnimationDelegate {
 public:
  explicit TabScrollingAnimation(
      views::View* contents_view,
      gfx::AnimationContainer* bounds_animator_container,
      base::TimeDelta duration,
      const gfx::Rect start_visible_rect,
      const gfx::Rect end_visible_rect);

  TabScrollingAnimation(const TabScrollingAnimation&) = delete;
  TabScrollingAnimation& operator=(const TabScrollingAnimation&) = delete;
  ~TabScrollingAnimation() override = default;

  // gfx::LinearAnimation:
  void AnimateToState(double state) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  const raw_ptr<views::View> contents_view_;
  const gfx::Rect start_visible_rect_;
  const gfx::Rect end_visible_rect_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SCROLLING_ANIMATION_H_
