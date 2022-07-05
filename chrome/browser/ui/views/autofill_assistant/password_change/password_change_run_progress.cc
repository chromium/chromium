// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_progress.h"

#include <string>
#include <vector>

#include "chrome/app/vector_icons/vector_icons.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace {

constexpr int kIconSize = 16;

constexpr int kNColumns = 7;
constexpr int kIconColumnWidth = 28;
constexpr int kBarColumnMinWidth = 46;

constexpr int kAnimationDurationSeconds = 2;

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

class AnimatedProgressBar : public gfx::LinearAnimation,
                            public views::ProgressBar {
 public:
  explicit AnimatedProgressBar(int id) : gfx::LinearAnimation(this) {
    SetValue(0);
    SetID(id);
    SetDuration(base::Seconds(kAnimationDurationSeconds));
  }
  AnimatedProgressBar(const AnimatedProgressBar&) = delete;
  AnimatedProgressBar& operator=(const AnimatedProgressBar&) = delete;
  ~AnimatedProgressBar() override = default;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    SetValue(GetCurrentValue());
  };

  // Override AnimationEnded to avoid ui/views/controls/progress_bar.h DCHECK.
  void AnimationEnded(const gfx::Animation* animation) override {}
};

PasswordChangeRunProgress::PasswordChangeRunProgress(int childrenIDsOffset) {
  // TODO(crbug.com/1322419): Use correct missing icons and add animations to
  // them, see go/apc-desktop-ui.
  auto* layout = MakeTableLayout(this);
  layout->AddRows(1, views::TableLayout::kFixedSize);

  // The `PROGRESS_STEP_START` step is a simple circle icon with a pulsing
  // animation.
  progress_step_ui_elements_[autofill_assistant::password_change::ProgressStep::
                                 PROGRESS_STEP_START] = {
      .icon = AddChildView(
          views::Builder<views::ImageView>()
              .SetImage(ui::ImageModel::FromVectorIcon(
                  vector_icons::kSettingsIcon, ui::kColorIcon, kIconSize))
              .SetHorizontalAlignment(views::ImageView::Alignment::kLeading)
              .SetID(static_cast<int>(ChildrenViewsIds::kStartStepIcon) +
                     childrenIDsOffset)
              .Build())};

  progress_step_ui_elements_[autofill_assistant::password_change::ProgressStep::
                                 PROGRESS_STEP_CHANGE_PASSWORD] = {
      .progress_bar = AddChildView(std::make_unique<AnimatedProgressBar>(
          static_cast<int>(ChildrenViewsIds::kChangePasswordStepBar) +
          childrenIDsOffset)),
      .icon = AddChildView(
          views::Builder<views::ImageView>()
              .SetImage(ui::ImageModel::FromVectorIcon(
                  vector_icons::kSettingsIcon, ui::kColorIcon, kIconSize))
              .SetHorizontalAlignment(views::ImageView::Alignment::kLeading)
              .SetID(
                  static_cast<int>(ChildrenViewsIds::kChangePasswordStepIcon) +
                  childrenIDsOffset)
              .Build()),
  };

  progress_step_ui_elements_[autofill_assistant::password_change::ProgressStep::
                                 PROGRESS_STEP_SAVE_PASSWORD] = {
      .progress_bar = AddChildView(std::make_unique<AnimatedProgressBar>(
          static_cast<int>(ChildrenViewsIds::kSavePasswordStepBar))),
      .icon = AddChildView(
          views::Builder<views::ImageView>()
              .SetImage(ui::ImageModel::FromVectorIcon(kKeyIcon, ui::kColorIcon,
                                                       kIconSize))
              .SetHorizontalAlignment(views::ImageView::Alignment::kLeading)
              .SetID(static_cast<int>(ChildrenViewsIds::kSavePasswordStepIcon) +
                     childrenIDsOffset)

              .Build()),
  };

  progress_step_ui_elements_
      [autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_END] = {
          .progress_bar = AddChildView(std::make_unique<AnimatedProgressBar>(
              static_cast<int>(ChildrenViewsIds::kEndStepBar))),
          .icon = AddChildView(
              views::Builder<views::ImageView>()
                  .SetImage(ui::ImageModel::FromVectorIcon(
                      vector_icons::kCheckCircleIcon, ui::kColorIcon,
                      kIconSize))
                  .SetHorizontalAlignment(views::ImageView::Alignment::kLeading)
                  .SetID(static_cast<int>(ChildrenViewsIds::kEndStepIcon) +
                         childrenIDsOffset)
                  .Build()),
      };
}

PasswordChangeRunProgress::~PasswordChangeRunProgress() = default;

void PasswordChangeRunProgress::SetProgressBarStep(
    autofill_assistant::password_change::ProgressStep next_progress_step) {
  if (ProgressStepToIndex(next_progress_step) <=
      ProgressStepToIndex(current_progress_step_))
    return;

  current_progress_step_ = next_progress_step;
  // TODO(crbug.com/1322419): Finish animation of the prior step by filling the
  // icon color. This needs to be done before start filling the next progress
  // bar.
  progress_step_ui_elements_[current_progress_step_].progress_bar->Start();
}

autofill_assistant::password_change::ProgressStep
PasswordChangeRunProgress::GetCurrentProgressBarStep() {
  return current_progress_step_;
}

BEGIN_METADATA(PasswordChangeRunProgress, views::View)
END_METADATA
