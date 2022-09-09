// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_icon.h"

#include "base/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autofill_assistant/password_change/vector_icons/vector_icons.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/image_view.h"

namespace {

constexpr int kIconSize = 16;

const gfx::VectorIcon& ProgressStepToIcon(
    autofill_assistant::password_change::ProgressStep progress_step) {
  switch (progress_step) {
    case autofill_assistant::password_change::ProgressStep::
        PROGRESS_STEP_UNSPECIFIED:
    case autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START:
      return autofill_assistant::password_change::
          kPasswordChangeProgressStartIcon;
    case autofill_assistant::password_change::ProgressStep::
        PROGRESS_STEP_CHANGE_PASSWORD:
      return vector_icons::kSettingsIcon;
    case autofill_assistant::password_change::ProgressStep::
        PROGRESS_STEP_SAVE_PASSWORD:
      return kKeyIcon;
    case autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_END:
      return vector_icons::kCheckCircleIcon;
  }
}

}  // namespace

PasswordChangeAnimatedIcon::PasswordChangeAnimatedIcon(
    int id,
    autofill_assistant::password_change::ProgressStep progress_step)
    : gfx::LinearAnimation(this), progress_step_(progress_step) {
  SetID(id);
  SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  SetImage(ui::ImageModel::FromVectorIcon(ProgressStepToIcon(progress_step_),
                                          ui::kColorIconDisabled, kIconSize));
}

PasswordChangeAnimatedIcon::~PasswordChangeAnimatedIcon() = default;

void PasswordChangeAnimatedIcon::SetAnimationEndedCallback(
    base::OnceClosure callback) {
  animation_ended_callback_ = std::move(callback);
  // If the animation is already finished, run callback right away.
  if (animation_ended_) {
    std::move(animation_ended_callback_).Run();
  }
}

void PasswordChangeAnimatedIcon::StartPulsingAnimation() {
  // Do not start a new cycle if the icon is already pulsing.
  if (IsPulsing()) {
    // Always set this variable in case it was the last pulse cycle.
    pulsing_animation_ = true;
    return;
  }

  pulsing_animation_ = true;
  animation_ended_ = false;

  SetDuration(kAnimationDuration);
  Start();
}

void PasswordChangeAnimatedIcon::StopPulsingAnimation() {
  pulsing_animation_ = false;
}

void PasswordChangeAnimatedIcon::AnimationProgressed(
    const gfx::Animation* animation) {
  const SkColor progress_bar_color =
      GetColorProvider()->GetColor(ui::kColorProgressBar);

  SetImage(ui::ImageModel::FromVectorIcon(
      ProgressStepToIcon(progress_step_),
      SkColorSetA(progress_bar_color, GetCurrentValue() * 0xFF), kIconSize));
}

void PasswordChangeAnimatedIcon::AnimationEnded(
    const gfx::Animation* animation) {
  // Add one more cycle after stop animation request to avoid abrupt changes.
  if (pulsing_animation_ || last_animation_cycle_) {
    if (!pulsing_animation_)
      last_animation_cycle_ = false;
    Start();
  } else {
    animation_ended_ = true;
    if (animation_ended_callback_) {
      std::move(animation_ended_callback_).Run();
    }
  }
}

bool PasswordChangeAnimatedIcon::IsPulsing() const {
  return !animation_ended_;
}
