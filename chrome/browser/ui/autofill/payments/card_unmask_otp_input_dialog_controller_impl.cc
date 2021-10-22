// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller_impl.h"

#include <string>

#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_view.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardUnmaskOtpInputDialogControllerImpl::
    ~CardUnmaskOtpInputDialogControllerImpl() {
  // This part of code is executed only if the browser window is closed when the
  // dialog is visible. In this case the controller is destroyed before
  // CardUnmaskOtpInputDialogViews::dtor() is called, but the reference to
  // controller is not reset. This resets the reference via
  // CardUnmaskOtpInputDialogView::Dismiss() to avoid
  // a crash.
  if (dialog_view_)
    dialog_view_->Dismiss(/*show_confirmation_before_closing=*/false,
                          /*user_closed_dialog=*/true);
}

void CardUnmaskOtpInputDialogControllerImpl::ShowDialog(
    size_t otp_length,
    base::WeakPtr<OtpUnmaskDelegate> delegate) {
  if (dialog_view_)
    return;

  otp_length_ = otp_length;
  delegate_ = delegate;
  dialog_view_ =
      CardUnmaskOtpInputDialogView::CreateAndShow(this, web_contents());
}

void CardUnmaskOtpInputDialogControllerImpl::OnOtpVerificationResult(
    OtpUnmaskResult result) {
  switch (result) {
    case OtpUnmaskResult::kSuccess:
      dialog_view_->Dismiss(/*show_confirmation_before_closing=*/true,
                            /*user_closed_dialog=*/false);
      break;
    case OtpUnmaskResult::kPermanentFailure:
      dialog_view_->Dismiss(/*show_confirmation_before_closing=*/false,
                            /*user_closed_dialog=*/false);
      break;
    case OtpUnmaskResult::kOtpExpired:
    case OtpUnmaskResult::kOtpMismatch:
      ShowInvalidState(result);
      break;
    case OtpUnmaskResult::kUnknownType:
      NOTREACHED();
      break;
  }
}

void CardUnmaskOtpInputDialogControllerImpl::OnDialogClosed(
    bool user_closed_dialog) {
  if (delegate_)
    delegate_->OnUnmaskPromptClosed(user_closed_dialog);

  dialog_view_ = nullptr;
}

void CardUnmaskOtpInputDialogControllerImpl::OnOkButtonClicked(
    const std::u16string& otp) {
  if (delegate_)
    delegate_->OnUnmaskPromptAccepted(otp);
}

void CardUnmaskOtpInputDialogControllerImpl::OnNewCodeLinkClicked() {
  if (delegate_)
    delegate_->OnNewOtpRequested();
}

std::u16string CardUnmaskOtpInputDialogControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_TITLE);
}

std::u16string
CardUnmaskOtpInputDialogControllerImpl::GetTextfieldPlaceholderText() const {
  return l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_TEXTFIELD_PLACEHOLDER_MESSAGE,
      base::NumberToString16(otp_length_));
}

#if defined(OS_ANDROID)
int CardUnmaskOtpInputDialogControllerImpl::GetExpectedOtpLength() const {
  return otp_length_;
}
#endif  // OS_ANDROID

bool CardUnmaskOtpInputDialogControllerImpl::IsValidOtp(
    const std::u16string& otp) const {
  return otp.length() == otp_length_ &&
         base::ContainsOnlyChars(otp,
                                 /*characters=*/u"0123456789");
}

FooterText CardUnmaskOtpInputDialogControllerImpl::GetFooterText(
    const std::u16string& link_text) const {
  size_t link_offset;
  std::u16string text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_FOOTER_MESSAGE, link_text,
      &link_offset);
  return {text, link_offset};
}

std::u16string CardUnmaskOtpInputDialogControllerImpl::GetNewCodeLinkText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_NEW_CODE_MESSAGE);
}

std::u16string CardUnmaskOtpInputDialogControllerImpl::GetOkButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_CARD_UNMASK_CONFIRM_BUTTON);
}

std::u16string CardUnmaskOtpInputDialogControllerImpl::GetProgressLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_PENDING_MESSAGE);
}

std::u16string CardUnmaskOtpInputDialogControllerImpl::GetConfirmationMessage()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_UNMASK_VERIFICATION_SUCCESS);
}

#if defined(UNIT_TEST)
CardUnmaskOtpInputDialogView*
CardUnmaskOtpInputDialogControllerImpl::GetDialogViewForTesting() {
  return dialog_view_;
}
#endif

CardUnmaskOtpInputDialogControllerImpl::CardUnmaskOtpInputDialogControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void CardUnmaskOtpInputDialogControllerImpl::ShowInvalidState(
    OtpUnmaskResult otp_unmask_result) {
  if (!dialog_view_)
    return;

  switch (otp_unmask_result) {
    case OtpUnmaskResult::kOtpExpired:
      dialog_view_->ShowInvalidState(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_VERIFICATION_CODE_EXPIRED_LABEL));
      break;
    case OtpUnmaskResult::kOtpMismatch:
      dialog_view_->ShowInvalidState(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_OTP_INPUT_DIALOG_ENTER_CORRECT_CODE_LABEL));
      break;
    case OtpUnmaskResult::kSuccess:
    case OtpUnmaskResult::kPermanentFailure:
    case OtpUnmaskResult::kUnknownType:
      NOTREACHED();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CardUnmaskOtpInputDialogControllerImpl);

}  // namespace autofill
