// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential_manager.h"

#include <utility>

#include "base/check.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace payments {

PaymentCredentialManager::~PaymentCredentialManager() = default;

PaymentCredentialManager* PaymentCredentialManager::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // CreateForWebContents does nothing if the manager instance already exists.
  PaymentCredentialManager::CreateForWebContents(web_contents);
  return PaymentCredentialManager::FromWebContents(web_contents);
}

void PaymentCredentialManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  payment_credential_ = nullptr;
}

void PaymentCredentialManager::CreatePaymentCredential(
    content::GlobalRenderFrameHostId initiator_frame_routing_id,
    scoped_refptr<PaymentManifestWebDataService> web_data_sevice,
    mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver) {
  payment_credential_ = std::make_unique<PaymentCredential>(
      web_contents(), initiator_frame_routing_id, web_data_sevice,
      std::move(receiver));
}

PaymentCredentialManager::PaymentCredentialManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentCredentialManager);

}  // namespace payments
