// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_

#include "components/payments/content/payment_request.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace payments {

// Tracks information related to PaymentRequest calls that needs to outlive
// individual PaymentRequest objects.
class PaymentRequestWebContentsManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PaymentRequestWebContentsManager> {
 public:
  // Retrieves the instance of PaymentRequestWebContentsManager that was
  // attached to the specified WebContents.  If no instance was attached,
  // creates one, and attaches it to the specified WebContents.
  static PaymentRequestWebContentsManager* GetOrCreateForWebContents(
      content::WebContents& web_contents);

  ~PaymentRequestWebContentsManager() override;

  PaymentRequestWebContentsManager(const PaymentRequestWebContentsManager&) =
      delete;
  PaymentRequestWebContentsManager& operator=(
      const PaymentRequestWebContentsManager&) = delete;

  bool HadActivationlessShow() const { return had_activationless_show_; }
  void RecordActivationlessShow();

  void SetSPCTransactionMode(SPCTransactionMode mode) {
    transaction_mode_ = mode;
  }
  SPCTransactionMode transaction_mode() const { return transaction_mode_; }

  // WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<PaymentRequestWebContentsManager>;

  explicit PaymentRequestWebContentsManager(content::WebContents* web_contents);

  // Whether there has been a call to PaymentRequest::Show since the last
  // user-initiated navigation. Used to suppress multiple activationless show
  // calls.
  bool had_activationless_show_ = false;

  // The current transaction automation mode for Secure Payment Confirmation, to
  // be used for any future PaymentRequests.
  SPCTransactionMode transaction_mode_ = SPCTransactionMode::NONE;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_
