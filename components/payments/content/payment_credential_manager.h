// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_MANAGER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_MANAGER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/payments/content/payment_credential.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace payments {

class PaymentManifestWebDataService;

class PaymentCredentialManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PaymentCredentialManager> {
 public:
  ~PaymentCredentialManager() override;
  PaymentCredentialManager(const PaymentCredentialManager&) = delete;
  PaymentCredentialManager& operator=(const PaymentCredentialManager&) = delete;

  static PaymentCredentialManager* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Creates the mojo IPC endpoint that will receive requests from the renderer
  // to store payment credential in user's profile.
  void CreatePaymentCredential(
      content::GlobalRenderFrameHostId initiator_frame_routing_id,
      scoped_refptr<PaymentManifestWebDataService> web_data_sevice,
      mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver);

  // WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  explicit PaymentCredentialManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PaymentCredentialManager>;

  std::unique_ptr<PaymentCredential> payment_credential_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_MANAGER_H_
