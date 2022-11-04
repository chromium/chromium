// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CONTROLLER_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/payments/content/initialization_task.h"
#include "components/payments/content/payment_request_dialog.h"
#include "components/payments/content/secure_payment_confirmation_model.h"
#include "components/payments/content/secure_payment_confirmation_view.h"

namespace payments {

class PaymentRequest;

// Controls the user interface in the secure payment confirmation flow.
class SecurePaymentConfirmationController
    : public PaymentRequestDialog,
      public InitializationTask::Observer {
 public:
  explicit SecurePaymentConfirmationController(
      base::WeakPtr<PaymentRequest> request);
  ~SecurePaymentConfirmationController() override;

  SecurePaymentConfirmationController(
      const SecurePaymentConfirmationController& other) = delete;
  SecurePaymentConfirmationController& operator=(
      const SecurePaymentConfirmationController& other) = delete;

  // PaymentRequestDialog:
  void ShowDialog() override;
  void RetryDialog() override;
  void CloseDialog() override;
  void ShowErrorMessage() override;
  void ShowProcessingSpinner() override;
  bool IsInteractive() const override;
  void ShowPaymentHandlerScreen(
      const GURL& url,
      PaymentHandlerOpenWindowCallback callback) override;
  void ConfirmPaymentForTesting() override;
  bool ClickOptOutForTesting() override;

  // InitializationTask::Observer:
  void OnInitialized(InitializationTask* initialization_task) override;

  // Callbacks for user interaction.
  void OnCancel();
  void OnConfirm();
  void OnOptOut();

  base::WeakPtr<SecurePaymentConfirmationController> GetWeakPtr();

 private:
  void SetupModelAndShowDialogIfApplicable();

  // Can be null when the webpage closes or the iframe refreshes or navigates.
  base::WeakPtr<PaymentRequest> request_;

  SecurePaymentConfirmationModel model_;

  // On desktop, the SecurePaymentConfirmationView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  base::WeakPtr<SecurePaymentConfirmationView> view_;

  int number_of_initialization_tasks_ = 0;

  base::WeakPtrFactory<SecurePaymentConfirmationController> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CONTROLLER_H_
