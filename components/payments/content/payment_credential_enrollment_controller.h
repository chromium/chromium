// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_CONTROLLER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_credential_enrollment_model.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SkBitmap;

namespace payments {

class PaymentCredentialEnrollmentView;

// Controls the user interface in the secure payment confirmation flow.
class PaymentCredentialEnrollmentController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          PaymentCredentialEnrollmentController> {
 public:
  using ResponseCallback = base::OnceCallback<void(bool user_confirm_from_ui)>;

  // Only one of these tokens can be given out at a time.
  class ScopedToken {
   public:
    ScopedToken();
    ~ScopedToken();

    ScopedToken(const ScopedToken& other) = delete;
    ScopedToken& operator=(const ScopedToken& other) = delete;

    base::WeakPtr<ScopedToken> GetWeakPtr();

   private:
    base::WeakPtrFactory<ScopedToken> weak_ptr_factory_{this};
  };

  class ObserverForTest {
   public:
    virtual void OnDialogOpened() = 0;
  };

  // Returns the object owned by the given contents, creating it if necessary.
  static PaymentCredentialEnrollmentController* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  explicit PaymentCredentialEnrollmentController(
      content::WebContents* web_contents);
  ~PaymentCredentialEnrollmentController() override;

  PaymentCredentialEnrollmentController(
      const PaymentCredentialEnrollmentController& other) = delete;
  PaymentCredentialEnrollmentController& operator=(
      const PaymentCredentialEnrollmentController& other) = delete;

  void ShowDialog(content::GlobalFrameRoutingId initiator_frame_routing_id,
                  std::unique_ptr<SkBitmap> instrument_icon,
                  const std::u16string& instrument_name,
                  ResponseCallback response_callback);
  void CloseDialog();
  void ShowProcessingSpinner();

  // Dialog callbacks.
  void OnCancel();
  void OnConfirm();

  void set_observer_for_test(ObserverForTest* observer_for_test) {
    observer_for_test_ = observer_for_test;
  }

  // Returns a new token or nullptr if a token is not available to give out. The
  // next token cannot be given out until the previous one has been destroyed.
  // Used for ensuring only one enrollment at a time takes place in a
  // WebContents.
  std::unique_ptr<ScopedToken> GetTokenIfAvailable();

  base::WeakPtr<PaymentCredentialEnrollmentController> GetWeakPtr();

 private:
  friend class content::WebContentsUserData<
      PaymentCredentialEnrollmentController>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  void RecordFirstCloseReason(
      SecurePaymentConfirmationEnrollDialogResult result);

  content::GlobalFrameRoutingId initiator_frame_routing_id_;

  ResponseCallback response_callback_;

  PaymentCredentialEnrollmentModel model_;

  // On desktop, the PaymentCredentialEnrollmentView object is memory managed by
  // the views:: machinery. It is deleted when the window is closed and
  // views::DialogDelegateView::DeleteDelegate() is called by its corresponding
  // views::Widget.
  base::WeakPtr<PaymentCredentialEnrollmentView> view_;

  ObserverForTest* observer_for_test_ = nullptr;
  base::WeakPtr<ScopedToken> token_;

  bool is_user_response_recorded_ = false;

  base::WeakPtrFactory<PaymentCredentialEnrollmentController> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_CONTROLLER_H_
