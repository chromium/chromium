// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_bounds_change_animation.h"

#include "ui/gfx/animation/tween.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kDuration = base::Milliseconds(200);

}  // namespace

PictureInPictureBoundsChangeAnimation::PictureInPictureBoundsChangeAnimation(
    views::Widget& pip_window,
    const gfx::Rect& new_bounds)
    : gfx::LinearAnimation(kDuration, kDefaultFrameRate, nullptr),
      pip_window_(pip_window),
      initial_bounds_(pip_window_->GetWindowBoundsInScreen()),
      new_bounds_(new_bounds) {}

PictureInPictureBoundsChangeAnimation::
    ~PictureInPictureBoundsChangeAnimation() = default;

void PictureInPictureBoundsChangeAnimation::AnimateToState(double state) {
  pip_window_->SetBounds(gfx::Tween::RectValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::EASE_IN_OUT_EMPHASIZED, state),
      initial_bounds_, new_bounds_));
}
