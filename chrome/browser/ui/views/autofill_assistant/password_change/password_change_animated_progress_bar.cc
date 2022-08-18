// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_progress_bar.h"

#include "base/callback.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/progress_bar.h"

PasswordChangeAnimatedProgressBar::PasswordChangeAnimatedProgressBar(int id)
    : gfx::LinearAnimation(this) {
  SetValue(0);
  SetID(id);
  SetDuration(kAnimationDuration);
}

PasswordChangeAnimatedProgressBar::~PasswordChangeAnimatedProgressBar() =
    default;

void PasswordChangeAnimatedProgressBar::SetAnimationEndedCallback(
    base::OnceClosure callback) {
  animation_ended_callback_ = std::move(callback);
}

void PasswordChangeAnimatedProgressBar::AnimationProgressed(
    const gfx::Animation* animation) {
  SetValue(GetCurrentValue());
}

void PasswordChangeAnimatedProgressBar::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackgroundColor(GetColorProvider()->GetColor(ui::kColorIconDisabled));
}

void PasswordChangeAnimatedProgressBar::AnimationEnded(
    const gfx::Animation* animation) {
  if (animation_ended_callback_) {
    std::move(animation_ended_callback_).Run();
  }
}
