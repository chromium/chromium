// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEW_H_

namespace content {
class WebContents;
}

namespace autofill {

class CardUnmaskOtpInputDialogController;

// Interface that exposes the view to CardUnmaskOtpInputDialogControllerImpl.
class CardUnmaskOtpInputDialogView {
 public:
  virtual ~CardUnmaskOtpInputDialogView() = default;
  // Creates a dialog and displays it as a modal on top of the web contents of
  // CardUnmaskOtpInputDialogController.
  static CardUnmaskOtpInputDialogView* CreateAndShow(
      CardUnmaskOtpInputDialogController* controller,
      content::WebContents* web_contents);

  // Pending state is shown once the user submits a valid OTP.
  virtual void ShowPendingState() = 0;

  // Show an error message when OTP verification fails.
  virtual void ShowErrorMessage(std::u16string error_message) = 0;

  // Method to safely close this dialog when the controller is destroyed by
  // unlinking the controller and closing the widget that owns this dialog.
  virtual void OnControllerDestroying() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEW_H_
