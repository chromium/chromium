// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_progress.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_icon.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_progress_bar.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace {

constexpr int kNColumns = 7;
constexpr int kIconColumnWidth = 28;
constexpr int kBarColumnMinWidth = 46;

int ProgressStepToIndex(
    autofill_assistant::password_change::ProgressStep progress_step) {
  switch (progress_step) {
    case autofill_assistant::password_change::ProgressStep::
        PROGRESS_STEP_UNSPECIFIED:
      return 0;
    case autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START:
      return 1;
    case autofill_assistant::password_change::ProgressStep::
        PROGRESS_STEP_CHANGE_PASSWORD:
      return 2;
    case autofill_assistant::password_change::ProgressStep::
        PROGRESS_STEP_SAVE_PASSWORD:
      return 3;
    case autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_END:
      return 4;
  }
}

// Creates the layout for a password change run progress bar.
views::TableLayout* MakeTableLayout(views::View* host) {
  // Num of columns to represent a password change run progress bar.
  auto* layout = host->SetLayoutManager(std::make_unique<views::TableLayout>());
  for (int i = 0; i < kNColumns; ++i) {
    // Even columns are specific to icons. Therefore they have different
    // dimensions.
    if (i % 2 == 0) {
      layout->AddColumn(views::LayoutAlignment::kCenter,
                        views::LayoutAlignment::kCenter,
                        views::TableLayout::kFixedSize,
                        views::TableLayout::ColumnSize::kFixed,
                        kIconColumnWidth, kIconColumnWidth);
    } else {
      layout->AddColumn(views::LayoutAlignment::kStretch,
                        views::LayoutAlignment::kCenter, 1.0f,
                        views::TableLayout::ColumnSize::kUsePreferred,
                        /* does not matter since the width is not fixed*/ 0,
                        kBarColumnMinWidth);
    }
  }
  return layout;
}

}  // namespace

PasswordChangeRunProgress::PasswordChangeRunProgress(int childrenIDsOffset) {
  views::TableLayout* layout = MakeTableLayout(this);
  layout->AddRows(1, views::TableLayout::kFixedSize);

  // The `PROGRESS_STEP_START` step is a simple circle icon with a pulsing
  // animation.
  progress_step_ui_elements_
      [autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START] =
          {.icon = AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
               static_cast<int>(ChildrenViewsIds::kStartStepIcon) +
                   childrenIDsOffset,
               autofill_assistant::password_change::ProgressStep::
                   PROGRESS_STEP_START))};
  progress_step_ui_elements_
      [autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START]
          .icon->StartPulsingAnimation();

  progress_step_ui_elements_[autofill_assistant::password_change::ProgressStep::
                                 PROGRESS_STEP_CHANGE_PASSWORD] = {
      .progress_bar =
          AddChildView(std::make_unique<PasswordChangeAnimatedProgressBar>(
              static_cast<int>(ChildrenViewsIds::kChangePasswordStepBar) +
              childrenIDsOffset)),
      .icon = AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
          static_cast<int>(ChildrenViewsIds::kChangePasswordStepIcon) +
              childrenIDsOffset,
          autofill_assistant::password_change::ProgressStep::
              PROGRESS_STEP_CHANGE_PASSWORD)),
  };

  progress_step_ui_elements_[autofill_assistant::password_change::ProgressStep::
                                 PROGRESS_STEP_SAVE_PASSWORD] = {
      .progress_bar =
          AddChildView(std::make_unique<PasswordChangeAnimatedProgressBar>(
              static_cast<int>(ChildrenViewsIds::kSavePasswordStepBar))),
      .icon = AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
          static_cast<int>(ChildrenViewsIds::kSavePasswordStepIcon) +
              childrenIDsOffset,
          autofill_assistant::password_change::ProgressStep::
              PROGRESS_STEP_SAVE_PASSWORD)),
  };

  progress_step_ui_elements_
      [autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_END] = {
          .progress_bar =
              AddChildView(std::make_unique<PasswordChangeAnimatedProgressBar>(
                  static_cast<int>(ChildrenViewsIds::kEndStepBar))),
          .icon = AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
              static_cast<int>(ChildrenViewsIds::kEndStepIcon) +
                  childrenIDsOffset,
              autofill_assistant::password_change::ProgressStep::
                  PROGRESS_STEP_END)),
      };
}

PasswordChangeRunProgress::~PasswordChangeRunProgress() = default;

void PasswordChangeRunProgress::SetProgressBarStep(
    autofill_assistant::password_change::ProgressStep next_progress_step) {
  if (ProgressStepToIndex(next_progress_step) <=
      ProgressStepToIndex(current_progress_step_))
    return;

  progress_step_ui_elements_[current_progress_step_]
      .icon->StopPulsingAnimation();

  current_progress_step_ = next_progress_step;

  // If we reached the end we animate the last progress bar and set up a
  // callback to complete the last icon.
  if (current_progress_step_ ==
      autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_END) {
    progress_step_ui_elements_
        [autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_END]
            .progress_bar->SetAnimationEndedCallback(base::BindOnce(
                &PasswordChangeRunProgress::OnLastProgressBarAnimationCompleted,
                base::Unretained(this)));
  }
  progress_step_ui_elements_[current_progress_step_].progress_bar->Start();
  progress_step_ui_elements_[current_progress_step_]
      .icon->StartPulsingAnimation();
}

autofill_assistant::password_change::ProgressStep
PasswordChangeRunProgress::GetCurrentProgressBarStep() {
  return current_progress_step_;
}

void PasswordChangeRunProgress::SetAnimationEndedCallback(
    base::OnceClosure callback) {
  progress_step_ui_elements_
      [autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_END]
          .icon->SetAnimationEndedCallback(std::move(callback));
}

void PasswordChangeRunProgress::PauseIconAnimation() {
  progress_step_ui_elements_[current_progress_step_]
      .icon->StopPulsingAnimation();
}

void PasswordChangeRunProgress::ResumeIconAnimation() {
  progress_step_ui_elements_[current_progress_step_]
      .icon->StartPulsingAnimation();
}

void PasswordChangeRunProgress::OnLastProgressBarAnimationCompleted() {
  progress_step_ui_elements_
      [autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_END]
          .icon->StopPulsingAnimation();
}

BEGIN_METADATA(PasswordChangeRunProgress, views::View)
END_METADATA
