// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_PROGRESS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_PROGRESS_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_icon.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/view.h"

namespace gfx {
class AnimationContainer;
}  // namespace gfx

class PasswordChangeAnimatedProgressBar;

// A password change run progress indicator that consists of a combination of
// individual progress bars and icons.
class PasswordChangeRunProgress : public views::View,
                                  public PasswordChangeAnimatedIcon::Delegate {
 public:
  METADATA_HEADER(PasswordChangeRunProgress);

  // IDs that identify a view within the dialog that was used in browsertests.
  // The offset is used to ensure that the IDs do not overlap with the parent
  // dialog.
  enum class ChildViewId : int {
    kUnknown = 0,
    kStartStepIcon = 100,
    kChangePasswordStepIcon = 101,
    kChangePasswordStepBar = 102,
    kSavePasswordStepIcon = 103,
    kSavePasswordStepBar = 104,
    kEndStepIcon = 105,
    kEndStepBar = 106,
  };

  using ProgressStep = autofill_assistant::password_change::ProgressStep;
  using OnChildAnimationContainerWasSetCallback =
      base::RepeatingCallback<void(ChildViewId, gfx::AnimationContainer*)>;

  explicit PasswordChangeRunProgress(
      OnChildAnimationContainerWasSetCallback container_set_callback =
          OnChildAnimationContainerWasSetCallback());

  PasswordChangeRunProgress(const PasswordChangeRunProgress&) = delete;
  PasswordChangeRunProgress& operator=(const PasswordChangeRunProgress&) =
      delete;

  ~PasswordChangeRunProgress() override;

  // Sets the current progress. Does nothing if `next_progress_step` is
  // logically before or equal to `current_progress_step`.
  void SetProgressBarStep(ProgressStep next_progress_step);

  // Returns the current progress bar step.
  ProgressStep GetCurrentProgressBarStep() const;

  // Returns the step that is currently pulsing or `absl::nullopt` is there is
  // none.
  absl::optional<ProgressStep> GetPulsingProgressBarStep() const;

  // Adds a callback for when the progress bar is complete.
  // The completion happens after the last step animation is done.
  void SetAnimationEndedCallback(base::OnceClosure callback);

  // Pauses the animation of the icon of the current step.
  void PauseIconAnimation();

  // Resumes the animation of the icon of the current step.
  void ResumeIconAnimation();

  // Returns whether the progress bar state corresponds to a completed flow,
  // i.e. whether the progress step is `ProgressStep::PROGRESS_STEP_END`
  // and no more icons are blinking.
  bool IsCompleted() const;

 private:
  // PasswordChangeAnimatedIcon::Delegate:
  // Reacts to an icon that stops blinking by either starting the animation of
  // the next icon or executing the callback that signals that the entire
  // progress bar animation is complete.
  void OnAnimationEnded(PasswordChangeAnimatedIcon* icon) override;
  void OnAnimationContainerWasSet(PasswordChangeAnimatedIcon* icon,
                                  gfx::AnimationContainer* container) override;

  // A progress step is made out of an icon, a progress bar, or both.
  struct ProgressStepUiElements {
    raw_ptr<PasswordChangeAnimatedProgressBar> progress_bar = nullptr;
    raw_ptr<PasswordChangeAnimatedIcon> icon = nullptr;
  };

  // Map of a progress step to the UI elements that represents it.
  base::flat_map<ProgressStep, ProgressStepUiElements>
      progress_step_ui_elements_;

  ProgressStep current_progress_step_ = ProgressStep::PROGRESS_STEP_START;

  // A queue of icons that is yet to be animated.
  base::queue<ProgressStep> pending_icon_animations_;

  // The callback to execute when the progress bar hits its final step.
  base::OnceClosure animation_ended_callback_;

  // An indication of whether the icon animation should be stopped as soon as it
  // can (after every icon in the `pending_icon_animations_` queue has pulsed
  // at least once).
  bool pause_icon_animation_ = false;

  // Callback that is executed when one of the children's animation container
  // is set. Used for testing purposes only.
  // Currently, this only covers animated icons.
  OnChildAnimationContainerWasSetCallback container_set_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_PROGRESS_H_
