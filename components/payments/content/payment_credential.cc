// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/secure_payment_confirmation_instrument.h"

namespace payments {

PaymentCredential::PaymentCredential(
    scoped_refptr<PaymentManifestWebDataService> web_data_sevice,
    mojo::PendingReceiver<mojom::PaymentCredential> receiver)
    : web_data_service_(web_data_sevice) {
  receiver_.Bind(std::move(receiver));
}

PaymentCredential::~PaymentCredential() = default;

void PaymentCredential::StorePaymentCredential(
    payments::mojom::PaymentCredentialInstrumentPtr instrument,
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    StorePaymentCredentialCallback callback) {
  // TODO(https://crbug.com/1121021): Download `instrument->icon` and encode it
  // into `std::vector<uint8_t>`.
  if (!web_data_service_) {
    std::move(callback).Run(
        mojom::PaymentCredentialCreationStatus::FAILED_TO_STORE_INSTRUMENT);
    return;
  }

  WebDataServiceBase::Handle handle =
      web_data_service_->AddSecurePaymentConfirmationInstrument(
          std::make_unique<SecurePaymentConfirmationInstrument>(
              credential_id, rp_id, base::UTF8ToUTF16(instrument->display_name),
              /*icon=*/std::vector<uint8_t>(1, 1)),
          /*consumer=*/this);
  callbacks_[handle] = std::move(callback);
}

void PaymentCredential::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  auto iterator = callbacks_.find(h);
  if (iterator == callbacks_.end())
    return;

  auto callback = std::move(iterator->second);
  DCHECK(callback);
  callbacks_.erase(iterator);

  std::move(callback).Run(
      static_cast<WDResult<bool>*>(result.get())->GetValue()
          ? mojom::PaymentCredentialCreationStatus::SUCCESS
          : mojom::PaymentCredentialCreationStatus::FAILED_TO_STORE_INSTRUMENT);
}

}  // namespace payments
