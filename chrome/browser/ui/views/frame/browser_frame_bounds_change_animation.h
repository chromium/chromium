// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_BOUNDS_CHANGE_ANIMATION_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_BOUNDS_CHANGE_ANIMATION_H_

#include "base/memory/raw_ref.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class Widget;
}  // namespace views

// Animates a BrowserView's frame's bounds change.
class BrowserFrameBoundsChangeAnimation : public gfx::LinearAnimation {
 public:
  // `frame` MUST outlive `this`.
  BrowserFrameBoundsChangeAnimation(views::Widget& frame,
                                    const gfx::Rect& new_bounds);
  BrowserFrameBoundsChangeAnimation(const BrowserFrameBoundsChangeAnimation&) =
      delete;
  BrowserFrameBoundsChangeAnimation& operator=(
      const BrowserFrameBoundsChangeAnimation&) = delete;
  ~BrowserFrameBoundsChangeAnimation() override;

  // gfx::LinearAnimation:
  void AnimateToState(double state) override;

 private:
  const raw_ref<views::Widget> frame_;
  const gfx::Rect initial_bounds_;
  const gfx::Rect new_bounds_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_BOUNDS_CHANGE_ANIMATION_H_
