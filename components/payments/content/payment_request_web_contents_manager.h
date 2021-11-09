// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_

#include <map>
#include <memory>

#include "components/payments/content/payment_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace content {
class RenderFrameHost;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace payments {

class ContentPaymentRequestDelegate;

enum class SPCTransactionMode {
  NONE,
  AUTOACCEPT,
  AUTOREJECT,
};

// This class owns the PaymentRequest associated with a given WebContents.
//
// Responsible for creating PaymentRequest's and retaining ownership. No request
// pointers are currently available because the request manages its interactions
// with UI and renderer. The PaymentRequest may call DestroyRequest() to signal
// it is ready to die. Otherwise it gets destroyed when the WebContents (thus
// this class) goes away.
class PaymentRequestWebContentsManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PaymentRequestWebContentsManager> {
 public:
  ~PaymentRequestWebContentsManager() override;
  PaymentRequestWebContentsManager(const PaymentRequestWebContentsManager&) =
      delete;
  PaymentRequestWebContentsManager& operator=(
      const PaymentRequestWebContentsManager&) = delete;

  // Retrieves the instance of PaymentRequestWebContentsManager that was
  // attached to the specified WebContents.  If no instance was attached,
  // creates one, and attaches it to the specified WebContents.
  static PaymentRequestWebContentsManager* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Creates the PaymentRequest that will interact with this `render_frame_host`
  // and the associated `web_contents`.
  void CreatePaymentRequest(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<ContentPaymentRequestDelegate> delegate,
      mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
      base::WeakPtr<PaymentRequest::ObserverForTest> observer_for_testing);

  // Destroys the given `request`.
  void DestroyRequest(base::WeakPtr<PaymentRequest> request);

  // WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // For test automation purposes, set the 'current transaction automation
  // mode' for Secure Payment Confirmation. See
  // https://w3c.github.io/secure-payment-confirmation/#sctn-automation-set-spc-transaction-mode
  void SetSPCTransactionMode(SPCTransactionMode mode);

  base::WeakPtr<PaymentRequestWebContentsManager> GetWeakPtr();

  // A testing-only version of |CreatePaymentRequest| that also returns the
  // created PaymentRequest.
  PaymentRequest* CreateAndReturnPaymentRequestForTesting(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<ContentPaymentRequestDelegate> delegate,
      mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
      base::WeakPtr<PaymentRequest::ObserverForTest> observer_for_testing);

 private:
  explicit PaymentRequestWebContentsManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PaymentRequestWebContentsManager>;
  friend class PaymentRequestBrowserTestBase;

  // Internal implementation of CreatePaymentRequest, which returns the created
  // PaymentRequest. As per the class-header comments, the public API of this
  // class does not give out request pointers because PaymentRequest manages
  // its own interactions. This internal API exists to support testing.
  PaymentRequest* CreatePaymentRequestInternal(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<ContentPaymentRequestDelegate> delegate,
      mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
      base::WeakPtr<PaymentRequest::ObserverForTest> observer_for_testing);

  // Owns all the PaymentRequest for this WebContents. Since the
  // PaymentRequestWebContentsManager's lifetime is tied to the WebContents,
  // these requests only get destroyed when the WebContents goes away, or when
  // the requests themselves call DestroyRequest().
  std::map<PaymentRequest*, std::unique_ptr<PaymentRequest>> payment_requests_;

  // The current transaction automation mode for Secure Payment Confirmation.
  // Used in automated testing.
  SPCTransactionMode spc_transaction_mode_;

  base::WeakPtrFactory<PaymentRequestWebContentsManager> weak_ptr_factory_{
      this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_
