// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"

namespace payments {

class PaymentManifestWebDataService;

// Implementation of the mojom::PaymentCredential interface for storing
// PaymentCredential instruments and their associated WebAuthn credential IDs.
// These can be retrieved later to authenticate during a PaymentRequest
// that uses Secure Payment Confirmation.
class PaymentCredential : public mojom::PaymentCredential,
                          public WebDataServiceConsumer {
 public:
  PaymentCredential(
      scoped_refptr<PaymentManifestWebDataService> web_data_sevice,
      mojo::PendingReceiver<mojom::PaymentCredential> receiver);
  ~PaymentCredential() override;

  PaymentCredential(const PaymentCredential&) = delete;
  PaymentCredential& operator=(const PaymentCredential&) = delete;

  // mojom::PaymentCredential:
  void StorePaymentCredential(
      payments::mojom::PaymentCredentialInstrumentPtr instrument,
      const std::vector<uint8_t>& credential_id,
      const std::string& rp_id,
      StorePaymentCredentialCallback callback) override;

 private:
  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
  std::map<WebDataServiceBase::Handle, StorePaymentCredentialCallback>
      callbacks_;
  mojo::Receiver<mojom::PaymentCredential> receiver_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_
