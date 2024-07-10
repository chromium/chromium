// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/browser/content_payment_link_handler_impl.h"

#include "components/facilitated_payments/core/browser/payment_link_handler_impl.h"
#include "content/public/browser/browser_thread.h"

namespace payments::facilitated {

ContentPaymentLinkHandlerImpl::ContentPaymentLinkHandlerImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::PaymentLinkHandler> receiver)
    : DocumentService<mojom::PaymentLinkHandler>(render_frame_host,
                                                 std::move(receiver)) {}

ContentPaymentLinkHandlerImpl::~ContentPaymentLinkHandlerImpl() = default;

void ContentPaymentLinkHandlerImpl::HandlePaymentLink(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_frame_host().IsActive()) {
    return;
  }

  // Validate and handle the payment link URL. The payment link will be parsed
  // and validated to support expected partners only, triggering a native
  // payment experience with users' approval.
  PaymentLinkHandlerImpl payment_link_handler;
  payment_link_handler.TriggerEwalletPushPayment(
      url, render_frame_host().GetLastCommittedURL());
}

}  // namespace payments::facilitated
