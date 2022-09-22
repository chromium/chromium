// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_progress.h"

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/ranges/algorithm.h"
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

int ProgressStepToIndex(PasswordChangeRunProgress::ProgressStep progress_step) {
  using autofill_assistant::password_change::ProgressStep;
  switch (progress_step) {
    case ProgressStep::PROGRESS_STEP_UNSPECIFIED:
      return 0;
    case ProgressStep::PROGRESS_STEP_START:
      return 1;
    case ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD:
      return 2;
    case ProgressStep::PROGRESS_STEP_SAVE_PASSWORD:
      return 3;
    case ProgressStep::PROGRESS_STEP_END:
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
  progress_step_ui_elements_[ProgressStep::PROGRESS_STEP_START] = {
      .icon = AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
          static_cast<int>(ChildrenViewsIds::kStartStepIcon) +
              childrenIDsOffset,
          ProgressStep::PROGRESS_STEP_START))};
  progress_step_ui_elements_[ProgressStep::PROGRESS_STEP_START]
      .icon->StartPulsingAnimation();

  progress_step_ui_elements_[ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD] = {
      .progress_bar =
          AddChildView(std::make_unique<PasswordChangeAnimatedProgressBar>(
              static_cast<int>(ChildrenViewsIds::kChangePasswordStepBar) +
              childrenIDsOffset)),
      .icon = AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
          static_cast<int>(ChildrenViewsIds::kChangePasswordStepIcon) +
              childrenIDsOffset,
          ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD)),
  };

  progress_step_ui_elements_[ProgressStep::PROGRESS_STEP_SAVE_PASSWORD] = {
      .progress_bar =
          AddChildView(std::make_unique<PasswordChangeAnimatedProgressBar>(
              static_cast<int>(ChildrenViewsIds::kSavePasswordStepBar))),
      .icon = AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
          static_cast<int>(ChildrenViewsIds::kSavePasswordStepIcon) +
              childrenIDsOffset,
          ProgressStep::PROGRESS_STEP_SAVE_PASSWORD)),
  };

  progress_step_ui_elements_[ProgressStep::PROGRESS_STEP_END] = {
      .progress_bar =
          AddChildView(std::make_unique<PasswordChangeAnimatedProgressBar>(
              static_cast<int>(ChildrenViewsIds::kEndStepBar))),
      .icon = AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
          static_cast<int>(ChildrenViewsIds::kEndStepIcon) + childrenIDsOffset,
          ProgressStep::PROGRESS_STEP_END)),
  };
}

PasswordChangeRunProgress::~PasswordChangeRunProgress() = default;

void PasswordChangeRunProgress::SetProgressBarStep(
    ProgressStep next_progress_step) {
  if (ProgressStepToIndex(next_progress_step) <=
      ProgressStepToIndex(current_progress_step_)) {
    return;
  }

  PasswordChangeAnimatedIcon* current_icon =
      progress_step_ui_elements_[current_progress_step_].icon;
  PasswordChangeAnimatedIcon* next_icon =
      progress_step_ui_elements_[next_progress_step].icon;
  // The last icon should only pulse once.
  bool pulse_once = next_progress_step == ProgressStep::PROGRESS_STEP_END;
  // Ensure that the next icon only starts pulsing once the previous one has
  // stopped pulsing.
  if (current_icon->IsPulsing()) {
    current_icon->SetAnimationEndedCallback(
        base::BindOnce(&PasswordChangeAnimatedIcon::StartPulsingAnimation,
                       base::Unretained(next_icon), pulse_once));
    current_icon->StopPulsingAnimation();
  } else {
    next_icon->StartPulsingAnimation(pulse_once);
  }

  current_progress_step_ = next_progress_step;
  progress_step_ui_elements_[current_progress_step_].progress_bar->Start();
}

PasswordChangeRunProgress::ProgressStep
PasswordChangeRunProgress::GetCurrentProgressBarStep() {
  return current_progress_step_;
}

void PasswordChangeRunProgress::SetAnimationEndedCallback(
    base::OnceClosure callback) {
  progress_step_ui_elements_[ProgressStep::PROGRESS_STEP_END]
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

bool PasswordChangeRunProgress::IsCompleted() const {
  auto is_pulsing =
      [](const std::pair<ProgressStep, ProgressStepUiElements>& el) -> bool {
    return el.second.icon->IsPulsing();
  };
  return current_progress_step_ == ProgressStep::PROGRESS_STEP_END &&
         base::ranges::none_of(progress_step_ui_elements_, is_pulsing);
}

BEGIN_METADATA(PasswordChangeRunProgress, views::View)
END_METADATA
