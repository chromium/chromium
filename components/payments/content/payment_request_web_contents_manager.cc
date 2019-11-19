// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_web_contents_manager.h"

#include <utility>

#include "base/logging.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace payments {

PaymentRequestWebContentsManager::~PaymentRequestWebContentsManager() {}

PaymentRequestWebContentsManager*
PaymentRequestWebContentsManager::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // CreateForWebContents does nothing if the manager instance already exists.
  PaymentRequestWebContentsManager::CreateForWebContents(web_contents);
  return PaymentRequestWebContentsManager::FromWebContents(web_contents);
}

void PaymentRequestWebContentsManager::CreatePaymentRequest(
    content::RenderFrameHost* render_frame_host,
    content::WebContents* web_contents,
    std::unique_ptr<ContentPaymentRequestDelegate> delegate,
    mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
    PaymentRequest::ObserverForTest* observer_for_testing) {
  auto new_request = std::make_unique<PaymentRequest>(
      render_frame_host, web_contents, std::move(delegate), this,
      delegate->GetDisplayManager(), std::move(receiver), observer_for_testing);
  PaymentRequest* request_ptr = new_request.get();
  payment_requests_.insert(std::make_pair(request_ptr, std::move(new_request)));
}

void PaymentRequestWebContentsManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Navigations that are not in the main frame (e.g. iframe) or that are in the
  // same document do not close the Payment Request. Disregard those.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  for (auto& it : payment_requests_) {
    // Since the PaymentRequest dialog blocks the content of the page, the user
    // cannot click on a link to navigate away. Therefore, if the navigation
    // is initiated in the renderer, it does not come from the user.
    it.second->DidStartMainFrameNavigationToDifferentDocument(
        !navigation_handle->IsRendererInitiated());
  }
}

void PaymentRequestWebContentsManager::DestroyRequest(PaymentRequest* request) {
  request->HideIfNecessary();
  payment_requests_.erase(request);
}

PaymentRequestWebContentsManager::PaymentRequestWebContentsManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentRequestWebContentsManager)

}  // namespace payments
