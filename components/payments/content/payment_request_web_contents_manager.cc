// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_web_contents_manager.h"

#include <utility>

#include "base/check.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
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
    std::unique_ptr<ContentPaymentRequestDelegate> delegate,
    mojo::PendingReceiver<payments::mojom::PaymentRequest> receiver,
    base::WeakPtr<PaymentRequest::ObserverForTest> observer_for_testing) {
  auto new_request = std::make_unique<PaymentRequest>(
      render_frame_host, std::move(delegate), /*manager=*/this,
      delegate->GetDisplayManager(), std::move(receiver), observer_for_testing);
  PaymentRequest* request_ptr = new_request.get();
  payment_requests_.insert(std::make_pair(request_ptr, std::move(new_request)));
}

void PaymentRequestWebContentsManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Navigations that are not in the main frame (e.g. iframe) or that are in the
  // same document do not close the Payment Request. Disregard those.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
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

void PaymentRequestWebContentsManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  const auto render_frame_host_id = render_frame_host->GetGlobalId();
  // Two passes to avoid modifying the |payment_requests_| map while iterating
  // over it.
  std::vector<PaymentRequest*> obsolete;
  for (auto& it : payment_requests_) {
    if (it.second->initiator_frame_routing_id() == render_frame_host_id) {
      obsolete.push_back(it.first);
    }
  }
  for (auto* request : obsolete) {
    request->RenderFrameDeleted(render_frame_host);
  }
}

void PaymentRequestWebContentsManager::DestroyRequest(
    base::WeakPtr<PaymentRequest> request) {
  if (!request)
    return;

  request->HideIfNecessary();
  payment_requests_.erase(request.get());
}

PaymentRequestWebContentsManager::PaymentRequestWebContentsManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentRequestWebContentsManager)

}  // namespace payments
