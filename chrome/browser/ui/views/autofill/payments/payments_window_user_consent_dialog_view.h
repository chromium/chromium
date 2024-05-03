// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog.h"
#include "components/autofill/core/browser/ui/payments/payments_window_user_consent_dialog_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill::payments {

class PaymentsWindowUserConsentDialogController;

// The Desktop native views implementation for PaymentsWindowUserConsentDialog.
// This is owned by the view hierarchy.
class PaymentsWindowUserConsentDialogView
    : public PaymentsWindowUserConsentDialog,
      public views::DialogDelegateView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopViewId);

  explicit PaymentsWindowUserConsentDialogView(
      base::WeakPtr<PaymentsWindowUserConsentDialogController> controller);
  PaymentsWindowUserConsentDialogView(
      const PaymentsWindowUserConsentDialogView&) = delete;
  PaymentsWindowUserConsentDialogView& operator=(
      const PaymentsWindowUserConsentDialogView&) = delete;
  ~PaymentsWindowUserConsentDialogView() override;

  // PaymentsWindowUserConsentDialog:
  void Dismiss() override;
  base::WeakPtr<PaymentsWindowUserConsentDialog> GetWeakPtr() override;

  // DialogDelegate:
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;

  void OnDialogClosing();

 private:
  // The controller that handles view interaction, as well as contains the
  // information for what to display in the view.
  base::WeakPtr<PaymentsWindowUserConsentDialogController>
      payments_window_user_consent_dialog_controller_;

  base::WeakPtrFactory<PaymentsWindowUserConsentDialogView> weak_ptr_factory_{
      this};
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_WINDOW_USER_CONSENT_DIALOG_VIEW_H_
