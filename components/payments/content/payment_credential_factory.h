// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom-forward.h"

namespace content {
class RenderFrameHost;
}

namespace payments {

// Connect a PaymentCredential receiver to handle payment credential creation.
void CreatePaymentCredential(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::PaymentCredential> receiver);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_FACTORY_H_
