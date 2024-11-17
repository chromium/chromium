// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/autofill_progress_dialog_views.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

AutofillProgressDialogViews::AutofillProgressDialogViews(
    base::WeakPtr<AutofillProgressDialogController> controller)
    : controller_(controller) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_->GetCancelButtonLabel());
  SetCancelCallback(
      base::BindOnce(&AutofillProgressDialogViews::OnDialogCanceled,
                     weak_ptr_factory_.GetWeakPtr()));
  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetShowCloseButton(false);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  auto* throbber_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  progress_throbber_ =
      throbber_container->AddChildView(std::make_unique<views::Throbber>());

  label_ = AddChildView(std::make_unique<views::Label>(
      controller_->GetLoadingMessage(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY));
  label_->SetMultiLine(true);
  label_->SetEnabledColorId(ui::kColorThrobber);
  // Set the maximum width of the label view so it will not compress the
  // throbber view.
  label_->SetMaximumWidth(views::LayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                          progress_throbber_->GetPreferredSize().width() -
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              views::DISTANCE_RELATED_CONTROL_HORIZONTAL) -
                          margins().width());

  // Center-align the throbber vertically with the first line of the label.
  progress_throbber_->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_top((label_->GetLineHeight() -
                             progress_throbber_->GetPreferredSize().height()) /
                            2));
}

AutofillProgressDialogViews::~AutofillProgressDialogViews() {
  // This if-statement is always entered, unless the tab is closed. In this
  // scenario `controller_` will already be invalidated.
  if (controller_) {
    controller_->OnDismissed(is_canceled_by_user_);
    controller_ = nullptr;
  }
}

void AutofillProgressDialogViews::Dismiss(bool show_confirmation_before_closing,
                                          bool is_canceled_by_user) {
  is_canceled_by_user_ = is_canceled_by_user;

  // If `show_confirmation_before_closing` is true, show the confirmation and
  // close the widget with a delay. `show_confirmation_before_closing` being
  // true implies that the user did not cancel the dialog, as it is only set to
  // true once this step in the current flow is completed without any user
  // interaction.
  if (show_confirmation_before_closing && controller_) {
    progress_throbber_->Stop();
    label_->SetText(controller_->GetConfirmationMessage());
    // For accessibility consideration, announce the confirmation message when
    // unmasking is finished.
    label_->GetViewAccessibility().AnnouncePolitely(
        controller_->GetConfirmationMessage());
    progress_throbber_->SetChecked(true);
    GetBubbleFrameView()->SetTitleView(
        std::make_unique<TitleWithIconAfterLabelView>(
            controller_->GetConfirmationTitle(),
            TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AutofillProgressDialogViews::CloseWidget,
                       weak_ptr_factory_.GetWeakPtr()),
        kDelayBeforeDismissingProgressDialog);
    return;
  }

  // Otherwise close the widget directly.
  CloseWidget();
}

void AutofillProgressDialogViews::InvalidateControllerForCallbacks() {
  controller_ = nullptr;
}

base::WeakPtr<AutofillProgressDialogView>
AutofillProgressDialogViews::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillProgressDialogViews::AddedToWidget() {
  DCHECK(progress_throbber_);
  progress_throbber_->Start();

  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

std::u16string AutofillProgressDialogViews::GetWindowTitle() const {
  return controller_ ? controller_->GetLoadingTitle() : u"";
}

void AutofillProgressDialogViews::CloseWidget() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void AutofillProgressDialogViews::OnDialogCanceled() {
  is_canceled_by_user_ = true;
}

base::WeakPtr<AutofillProgressDialogView> CreateAndShowProgressDialog(
    base::WeakPtr<AutofillProgressDialogController> controller,
    content::WebContents* web_contents) {
  AutofillProgressDialogViews* dialog_view =
      new AutofillProgressDialogViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  return dialog_view->GetWeakPtr();
}

}  // namespace autofill
