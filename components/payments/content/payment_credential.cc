// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

namespace payments {

PaymentCredential::PaymentCredential(
    mojo::PendingReceiver<mojom::PaymentCredential> receiver) {
  receiver_.Bind(std::move(receiver));
}

PaymentCredential::~PaymentCredential() = default;

void PaymentCredential::StorePaymentCredential(
    payments::mojom::PaymentCredentialInstrumentPtr instrument,
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    StorePaymentCredentialCallback callback) {
  // TODO(kenrb): Create storage for this credential and save it.
  std::move(callback).Run(mojom::PaymentCredentialCreationStatus::SUCCESS);
}

}  // namespace payments
