// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_
#define COMPONENTS_PAYMENTS_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_

#include <map>
#include <memory>

#include "base/macros.h"
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

  // Retrieves the instance of PaymentRequestWebContentsManager that was
  // attached to the specified WebContents.  If no instance was attached,
  // creates one, and attaches it to the specified WebContents.
  static PaymentRequestWebContentsManager* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Creates the PaymentRequest that will interact with this |render_frame_host|
  // and the associated |web_contents|.
  void CreatePaymentRequest(
      content::RenderFrameHost* render_frame_host,
      content::WebContents* web_contents,
      std::unique_ptr<ContentPaymentRequestDelegate> delegate,
      mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
      PaymentRequest::ObserverForTest* observer_for_testing);

  // Destroys the given |request|.
  void DestroyRequest(PaymentRequest* request);

  // WebContentsObserver::
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit PaymentRequestWebContentsManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PaymentRequestWebContentsManager>;
  friend class PaymentRequestBrowserTestBase;

  // Owns all the PaymentRequest for this WebContents. Since the
  // PaymentRequestWebContentsManager's lifetime is tied to the WebContents,
  // these requests only get destroyed when the WebContents goes away, or when
  // the requests themselves call DestroyRequest().
  std::map<PaymentRequest*, std::unique_ptr<PaymentRequest>> payment_requests_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestWebContentsManager);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_
