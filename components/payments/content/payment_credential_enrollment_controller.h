// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_CONTROLLER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_credential_enrollment_model.h"
#include "components/payments/content/payment_credential_enrollment_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace payments {

// Controls the user interface in the secure payment confirmation flow.
class PaymentCredentialEnrollmentController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          PaymentCredentialEnrollmentController> {
 public:
  using AcceptCallback = base::OnceCallback<void()>;
  using CancelCallback = base::OnceCallback<void()>;

  PaymentCredentialEnrollmentController(content::WebContents* web_contents,
                                        AcceptCallback accept_callback,
                                        CancelCallback cancel_callback);
  ~PaymentCredentialEnrollmentController() override;

  PaymentCredentialEnrollmentController(
      const PaymentCredentialEnrollmentController& other) = delete;
  PaymentCredentialEnrollmentController& operator=(
      const PaymentCredentialEnrollmentController& other) = delete;

  void ShowDialog();
  void CloseDialog();
  void ShowProcessingSpinner();

  // Dialog callbacks.
  void OnCancel();
  void OnConfirm();

  base::WeakPtr<PaymentCredentialEnrollmentController> GetWeakPtr();

 private:
  friend class content::WebContentsUserData<
      PaymentCredentialEnrollmentController>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  AcceptCallback accept_callback_;
  CancelCallback cancel_callback_;

  PaymentCredentialEnrollmentModel model_;

  // On desktop, the PaymentCredentialEnrollmentView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  base::WeakPtr<PaymentCredentialEnrollmentView> view_;

  base::WeakPtrFactory<PaymentCredentialEnrollmentController> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_CONTROLLER_H_
