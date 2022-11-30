// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_view.h"
#include "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

enum class OtpUnmaskResult;

class CardUnmaskOtpInputDialogControllerImpl
    : public CardUnmaskOtpInputDialogController,
      public content::WebContentsUserData<
          CardUnmaskOtpInputDialogControllerImpl> {
 public:
  CardUnmaskOtpInputDialogControllerImpl(
      const CardUnmaskOtpInputDialogControllerImpl&) = delete;
  CardUnmaskOtpInputDialogControllerImpl& operator=(
      const CardUnmaskOtpInputDialogControllerImpl&) = delete;
  ~CardUnmaskOtpInputDialogControllerImpl() override;

  // Show the dialog for users to type in OTPs.
  void ShowDialog(size_t otp_length, base::WeakPtr<OtpUnmaskDelegate> delegate);

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

#if defined(UNIT_TEST)
  CardUnmaskOtpInputDialogView* GetDialogViewForTesting() {
    return dialog_view_;
  }
#endif

 protected:
  explicit CardUnmaskOtpInputDialogControllerImpl(
      content::WebContents* web_contents);

  raw_ptr<CardUnmaskOtpInputDialogView> dialog_view_ = nullptr;

 private:
  friend class content::WebContentsUserData<
      CardUnmaskOtpInputDialogControllerImpl>;

  // Sets the view's state to the invalid state for the corresponding
  // |otp_unmask_result|.
  void ShowInvalidState(OtpUnmaskResult otp_unmask_result);

  // The length of the OTP expected to be entered by the user.
  size_t otp_length_;

  // Weak reference to the delegate. Used to handle events of the dialog.
  base::WeakPtr<OtpUnmaskDelegate> delegate_;

  // Indicates whether any temporary error has been shown on the dialog. Used
  // for logging.
  bool temporary_error_shown_ = false;

  // Indicates whether the OK button in the dialog has been clicked. Used for
  // logging.
  bool ok_button_clicked_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_IMPL_H_
