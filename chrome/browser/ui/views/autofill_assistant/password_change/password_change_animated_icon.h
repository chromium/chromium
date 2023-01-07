// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_ANIMATED_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_ANIMATED_ICON_H_

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
class AnimationContainer;
}  // namespace gfx

// A pulsing icon used as an element of the password change progress bar.
class PasswordChangeAnimatedIcon : public gfx::LinearAnimation,
                                   public gfx::AnimationDelegate,
                                   public views::ImageView {
 public:
  class Delegate {
   public:
    // Handles that the animation has ended.
    virtual void OnAnimationEnded(PasswordChangeAnimatedIcon* icon) = 0;

    // Handles that the animation container was set. Used for testing purposes
    // only.
    virtual void OnAnimationContainerWasSet(
        PasswordChangeAnimatedIcon* icon,
        gfx::AnimationContainer* container) = 0;
  };

  // The duration of one icon pulse cycle.
  static constexpr base::TimeDelta kAnimationDuration = base::Seconds(1);

  PasswordChangeAnimatedIcon(
      int id,
      autofill_assistant::password_change::ProgressStep progress_step,
      Delegate* delegate);
  PasswordChangeAnimatedIcon(const PasswordChangeAnimatedIcon&) = delete;
  PasswordChangeAnimatedIcon& operator=(const PasswordChangeAnimatedIcon&) =
      delete;
  ~PasswordChangeAnimatedIcon() override;

  // Starts the pulsing of the icon. If the icon is already pulsing and not
  // in its last cycle, it does nothing. If the icon is in its last pulse cycle,
  // it sets it to keep pulsing.
  // If `pulse_once` is `true`, it ensures that it will pulse at most once (less
  // if is currently pulsing).
  void StartPulsingAnimation(bool pulse_once = false);

  // Signals to stop pulsing the animation after completing the current cycle.
  void StopPulsingAnimation();

  // Returns whether the icon is currently pulsing.
  bool IsPulsing() const;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationContainerWasSet(gfx::AnimationContainer* container) override;

  // The progress step with which this icon is associated. Determines the icon
  // that is shown.
  const autofill_assistant::password_change::ProgressStep progress_step_;

  // Describes whether the animation should be pulsing.
  bool pulsing_animation_ = false;

  // Is `true` when the animation is currently not pulsing, `false` otherwise.
  bool animation_ended_ = true;

  // A raw pointer to the delegate for this icon that must outlive `this`.
  Delegate* delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_ANIMATED_ICON_H_
