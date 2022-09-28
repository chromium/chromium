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

namespace gfx {
class AnimationContainer;
}  // namespace gfx

namespace {

constexpr int kIconSize = 16;

const gfx::VectorIcon& ProgressStepToIcon(
    autofill_assistant::password_change::ProgressStep progress_step) {
  using autofill_assistant::password_change::ProgressStep;
  switch (progress_step) {
    case ProgressStep::PROGRESS_STEP_UNSPECIFIED:
    case ProgressStep::PROGRESS_STEP_START:
      return autofill_assistant::password_change::
          kPasswordChangeProgressStartIcon;
    case ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD:
      return vector_icons::kSettingsIcon;
    case ProgressStep::PROGRESS_STEP_SAVE_PASSWORD:
      return kKeyIcon;
    case ProgressStep::PROGRESS_STEP_END:
      return vector_icons::kCheckCircleIcon;
  }
}

}  // namespace

PasswordChangeAnimatedIcon::PasswordChangeAnimatedIcon(
    int id,
    autofill_assistant::password_change::ProgressStep progress_step,
    Delegate* delegate)
    : gfx::LinearAnimation(this),
      progress_step_(progress_step),
      delegate_(delegate) {
  SetID(id);
  SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  SetImage(ui::ImageModel::FromVectorIcon(ProgressStepToIcon(progress_step_),
                                          ui::kColorIconDisabled, kIconSize));
}

PasswordChangeAnimatedIcon::~PasswordChangeAnimatedIcon() = default;

void PasswordChangeAnimatedIcon::StartPulsingAnimation(bool pulse_once) {
  bool is_already_pulsing = IsPulsing();
  pulsing_animation_ = !pulse_once;
  animation_ended_ = false;

  // Only start a new cycle if the icon is not already pulsing.
  if (!is_already_pulsing) {
    SetDuration(kAnimationDuration);
    Start();
  }
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
  if (pulsing_animation_) {
    Start();
  } else {
    animation_ended_ = true;
    delegate_->OnAnimationEnded(this);
  }
}

void PasswordChangeAnimatedIcon::AnimationContainerWasSet(
    gfx::AnimationContainer* container) {
  delegate_->OnAnimationContainerWasSet(this, container);
}

bool PasswordChangeAnimatedIcon::IsPulsing() const {
  return !animation_ended_;
}
