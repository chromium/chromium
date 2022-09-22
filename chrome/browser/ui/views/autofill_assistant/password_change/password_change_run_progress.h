// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_PROGRESS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_PROGRESS_H_

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "ui/views/view.h"

class PasswordChangeAnimatedIcon;
class PasswordChangeAnimatedProgressBar;

// A password change run progress indicator that consists of a combination of
// individual progress bars and icons.
class PasswordChangeRunProgress : public views::View {
 public:
  METADATA_HEADER(PasswordChangeRunProgress);

  // IDs that identify a view within the dialog that was used in browsertests.
  // The offset is used to ensure that the IDs do not overlap with the parent
  // dialog.
  enum class ChildrenViewsIds : int {
    kStartStepIcon = 100,
    kChangePasswordStepIcon = 101,
    kChangePasswordStepBar = 102,
    kSavePasswordStepIcon = 103,
    kSavePasswordStepBar = 104,
    kEndStepIcon = 105,
    kEndStepBar = 106,
  };

  // `childrendsIDsOffset` can be used by parent views to make sure that the
  // `PasswordChangeRunProgress` children view ids do not collide with the
  // parent's.
  explicit PasswordChangeRunProgress(int childrenIDsOffset = 0);

  PasswordChangeRunProgress(const PasswordChangeRunProgress&) = delete;
  PasswordChangeRunProgress& operator=(const PasswordChangeRunProgress&) =
      delete;

  ~PasswordChangeRunProgress() override;

  // Sets the current progress. Does nothing if `next_progress_step` is
  // logically before or equal to `current_progress_step`.
  void SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep next_progress_step);

  // Returns the current progress bar step.
  autofill_assistant::password_change::ProgressStep GetCurrentProgressBarStep();

  // Adds a callback for when the progress bar is complete.
  // The completion happens after the last step animation is done.
  void SetAnimationEndedCallback(base::OnceClosure callback);

  // Pauses the animation of the icon of the current step.
  void PauseIconAnimation();

  // Resumes the animation of the icon of the current step.
  void ResumeIconAnimation();

 private:
  // Method run once the last progress bar animation is completed that is used
  // to trigger the last item animation.
  void OnLastProgressBarAnimationCompleted();

  // A progress step is made out of an icon, a progress bar, or both.
  struct ProgressStepUIElements {
    raw_ptr<PasswordChangeAnimatedProgressBar> progress_bar = nullptr;
    raw_ptr<PasswordChangeAnimatedIcon> icon = nullptr;
  };

  // Maps a progress step to the UI elements that represent it.
  base::flat_map<autofill_assistant::password_change::ProgressStep,
                 ProgressStepUIElements>
      progress_step_ui_elements_;

  autofill_assistant::password_change::ProgressStep current_progress_step_ =
      autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_PASSWORD_CHANGE_RUN_PROGRESS_H_
