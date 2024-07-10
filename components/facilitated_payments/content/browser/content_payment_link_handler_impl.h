// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_PAYMENT_LINK_HANDLER_IMPL_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_PAYMENT_LINK_HANDLER_IMPL_H_

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/facilitated_payments/payment_link_handler.mojom.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments::facilitated {

// Implementation of the mojom::PaymentLinkHandler interface, responsible for
// handling payment links within Chromium's content layer. This class receives
// payment link URLs from the renderer process, then processes them
// appropriately.
class ContentPaymentLinkHandlerImpl
    : public content::DocumentService<mojom::PaymentLinkHandler> {
 public:
  ContentPaymentLinkHandlerImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::PaymentLinkHandler> receiver);

  ContentPaymentLinkHandlerImpl(const ContentPaymentLinkHandlerImpl&) = delete;
  ContentPaymentLinkHandlerImpl& operator=(
      const ContentPaymentLinkHandlerImpl&) = delete;

  ~ContentPaymentLinkHandlerImpl() override;

  // mojom::PaymentLinkHandler:
  void HandlePaymentLink(const GURL& url) override;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_PAYMENT_LINK_HANDLER_IMPL_H_
