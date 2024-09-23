// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_H_

#include <string>

#include "components/autofill/core/browser/metrics/payments/payments_window_metrics.h"

namespace autofill::payments {

// Interface for the controller that handles functionality related to the user
// consent dialog in a payments window pop-up flow.
class PaymentsWindowUserConsentDialogController {
 public:
  virtual ~PaymentsWindowUserConsentDialogController() = default;

  // Triggers the callback that continues the payments window pop-up flow.
  virtual void OnOkButtonClicked() = 0;

  // Triggers the callback that cancels the payments window pop-up flow.
  virtual void OnCancelButtonClicked() = 0;

  // Logs when the dialog is closing, and `result` indicates the closed reason.
  virtual void OnDialogClosing(
      autofill_metrics::PaymentsWindowUserConsentDialogResult result) = 0;

  // Returns the title of the payments window pop-up user consent dialog to be
  // displayed to the user.
  virtual std::u16string GetWindowTitle() const = 0;

  // Returns the description of the payments window pop-up user consent dialog
  // to be displayed to the user.
  virtual std::u16string GetDialogDescription() const = 0;

  // Returns the label for the accept button.
  virtual std::u16string GetOkButtonLabel() const = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_CONTROLLER_H_
