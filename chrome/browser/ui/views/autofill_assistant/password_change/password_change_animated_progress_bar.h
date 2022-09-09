// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_ANIMATED_PROGRESS_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_ANIMATED_PROGRESS_BAR_H_

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/progress_bar.h"

// Helper class to display an progress bar that is animated once, i.e. it draws
// from 0% to 100% once.
class PasswordChangeAnimatedProgressBar : public gfx::LinearAnimation,
                                          public views::ProgressBar {
 public:
  // The time it takes to move from 0% to 100% in the progress bar.
  static constexpr base::TimeDelta kAnimationDuration = base::Seconds(1);

  explicit PasswordChangeAnimatedProgressBar(int id);
  PasswordChangeAnimatedProgressBar(const PasswordChangeAnimatedProgressBar&) =
      delete;
  PasswordChangeAnimatedProgressBar& operator=(
      const PasswordChangeAnimatedProgressBar&) = delete;
  ~PasswordChangeAnimatedProgressBar() override;

  // Sets a `callback` that is executed when the progress bar animation
  // finishes.
  void SetAnimationEndedCallback(base::OnceClosure callback);

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void OnThemeChanged() override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // The callback to execute when the animation of the progress bar is finished.
  base::OnceClosure animation_ended_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_ANIMATED_PROGRESS_BAR_H_
