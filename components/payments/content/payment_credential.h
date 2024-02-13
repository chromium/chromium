// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"

namespace payments {

class PaymentManifestWebDataService;

// Implementation of the mojom::PaymentCredential interface for storing
// PaymentCredential instruments and their associated WebAuthn credential IDs.
// These can be retrieved later to authenticate during a PaymentRequest
// that uses Secure Payment Confirmation.
class PaymentCredential
    : public content::DocumentService<mojom::PaymentCredential>,
      public WebDataServiceConsumer {
 public:
  PaymentCredential(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::PaymentCredential> receiver,
      scoped_refptr<PaymentManifestWebDataService> web_data_service);
  ~PaymentCredential() override;

  PaymentCredential(const PaymentCredential&) = delete;
  PaymentCredential& operator=(const PaymentCredential&) = delete;

  // mojom::PaymentCredential:
  void StorePaymentCredential(const std::vector<uint8_t>& credential_id,
                              const std::string& rp_id,
                              const std::vector<uint8_t>& user_id,
                              StorePaymentCredentialCallback callback) override;

 private:
  // States of the enrollment flow, necessary to ensure correctness with
  // round-trips to the renderer process. Methods that perform async
  // actions (like StorePaymentCredential) have procedure:
  //   1. Validate state.
  //   2. Validate parameters.
  //   3. Use parameters.
  //   4. Update the state.
  //   5. Make the async call.
  // Methods that perform terminating actions (like OnWebDataServiceRequestDone)
  // have procedure:
  //   1. Validate state.
  //   2. Validate parameters.
  //   3. Use parameters.
  //   4. Call Reset() to perform cleanup.
  //   5. Invoke a mojo callback to the renderer.
  // Any method may call Reset() to ensure callbacks are called and return to a
  // valid Idle state.
  enum class State { kIdle, kStoringCredential };

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  bool IsCurrentStateValid() const;
  void RecordFirstSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult result);
  void Reset();

  State state_ = State::kIdle;
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
  std::optional<WebDataServiceBase::Handle> data_service_request_handle_;
  StorePaymentCredentialCallback storage_callback_;
  bool is_system_prompt_result_recorded_ = false;

  base::WeakPtrFactory<PaymentCredential> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_H_
