// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_IMPL_H_

#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller.h"

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"

namespace autofill {

enum class OtpUnmaskResult;
class OtpUnmaskDelegate;
class CardUnmaskOtpInputDialogView;

class CardUnmaskOtpInputDialogControllerImpl
    : public CardUnmaskOtpInputDialogController {
 public:
  CardUnmaskOtpInputDialogControllerImpl(
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate);
  CardUnmaskOtpInputDialogControllerImpl(
      const CardUnmaskOtpInputDialogControllerImpl&) = delete;
  CardUnmaskOtpInputDialogControllerImpl& operator=(
      const CardUnmaskOtpInputDialogControllerImpl&) = delete;
  ~CardUnmaskOtpInputDialogControllerImpl() override;

  // Show the dialog for users to type in OTPs.
  void ShowDialog(
      base::OnceCallback<base::WeakPtr<CardUnmaskOtpInputDialogView>()>
          create_and_show_view_callback);

  // Invoked when the OTP verification is completed.
  void OnOtpVerificationResult(OtpUnmaskResult result);

  // CardUnmaskOtpInputDialogController:
  void OnDialogClosed(bool user_closed_dialog,
                      bool server_request_succeeded) override;
  void OnOkButtonClicked(const std::u16string& otp) override;
  void OnNewCodeLinkClicked() override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetTextfieldPlaceholderText() const override;
#if BUILDFLAG(IS_ANDROID)
  int GetExpectedOtpLength() const override;
#endif
  bool IsValidOtp(const std::u16string& otp) const override;
  FooterText GetFooterText(const std::u16string& link_text) const override;
  std::u16string GetNewCodeLinkText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetProgressLabel() const override;
  std::u16string GetConfirmationMessage() const override;
  base::WeakPtr<CardUnmaskOtpInputDialogController> GetWeakPtr() override;

#if BUILDFLAG(IS_IOS)
  base::WeakPtr<CardUnmaskOtpInputDialogControllerImpl> GetImplWeakPtr();
#endif

#if defined(UNIT_TEST)
  base::WeakPtr<CardUnmaskOtpInputDialogView> GetDialogViewForTesting() {
    return dialog_view_;
  }
#endif

 protected:
  base::WeakPtr<CardUnmaskOtpInputDialogView> dialog_view_;

  // The challenge type of the OTP input dialog.
  const CardUnmaskChallengeOptionType challenge_type_;

 private:
  // Sets the view's state to the invalid state for the corresponding
  // |otp_unmask_result|.
  void ShowInvalidState(OtpUnmaskResult otp_unmask_result);

  // The length of the OTP expected to be entered by the user.
  const size_t otp_length_;

  // Weak reference to the delegate. Used to handle events of the dialog.
  base::WeakPtr<OtpUnmaskDelegate> delegate_;

  // Indicates whether any temporary error has been shown on the dialog. Used
  // for logging.
  bool temporary_error_shown_ = false;

  // Indicates whether the OK button in the dialog has been clicked. Used for
  // logging.
  bool ok_button_clicked_ = false;

  base::WeakPtrFactory<CardUnmaskOtpInputDialogControllerImpl>
      weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_IMPL_H_
