// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/card_unmask_otp_input_dialog_views.h"

#include <string>

#include "base/strings/strcat.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_provider.h"
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
    CardUnmaskOtpInputDialogController* controller)
    : controller_(controller) {
  SetShowTitle(true);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller_->GetOkButtonLabel());
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, false);
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL));
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kText));
  InitViews();
}

CardUnmaskOtpInputDialogViews::~CardUnmaskOtpInputDialogViews() {
  // Inform |controller_| of the dialog's destruction.
  if (controller_)
    controller_->OnDialogClosed();
}

// static
CardUnmaskOtpInputDialogView* CardUnmaskOtpInputDialogView::CreateAndShow(
    CardUnmaskOtpInputDialogController* controller,
    content::WebContents* web_contents) {
  CardUnmaskOtpInputDialogViews* dialog_view =
      new CardUnmaskOtpInputDialogViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  return dialog_view;
}

void CardUnmaskOtpInputDialogViews::ShowPendingState() {
  otp_input_view_->SetVisible(false);
  progress_view_->SetVisible(true);
  progress_throbber_->Start();
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, false);
}

void CardUnmaskOtpInputDialogViews::ShowErrorMessage(
    const std::u16string error_message) {
  // TODO(crbug.com/1196021): Show error message when OTP verification fails.
  NOTIMPLEMENTED();
}

void CardUnmaskOtpInputDialogViews::OnControllerDestroying() {
  controller_ = nullptr;
  GetWidget()->Close();
}

std::u16string CardUnmaskOtpInputDialogViews::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void CardUnmaskOtpInputDialogViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(
          GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

bool CardUnmaskOtpInputDialogViews::Accept() {
  ShowPendingState();
  return false;
}

void CardUnmaskOtpInputDialogViews::OnThemeChanged() {
  views::DialogDelegateView::OnThemeChanged();

  // We need to ensure |progress_label_|'s color matches the color of the
  // throbber above it.
  progress_label_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorThrobber));
}

void CardUnmaskOtpInputDialogViews::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  SetButtonEnabled(ui::DIALOG_BUTTON_OK,
                   /*enabled=*/controller_->IsValidOtp(new_contents));
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
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // Set initial creation of |otp_input_view_| to visible as it is the first
  // view shown in the dialog.
  otp_input_view_->SetVisible(true);

  // Adds textfield.
  otp_input_textfield_ =
      otp_input_view_->AddChildView(std::make_unique<views::Textfield>());
  otp_input_textfield_->SetPlaceholderText(
      controller_->GetTextfieldPlaceholderText());
  otp_input_textfield_->SetTextInputType(
      ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  otp_input_textfield_->SetController(this);

  // Adds footer.
  const std::u16string link_text = controller_->GetNewCodeLinkText();
  const FooterText footer_text = controller_->GetFooterText(link_text);
  auto footer_label = std::make_unique<views::StyledLabel>();
  footer_label->SetText(footer_text.text);
  footer_label->AddStyleRange(
      gfx::Range(footer_text.link_offset_in_text,
                 footer_text.link_offset_in_text + link_text.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          // TODO(crbug.com/1243475): Switch with correct callback for re-send
          // OTP once implemented.
          views::Link::ClickedCallback()));
  footer_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  otp_input_view_->AddChildView(std::move(footer_label));
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
      std::make_unique<views::Label>(controller_->GetProgressLabel()));
  progress_label_->SetMultiLine(true);
}

}  // namespace autofill
