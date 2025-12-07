// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_SERVICE_FACTORY_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_SERVICE_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/payments/secure_payment_confirmation_service.mojom-forward.h"

namespace content {
class RenderFrameHost;
}

namespace payments {

// Connect a SecurePaymentConfirmationService receiver to handle SPC-related
// tasks including payment credential creation.
void CreateSecurePaymentConfirmationService(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::SecurePaymentConfirmationService> receiver,
    std::string browser_bound_key_store_keychain_access_group);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_SERVICE_FACTORY_H_
