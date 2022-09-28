// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_progress.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_icon.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_progress_bar.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

using autofill_assistant::password_change::ProgressStep;
using ChildViewId = PasswordChangeRunProgress::ChildViewId;

namespace {

constexpr int kNColumns = 7;
constexpr int kIconColumnWidth = 28;
constexpr int kBarColumnMinWidth = 46;

constexpr int ProgressStepToIndex(ProgressStep progress_step) {
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

constexpr ChildViewId ProgressStepToIconId(
    PasswordChangeRunProgress::ProgressStep progress_step) {
  switch (progress_step) {
    case ProgressStep::PROGRESS_STEP_UNSPECIFIED:
      return ChildViewId::kUnknown;
    case ProgressStep::PROGRESS_STEP_START:
      return ChildViewId::kStartStepIcon;
    case ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD:
      return ChildViewId::kChangePasswordStepIcon;
    case ProgressStep::PROGRESS_STEP_SAVE_PASSWORD:
      return ChildViewId::kSavePasswordStepIcon;
    case ProgressStep::PROGRESS_STEP_END:
      return ChildViewId::kEndStepIcon;
  }
}

constexpr ChildViewId ProgressStepToProgressBarId(ProgressStep progress_step) {
  switch (progress_step) {
    case ProgressStep::PROGRESS_STEP_UNSPECIFIED:
      return ChildViewId::kUnknown;
    case ProgressStep::PROGRESS_STEP_START:
      return ChildViewId::kUnknown;
    case ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD:
      return ChildViewId::kChangePasswordStepBar;
    case ProgressStep::PROGRESS_STEP_SAVE_PASSWORD:
      return ChildViewId::kSavePasswordStepBar;
    case ProgressStep::PROGRESS_STEP_END:
      return ChildViewId::kEndStepBar;
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

PasswordChangeRunProgress::PasswordChangeRunProgress(
    OnChildAnimationContainerWasSetCallback container_set_callback)
    : container_set_callback_(std::move(container_set_callback)) {
  MakeTableLayout(this)->AddRows(1, views::TableLayout::kFixedSize);

  for (ProgressStep step : {ProgressStep::PROGRESS_STEP_START,
                            ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD,
                            ProgressStep::PROGRESS_STEP_SAVE_PASSWORD,
                            ProgressStep::PROGRESS_STEP_END}) {
    PasswordChangeAnimatedProgressBar* new_bar =
        (step != ProgressStep::PROGRESS_STEP_START
             ? AddChildView(std::make_unique<PasswordChangeAnimatedProgressBar>(
                   static_cast<int>(ProgressStepToProgressBarId(step))))
             : nullptr);

    // Every icon notifies the `PasswordChangeRunProgress` when it stops pulsing
    // so that the `PasswordChangeRunProgress` can decide whether to initiate
    // the pulsing of the subsequent item. This is to ensure that there never
    // are two simultaneously pulsing icons.
    ChildViewId icon_id = ProgressStepToIconId(step);
    PasswordChangeAnimatedIcon* new_icon =
        AddChildView(std::make_unique<PasswordChangeAnimatedIcon>(
            static_cast<int>(icon_id), step, /*delegate=*/this));

    progress_step_ui_elements_[step] = {.progress_bar = new_bar,
                                        .icon = new_icon};
  }

  // Initially, the first element should be pulsing.
  progress_step_ui_elements_[ProgressStep::PROGRESS_STEP_START]
      .icon->StartPulsingAnimation();
}

PasswordChangeRunProgress::~PasswordChangeRunProgress() = default;

void PasswordChangeRunProgress::SetProgressBarStep(
    ProgressStep next_progress_step) {
  if (ProgressStepToIndex(next_progress_step) <=
      ProgressStepToIndex(current_progress_step_)) {
    return;
  }

  if (absl::optional<ProgressStep> pulsing_step = GetPulsingProgressBarStep();
      pulsing_step.has_value()) {
    // If there is a pulsing element, stop the pulsing and push the next one
    // to the pending queue.
    pending_icon_animations_.push(next_progress_step);
    progress_step_ui_elements_[pulsing_step.value()]
        .icon->StopPulsingAnimation();
  } else {
    // If no element is pulsing, start the pulsing of the next step.
    progress_step_ui_elements_[next_progress_step].icon->StartPulsingAnimation(
        /*pulse_once=*/next_progress_step == ProgressStep::PROGRESS_STEP_END);
  }

  current_progress_step_ = next_progress_step;
  progress_step_ui_elements_[current_progress_step_].progress_bar->Start();
  pause_icon_animation_ = false;
}

PasswordChangeRunProgress::ProgressStep
PasswordChangeRunProgress::GetCurrentProgressBarStep() const {
  return current_progress_step_;
}

void PasswordChangeRunProgress::SetAnimationEndedCallback(
    base::OnceClosure callback) {
  animation_ended_callback_ = std::move(callback);
}

void PasswordChangeRunProgress::PauseIconAnimation() {
  pause_icon_animation_ = true;
  if (absl::optional<ProgressStep> pulsing_step = GetPulsingProgressBarStep();
      pulsing_step.has_value()) {
    progress_step_ui_elements_[pulsing_step.value()]
        .icon->StopPulsingAnimation();
  }
}

void PasswordChangeRunProgress::ResumeIconAnimation() {
  pause_icon_animation_ = false;
  progress_step_ui_elements_[current_progress_step_]
      .icon->StartPulsingAnimation();
}

bool PasswordChangeRunProgress::IsCompleted() const {
  return current_progress_step_ == ProgressStep::PROGRESS_STEP_END &&
         !GetPulsingProgressBarStep().has_value();
}

void PasswordChangeRunProgress::OnAnimationEnded(
    PasswordChangeAnimatedIcon* icon) {
  if (pending_icon_animations_.empty()) {
    if (current_progress_step_ == ProgressStep::PROGRESS_STEP_END &&
        animation_ended_callback_) {
      std::move(animation_ended_callback_).Run();
    }
    return;
  }

  // If there is more than one pending icon animation, the icon is the one for
  // the final step or the animations are intended to be paused, only pulse
  // once.
  ProgressStep next_step = pending_icon_animations_.front();
  pending_icon_animations_.pop();
  bool pulse_once = !pending_icon_animations_.empty() ||
                    next_step == ProgressStep::PROGRESS_STEP_END ||
                    pause_icon_animation_;
  progress_step_ui_elements_[next_step].icon->StartPulsingAnimation(pulse_once);
}

void PasswordChangeRunProgress::OnAnimationContainerWasSet(
    PasswordChangeAnimatedIcon* icon,
    gfx::AnimationContainer* container) {
  if (container_set_callback_) {
    container_set_callback_.Run(static_cast<ChildViewId>(icon->GetID()),
                                container);
  }
}

absl::optional<PasswordChangeRunProgress::ProgressStep>
PasswordChangeRunProgress::GetPulsingProgressBarStep() const {
  auto is_pulsing =
      [](const std::pair<ProgressStep, ProgressStepUiElements>& el) -> bool {
    return el.second.icon->IsPulsing();
  };
  auto pulsing_element =
      base::ranges::find_if(progress_step_ui_elements_, is_pulsing);

  if (pulsing_element != progress_step_ui_elements_.end()) {
    return pulsing_element->first;
  } else {
    return absl::nullopt;
  }
}

BEGIN_METADATA(PasswordChangeRunProgress, views::View)
END_METADATA
