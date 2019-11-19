// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_animation.h"

#include <algorithm>
#include <utility>

#include "base/numerics/ranges.h"
#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"
#include "ui/gfx/animation/tween.h"

namespace {

constexpr base::TimeDelta kZeroDuration = base::TimeDelta::FromMilliseconds(0);

}  // namespace

constexpr base::TimeDelta TabAnimation::kAnimationDuration;

TabAnimation::TabAnimation(TabAnimationState static_state,
                           base::OnceClosure tab_removed_callback)
    : initial_state_(static_state),
      target_state_(static_state),
      start_time_(base::TimeTicks::Now()),
      duration_(kZeroDuration),
      tab_removed_callback_(std::move(tab_removed_callback)) {}

TabAnimation::~TabAnimation() = default;

bool TabAnimation::IsClosing() const {
  return target_state_.IsFullyClosed();
}

bool TabAnimation::IsClosed() const {
  return target_state_.IsFullyClosed() && GetTimeRemaining().is_zero();
}

void TabAnimation::AnimateTo(TabAnimationState target_state) {
  initial_state_ = GetCurrentState();
  target_state_ = target_state;
  start_time_ = base::TimeTicks::Now();
  duration_ = kAnimationDuration;
}

void TabAnimation::RetargetTo(TabAnimationState target_state) {
  base::TimeDelta duration = GetTimeRemaining();

  initial_state_ = GetCurrentState();
  target_state_ = target_state;
  start_time_ = base::TimeTicks::Now();
  duration_ = duration;
}

void TabAnimation::CompleteAnimation() {
  initial_state_ = target_state_;
  start_time_ = base::TimeTicks::Now();
  duration_ = kZeroDuration;
}

void TabAnimation::NotifyCloseCompleted() {
  std::move(tab_removed_callback_).Run();
}

base::TimeDelta TabAnimation::GetTimeRemaining() const {
  return std::max(start_time_ + duration_ - base::TimeTicks::Now(),
                  kZeroDuration);
}

TabWidthConstraints TabAnimation::GetCurrentTabWidthConstraints(
    const TabLayoutConstants& layout_constants,
    const TabSizeInfo& size_info) const {
  return TabWidthConstraints(GetCurrentState(), layout_constants, size_info);
}

TabAnimationState TabAnimation::GetCurrentState() const {
  if (duration_.is_zero())
    return target_state_;

  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  const double normalized_elapsed_time = base::ClampToRange(
      elapsed_time.InMillisecondsF() / duration_.InMillisecondsF(), 0.0, 1.0);
  const double interpolation_value = gfx::Tween::CalculateValue(
      gfx::Tween::Type::EASE_OUT, normalized_elapsed_time);
  return TabAnimationState::Interpolate(interpolation_value, initial_state_,
                                        target_state_);
}
