// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/reviewing_page_status_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

namespace {
constexpr base::TimeDelta kReviewingPageStatusDuration = base::Seconds(3);
}  // namespace

namespace dictation {

ReviewingPageStatusView::ReviewingPageStatusView(
    VisibilityChangedCallback visibility_changed_callback)
    : visibility_changed_callback_(std::move(visibility_changed_callback)),
      duration_(kReviewingPageStatusDuration),
      remaining_duration_(kReviewingPageStatusDuration) {
  SetVisible(false);
}

ReviewingPageStatusView::~ReviewingPageStatusView() = default;

void ReviewingPageStatusView::Init() {
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetMinimumCrossAxisSize(
          lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON));

  const int child_spacing =
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING);
  const gfx::Insets child_margins = gfx::Insets::TLBR(0, child_spacing, 0, 0);

  separator_ = AddChildView(std::make_unique<views::Separator>());
  separator_->SetColorId(ui::kColorSysNeutralOutline);
  separator_->SetPreferredLength(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT) / 2);
  separator_->SetProperty(views::kElementIdentifierKey,
                          DictationBubbleUi::kSeparatorElementIdForTesting);

  label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_DICTATION_REVIEWING_PAGE),
      CONTEXT_TOAST_BODY_TEXT, views::style::STYLE_PRIMARY));
  label_->SetEnabledColor(ui::kColorSysOnSurface);
  label_->SetProperty(views::kMarginsKey, child_margins);
  label_->SetProperty(
      views::kElementIdentifierKey,
      DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting);
}

void ReviewingPageStatusView::Show() {
  SetVisible(true);
  remaining_duration_ = duration_;
  UpdateTimer();
  visibility_changed_callback_.Run(true);
}

void ReviewingPageStatusView::SetDurationForTesting(base::TimeDelta duration) {
  duration_ = duration;
  remaining_duration_ = duration;
  if (timer_.IsRunning()) {
    timer_.Stop();
    UpdateTimer();
  }
}

void ReviewingPageStatusView::AddedToWidget() {
  if (auto* focus_manager = GetFocusManager()) {
    focus_manager->AddFocusChangeListener(this);
  }
}

void ReviewingPageStatusView::RemovedFromWidget() {
  if (auto* focus_manager = GetFocusManager()) {
    focus_manager->RemoveFocusChangeListener(this);
  }
}

void ReviewingPageStatusView::OnDidChangeFocus(views::View* focused_before,
                                               views::View* focused_now) {
  UpdateTimer();
}

void ReviewingPageStatusView::UpdateTimer() {
  if (!GetVisible()) {
    return;
  }
  const bool should_pause = HasFocusOrHover();
  if (timer_.IsRunning()) {
    if (should_pause) {
      timer_.Stop();
      const base::TimeDelta elapsed =
          base::TimeTicks::Now() - timer_start_time_;
      remaining_duration_ =
          std::max(base::TimeDelta(), remaining_duration_ - elapsed);
    }
    return;
  }
  if (should_pause) {
    return;
  }
  timer_start_time_ = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, remaining_duration_,
               base::BindOnce(&ReviewingPageStatusView::Dismiss,
                              base::Unretained(this)));
}

void ReviewingPageStatusView::Dismiss() {
  SetVisible(false);
  visibility_changed_callback_.Run(false);
}

bool ReviewingPageStatusView::HasFocusOrHover() const {
  if (parent() && parent()->IsMouseHovered()) {
    return true;
  }
  const views::FocusManager* focus_manager = GetFocusManager();
  const views::View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;
  return focused_view && parent() && parent()->Contains(focused_view);
}

BEGIN_METADATA(ReviewingPageStatusView)
END_METADATA

}  // namespace dictation
