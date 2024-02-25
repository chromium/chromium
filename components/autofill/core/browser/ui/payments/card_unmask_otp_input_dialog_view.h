// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEW_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

// Interface that exposes the view to CardUnmaskOtpInputDialogControllerImpl.
class CardUnmaskOtpInputDialogView {
 public:
  virtual ~CardUnmaskOtpInputDialogView() = default;

  // Pending state is shown once the user submits a possible valid OTP.
  virtual void ShowPendingState() = 0;

  // Invalid state is shown if the user submits an incorrect or expired OTP. The
  // view will automatically hide invalid state once the contents of the
  // textfield are changed. Once the contents are changed and invalid state is
  // hidden, the OK button might still be disabled due to not being a valid OTP.
  virtual void ShowInvalidState(const std::u16string& invalid_label_text) = 0;

  // Method to safely close this dialog including the case where the controller
  // is destroyed first. |show_confirmation_before_closing| indicates whether we
  // should update the throbber to show a checkmark before closing the dialog.
  // |user_closed_dialog| indicates whether the dismissal was triggered by user
  // closing the dialog.
  virtual void Dismiss(bool show_confirmation_before_closing,
                       bool user_closed_dialog) = 0;

  virtual base::WeakPtr<CardUnmaskOtpInputDialogView> GetWeakPtr() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_OTP_INPUT_DIALOG_VIEW_H_
