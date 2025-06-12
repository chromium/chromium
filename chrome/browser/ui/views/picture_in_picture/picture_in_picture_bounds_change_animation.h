// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_BOUNDS_CHANGE_ANIMATION_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_BOUNDS_CHANGE_ANIMATION_H_

#include "base/memory/raw_ref.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class Widget;
}  // namespace views

// Animates a picture-in-picture window's views::Widget's bounds change.
class PictureInPictureBoundsChangeAnimation : public gfx::LinearAnimation {
 public:
  // `pip_window` MUST outlive `this`.
  PictureInPictureBoundsChangeAnimation(views::Widget& pip_window,
                                        const gfx::Rect& new_bounds);
  PictureInPictureBoundsChangeAnimation(
      const PictureInPictureBoundsChangeAnimation&) = delete;
  PictureInPictureBoundsChangeAnimation& operator=(
      const PictureInPictureBoundsChangeAnimation&) = delete;
  ~PictureInPictureBoundsChangeAnimation() override;

  // gfx::LinearAnimation:
  void AnimateToState(double state) override;

 private:
  const raw_ref<views::Widget> pip_window_;
  const gfx::Rect initial_bounds_;
  const gfx::Rect new_bounds_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_BOUNDS_CHANGE_ANIMATION_H_
