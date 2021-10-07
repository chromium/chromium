// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/autofill_progress_dialog_views.h"

#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace autofill {

AutofillProgressDialogViews::AutofillProgressDialogViews(
    AutofillProgressDialogController* controller)
    : controller_(controller) {
  SetButtons(ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller_->GetCancelButtonLabel());
  SetModalType(ui::MODAL_TYPE_CHILD);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetShowCloseButton(false);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  progress_throbber_ = AddChildView(std::make_unique<views::Throbber>());

  label_ = AddChildView(std::make_unique<views::Label>(
      controller_->GetLoadingMessage(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY));
  label_->SetMultiLine(true);
}

AutofillProgressDialogViews::~AutofillProgressDialogViews() {
  if (controller_) {
    controller_->OnDismissed();
    controller_ = nullptr;
  }
}

// static
AutofillProgressDialogView* AutofillProgressDialogView::CreateAndShow(
    AutofillProgressDialogController* controller) {
  AutofillProgressDialogViews* dialog_view =
      new AutofillProgressDialogViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view,
                                              controller->GetWebContents());
  return dialog_view;
}

void AutofillProgressDialogViews::Dismiss() {
  if (controller_) {
    controller_->OnDismissed();
    controller_ = nullptr;
  }
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void AutofillProgressDialogViews::ShowConfirmation() {
  label_->SetText(controller_->GetConfirmationMessage());
  progress_throbber_->SetChecked(true);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AutofillProgressDialogViews::Dismiss,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(1));
}

void AutofillProgressDialogViews::AddedToWidget() {
  DCHECK(progress_throbber_);
  progress_throbber_->Start();

  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(
          GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

void AutofillProgressDialogViews::OnThemeChanged() {
  views::DialogDelegateView::OnThemeChanged();
  SkColor enabled_color = GetColorProvider()->GetColor(ui::kColorThrobber);
  label_->SetEnabledColor(enabled_color);
}

std::u16string AutofillProgressDialogViews::GetWindowTitle() const {
  return controller_->GetTitle();
}

}  // namespace autofill
