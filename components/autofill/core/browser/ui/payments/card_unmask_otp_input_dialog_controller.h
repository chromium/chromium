// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"

namespace autofill {

struct FooterText {
  std::u16string text;
  size_t link_offset_in_text;
};

// Interface that exposes controller functionality to
// CardUnmaskOtpInputDialogView.
class CardUnmaskOtpInputDialogController {
 public:
  virtual ~CardUnmaskOtpInputDialogController() = default;

  // Called whenever the dialog is closed, and it sets the |dialog_view_|
  // variable in this class to nullptr. |user_closed_dialog| indicates whether
  // the user closed the dialog and cancelled the flow.
  // |server_request_succeeded| indicates if the server call succeeded, this is
  // only meaningful if the dialog closure is not triggered by user
  // cancellation.
  virtual void OnDialogClosed(bool user_closed_dialog,
                              bool server_request_succeeded) = 0;

  // Invoked when the OK button of the dialog is clicked.
  virtual void OnOkButtonClicked(const std::u16string& otp) = 0;

  // Invoked when the "Get New Code" link is clicked.
  virtual void OnNewCodeLinkClicked() = 0;

  virtual std::u16string GetWindowTitle() const = 0;

  // The placeholder text for the textfield in the first state of this dialog.
  // This placeholder text lets the user know how many digits need to be entered
  // for the OTP, and it can change based on OTP length.
  virtual std::u16string GetTextfieldPlaceholderText() const = 0;

#if BUILDFLAG(IS_ANDROID)
  // The length of the OTP that the user is expected to fill into the text
  // field.
  virtual int GetExpectedOtpLength() const = 0;
#endif

  // Checks if the given text is a possible valid OTP before sending a request
  // to the backend to see if the otp is correct.
  virtual bool IsValidOtp(const std::u16string& otp) const = 0;

  // Gets the footer text for the first state of the dialog (input OTP state).
  // This function sets the text of the link in the footer to |link_text|. Then
  // it returns a FooterText struct containing the entire footer text and the
  // link offset in the text.
  virtual FooterText GetFooterText(const std::u16string& link_text) const = 0;

  virtual std::u16string GetNewCodeLinkText() const = 0;

  virtual std::u16string GetOkButtonLabel() const = 0;

  // The label directly under the throbber in the pending state of this dialog,
  // letting the user know that the the code is being verified.
  virtual std::u16string GetProgressLabel() const = 0;

  // The label shown when the OTP verification is completed.
  virtual std::u16string GetConfirmationMessage() const = 0;

  virtual base::WeakPtr<CardUnmaskOtpInputDialogController> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_CONTROLLER_H_
