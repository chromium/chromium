// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"

#include <string>

#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#include "components/autofill/core/browser/payments/otp_unmask_result.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

CardUnmaskOtpInputDialogControllerImpl::CardUnmaskOtpInputDialogControllerImpl(
    const CardUnmaskChallengeOption& challenge_option,
    base::WeakPtr<OtpUnmaskDelegate> delegate)
    : challenge_type_(challenge_option.type),
      otp_length_(challenge_option.challenge_input_length),
      delegate_(delegate) {}

CardUnmaskOtpInputDialogControllerImpl::
    ~CardUnmaskOtpInputDialogControllerImpl() {
  // This part of code is executed only if the browser window is closed when the
  // dialog is visible. In this case the controller is destroyed before
  // CardUnmaskOtpInputDialogViews::dtor() is called, but the reference to
  // controller is not reset. This resets the reference via
  // CardUnmaskOtpInputDialogView::Dismiss() to avoid
  // a crash.
  if (dialog_view_) {
    dialog_view_->Dismiss(/*show_confirmation_before_closing=*/false,
                          /*user_closed_dialog=*/true);
  }
}

void CardUnmaskOtpInputDialogControllerImpl::ShowDialog(
    base::OnceCallback<base::WeakPtr<CardUnmaskOtpInputDialogView>()>
        create_and_show_view_callback) {
  if (dialog_view_) {
    return;
  }

  dialog_view_ = std::move(create_and_show_view_callback).Run();
  if (dialog_view_) {
    autofill_metrics::LogOtpInputDialogShown(challenge_type_);
  }
}

void CardUnmaskOtpInputDialogControllerImpl::OnOtpVerificationResult(
    OtpUnmaskResult result) {
  // This can be invoked when the dialog is not visible. In this case we do
  // nothing.
  if (!dialog_view_) {
    return;
  }

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
      temporary_error_shown_ = true;
      autofill_metrics::LogOtpInputDialogErrorMessageShown(
          result == OtpUnmaskResult::kOtpMismatch
              ? autofill_metrics::OtpInputDialogError::kOtpMismatchError
              : autofill_metrics::OtpInputDialogError::kOtpExpiredError,
          challenge_type_);
      ShowInvalidState(result);
      break;
    case OtpUnmaskResult::kUnknownType:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void CardUnmaskOtpInputDialogControllerImpl::OnDialogClosed(
    bool user_closed_dialog,
    bool server_request_succeeded) {
  if (delegate_) {
    delegate_->OnUnmaskPromptClosed(user_closed_dialog);
  }

  if (user_closed_dialog) {
    autofill_metrics::LogOtpInputDialogResult(
        ok_button_clicked_ ? autofill_metrics::OtpInputDialogResult::
                                 kDialogCancelledByUserAfterConfirmation
                           : autofill_metrics::OtpInputDialogResult::
                                 kDialogCancelledByUserBeforeConfirmation,
        temporary_error_shown_, challenge_type_);
  } else if (server_request_succeeded) {
    autofill_metrics::LogOtpInputDialogResult(
        autofill_metrics::OtpInputDialogResult::
            kDialogClosedAfterVerificationSucceeded,
        temporary_error_shown_, challenge_type_);
  } else {
    autofill_metrics::LogOtpInputDialogResult(
        autofill_metrics::OtpInputDialogResult::
            kDialogClosedAfterVerificationFailed,
        temporary_error_shown_, challenge_type_);
  }

  // Resets the variables to their initial states since the controller will stay
  // even if the view is gone.
  dialog_view_ = nullptr;
  temporary_error_shown_ = false;
  ok_button_clicked_ = false;
}

void CardUnmaskOtpInputDialogControllerImpl::OnOkButtonClicked(
    const std::u16string& otp) {
  ok_button_clicked_ = true;
  if (delegate_) {
    delegate_->OnUnmaskPromptAccepted(otp);
  }
}

void CardUnmaskOtpInputDialogControllerImpl::OnNewCodeLinkClicked() {
  if (delegate_) {
    delegate_->OnNewOtpRequested();
  }

  autofill_metrics::LogOtpInputDialogNewOtpRequested(challenge_type_);
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

#if BUILDFLAG(IS_ANDROID)
int CardUnmaskOtpInputDialogControllerImpl::GetExpectedOtpLength() const {
  return otp_length_;
}
#endif  // BUILDFLAG(IS_ANDROID)

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

base::WeakPtr<CardUnmaskOtpInputDialogController>
CardUnmaskOtpInputDialogControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if BUILDFLAG(IS_IOS)
base::WeakPtr<CardUnmaskOtpInputDialogControllerImpl>
CardUnmaskOtpInputDialogControllerImpl::GetImplWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
#endif

void CardUnmaskOtpInputDialogControllerImpl::ShowInvalidState(
    OtpUnmaskResult otp_unmask_result) {
  if (!dialog_view_) {
    return;
  }

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
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace autofill
