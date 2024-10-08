// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/card_unmask_otp_input_dialog_views.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

CardUnmaskOtpInputDialogViews::CardUnmaskOtpInputDialogViews(
    base::WeakPtr<CardUnmaskOtpInputDialogController> controller)
    : controller_(controller) {
  SetShowTitle(true);
  SetButtonLabel(ui::mojom::DialogButton::kOk, controller_->GetOkButtonLabel());
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 GetDialogButtonLabel(ui::mojom::DialogButton::kCancel));
  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kText));
  InitViews();
}

CardUnmaskOtpInputDialogViews::~CardUnmaskOtpInputDialogViews() {
  // Inform |controller_| of the dialog's destruction.
  if (controller_) {
    controller_->OnDialogClosed(/*user_closed_dialog=*/true,
                                /*server_request_succeeded=*/false);
    controller_ = nullptr;
  }
}

void CardUnmaskOtpInputDialogViews::ShowPendingState() {
  otp_input_view_->SetVisible(false);
  progress_view_->SetVisible(true);
  progress_throbber_->Start();
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
}

void CardUnmaskOtpInputDialogViews::ShowInvalidState(
    const std::u16string& invalid_label_text) {
  otp_input_view_->SetVisible(true);
  progress_view_->SetVisible(false);
  progress_throbber_->Stop();
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  otp_input_textfield_->SetInvalid(true);
  otp_input_textfield_invalid_label_->SetVisible(true);
  otp_input_textfield_invalid_label_->SetText(invalid_label_text);
  otp_input_textfield_invalid_label_padding_->SetVisible(false);
  otp_input_textfield_->GetViewAccessibility().SetName(
      *otp_input_textfield_invalid_label_);
}

void CardUnmaskOtpInputDialogViews::Dismiss(
    bool show_confirmation_before_closing,
    bool user_closed_dialog) {
  // If |show_confirmation_before_closing| is true, show the confirmation and
  // close the widget with a delay.
  if (controller_ && show_confirmation_before_closing) {
    progress_throbber_->Stop();
    progress_label_->SetText(controller_->GetConfirmationMessage());
    progress_throbber_->SetChecked(true);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CardUnmaskOtpInputDialogViews::CloseWidget,
                       weak_ptr_factory_.GetWeakPtr(), user_closed_dialog,
                       /*server_request_succeeded=*/true),
        kDelayBeforeDismissingProgressDialog);
    return;
  }

  // Otherwise close the widget directly.
  CloseWidget(user_closed_dialog, /*server_request_succeeded=*/false);
}

base::WeakPtr<CardUnmaskOtpInputDialogView>
CardUnmaskOtpInputDialogViews::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::u16string CardUnmaskOtpInputDialogViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : u"";
}

void CardUnmaskOtpInputDialogViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

bool CardUnmaskOtpInputDialogViews::Accept() {
  if (controller_) {
    controller_->OnOkButtonClicked(otp_input_textfield_->GetText());
  }
  ShowPendingState();
  return false;
}

views::View* CardUnmaskOtpInputDialogViews::GetInitiallyFocusedView() {
  DCHECK(otp_input_textfield_);
  return otp_input_textfield_;
}

void CardUnmaskOtpInputDialogViews::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  if (otp_input_textfield_->GetInvalid())
    HideInvalidState();

  SetButtonEnabled(
      ui::mojom::DialogButton::kOk,
      /*enabled=*/controller_ && controller_->IsValidOtp(new_contents));
}

void CardUnmaskOtpInputDialogViews::InitViews() {
  DCHECK(children().empty());
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  CreateOtpInputView();
  CreateHiddenProgressView();
}

void CardUnmaskOtpInputDialogViews::CreateOtpInputView() {
  otp_input_view_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  otp_input_view_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  otp_input_view_->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL));

  // Set initial creation of |otp_input_view_| to visible as it is the first
  // view shown in the dialog.
  otp_input_view_->SetVisible(true);

  // Adds view that combines textfield and textfield invalid label.
  auto* otp_input_textfield_view =
      otp_input_view_->AddChildView(std::make_unique<views::BoxLayoutView>());
  otp_input_textfield_view->SetOrientation(
      views::BoxLayout::Orientation::kVertical);

  // Adds textfield.
  otp_input_textfield_ = otp_input_textfield_view->AddChildView(
      std::make_unique<views::Textfield>());
  otp_input_textfield_->SetPlaceholderText(
      controller_ ? controller_->GetTextfieldPlaceholderText() : u"");
  otp_input_textfield_->SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  otp_input_textfield_->SetController(this);

  // Adds textfield invalid label. The invalid label is initially not visible.
  otp_input_textfield_invalid_label_ =
      otp_input_textfield_view->AddChildView(std::make_unique<views::Label>(
          std::u16string(), ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
          STYLE_RED));
  otp_input_textfield_invalid_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  otp_input_textfield_invalid_label_->SetVisible(false);
  otp_input_textfield_invalid_label_->SetMultiLine(true);
  // Disable the label so that its accessibility data will not be read.
  otp_input_textfield_invalid_label_->SetEnabled(false);

  // Adds padding between the textfield and footer text while textfield label
  // is not visible, so that the initial dialog layout allows room for the error
  // label to appear if necessary. The padding is initially visible.
  otp_input_textfield_invalid_label_padding_ =
      otp_input_textfield_view->AddChildView(std::make_unique<views::View>());
  otp_input_textfield_invalid_label_padding_->SetPreferredSize(
      otp_input_textfield_invalid_label_->GetPreferredSize(
          views::SizeBounds(otp_input_textfield_invalid_label_->width(), {})));

  // Adds footer.
  footer_label_ =
      otp_input_view_->AddChildView(std::make_unique<views::StyledLabel>());
  SetDialogFooter(/*enabled=*/true);
}

void CardUnmaskOtpInputDialogViews::OnNewCodeLinkClicked() {
  if (controller_) {
    controller_->OnNewCodeLinkClicked();
  }
  SetDialogFooter(/*enabled=*/false);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CardUnmaskOtpInputDialogViews::EnableNewCodeLink,
                     weak_ptr_factory_.GetWeakPtr()),
      kNewOtpCodeLinkDisabledDuration);
}

void CardUnmaskOtpInputDialogViews::EnableNewCodeLink() {
  SetDialogFooter(/*enabled=*/true);

  if (closure_to_run_after_new_code_link_is_enabled_for_testing_) {
    closure_to_run_after_new_code_link_is_enabled_for_testing_.Run();
  }
}

void CardUnmaskOtpInputDialogViews::CreateHiddenProgressView() {
  progress_view_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  progress_view_->SetOrientation(views::BoxLayout::Orientation::kVertical);

  // Set initial creation of |progress_view_| to not visible as it should only
  // be visible after the user submits an OTP.
  progress_view_->SetVisible(false);

  // Adds progress throbber.
  auto* throbber_view =
      progress_view_->AddChildView(std::make_unique<views::BoxLayoutView>());
  throbber_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  throbber_view->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  progress_throbber_ =
      throbber_view->AddChildView(std::make_unique<views::Throbber>());

  // Adds label under progress throbber.
  progress_label_ = progress_view_->AddChildView(
      views::Builder<views::Label>()
          .SetText(controller_ ? controller_->GetProgressLabel() : u"")
          .SetMultiLine(true)
          .SetEnabledColorId(ui::kColorThrobber)
          .Build());
}

void CardUnmaskOtpInputDialogViews::HideInvalidState() {
  DCHECK(otp_input_textfield_->GetInvalid());
  otp_input_textfield_->SetInvalid(false);
  otp_input_textfield_invalid_label_->SetText(std::u16string());
  otp_input_textfield_invalid_label_->SetVisible(false);
  otp_input_textfield_->GetViewAccessibility().SetName(
      *otp_input_textfield_invalid_label_);
  otp_input_textfield_invalid_label_padding_->SetVisible(true);
}

void CardUnmaskOtpInputDialogViews::CloseWidget(bool user_closed_dialog,
                                                bool server_request_succeeded) {
  if (controller_) {
    controller_->OnDialogClosed(user_closed_dialog, server_request_succeeded);
    controller_ = nullptr;
  }
  GetWidget()->Close();
}

void CardUnmaskOtpInputDialogViews::SetDialogFooter(bool enabled) {
  const std::u16string link_text =
      controller_ ? controller_->GetNewCodeLinkText() : u"";
  const FooterText footer_text =
      controller_ ? controller_->GetFooterText(link_text) : FooterText();
  footer_label_->SetEnabled(enabled);
  footer_label_->SetText(footer_text.text);
  footer_label_->ClearStyleRanges();
  views::StyledLabel::RangeStyleInfo style_info;
  if (enabled) {
    style_info =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &CardUnmaskOtpInputDialogViews::OnNewCodeLinkClicked,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    style_info.text_style = views::style::STYLE_DISABLED;
  }
  footer_label_->AddStyleRange(
      gfx::Range(footer_text.link_offset_in_text,
                 footer_text.link_offset_in_text + link_text.length()),
      style_info);
  footer_label_->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
}

base::WeakPtr<CardUnmaskOtpInputDialogView> CreateAndShowOtpInputDialog(
    base::WeakPtr<CardUnmaskOtpInputDialogController> controller,
    content::WebContents* web_contents) {
  CardUnmaskOtpInputDialogViews* dialog_view =
      new CardUnmaskOtpInputDialogViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  return dialog_view->GetWeakPtr();
}

}  // namespace autofill
