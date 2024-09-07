// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_bounds_change_animation.h"

#include "ui/gfx/animation/tween.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kDuration = base::Milliseconds(200);

}  // namespace

BrowserFrameBoundsChangeAnimation::BrowserFrameBoundsChangeAnimation(
    views::Widget& frame,
    const gfx::Rect& new_bounds)
    : gfx::LinearAnimation(kDuration, kDefaultFrameRate, nullptr),
      frame_(frame),
      initial_bounds_(frame_->GetWindowBoundsInScreen()),
      new_bounds_(new_bounds) {}

BrowserFrameBoundsChangeAnimation::~BrowserFrameBoundsChangeAnimation() =
    default;

void BrowserFrameBoundsChangeAnimation::AnimateToState(double state) {
  frame_->SetBounds(gfx::Tween::RectValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::EASE_IN_OUT_EMPHASIZED, state),
      initial_bounds_, new_bounds_));
}
