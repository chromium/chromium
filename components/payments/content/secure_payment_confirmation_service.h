// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_SERVICE_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_SERVICE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/payments/secure_payment_confirmation_service.mojom.h"

namespace webauthn {

class InternalAuthenticator;

}  // namespace webauthn

namespace payments {

class BrowserBoundKey;
class PaymentManifestWebDataService;

// Implementation of the mojom::SecurePaymentConfirmationService interface,
// which provides SPC-related functionality that is not tied to a specific
// PaymentRequest invocation.
class SecurePaymentConfirmationService
    : public content::DocumentService<mojom::SecurePaymentConfirmationService>,
      public WebDataServiceConsumer {
 public:
  SecurePaymentConfirmationService(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::SecurePaymentConfirmationService> receiver,
      scoped_refptr<PaymentManifestWebDataService> web_data_service,
      std::unique_ptr<webauthn::InternalAuthenticator> authenticator);
  ~SecurePaymentConfirmationService() override;

  SecurePaymentConfirmationService(const SecurePaymentConfirmationService&) =
      delete;
  SecurePaymentConfirmationService& operator=(
      const SecurePaymentConfirmationService&) = delete;

  // mojom::SecurePaymentConfirmationService:
  void StorePaymentCredential(const std::vector<uint8_t>& credential_id,
                              const std::string& rp_id,
                              const std::vector<uint8_t>& user_id,
                              StorePaymentCredentialCallback callback) override;

  // mojom::SecurePaymentConfirmationService:
  void MakePaymentCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      MakePaymentCredentialCallback callback) override;

#if BUILDFLAG(IS_ANDROID)
  void SetBrowserBoundKeyStoreForTesting(
      std::unique_ptr<BrowserBoundKeyStore> browser_bound_key_store);
  // Inject a random byte generator. The callback takes the desired number of
  // bytes and returns a vector of that size.
  void SetRandomBytesAsVectorForTesting(
      base::RepeatingCallback<std::vector<uint8_t>(size_t)> callback);
#endif  // BUILDFLAG(IS_ANDROID)

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

  // MakeCredentialCallback:
  void OnAuthenticatorMakeCredential(
      SecurePaymentConfirmationService::MakePaymentCredentialCallback callback,
      std::string maybe_relying_party,
      std::optional<std::vector<uint8_t>> maybe_browser_bound_key_id,
      std::unique_ptr<BrowserBoundKey> maybe_browser_bound_key,
      ::blink::mojom::AuthenticatorStatus authenticator_status,
      ::blink::mojom::MakeCredentialAuthenticatorResponsePtr response,
      ::blink::mojom::WebAuthnDOMExceptionDetailsPtr maybe_exception_details);

  bool IsCurrentStateValid() const;
  void RecordFirstSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult result);
  void Reset();
#if BUILDFLAG(IS_ANDROID)
  // Creates a new random identifier when new browser bound keys are
  // constructed. The returned value is used as the identifier for the browser
  // bound key to be created. The identifier is expected to be sufficiently
  // random to avoid collisions on chrome profile on one device.
  //
  // Tests can inject a stable identifier by calling
  // `SetBrowserBoundKeyIdForTesting()` to avoid randomness in tests.
  std::vector<uint8_t> GetRandomBrowserBoundKeyId();
#endif  // BUILDFLAG(IS_ANDROID)

  State state_ = State::kIdle;
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
  std::unique_ptr<webauthn::InternalAuthenticator> authenticator_;
  std::optional<WebDataServiceBase::Handle> data_service_request_handle_;
  StorePaymentCredentialCallback storage_callback_;
  std::optional<WebDataServiceBase::Handle>
      set_browser_bound_key_request_handle_;
  bool is_system_prompt_result_recorded_ = false;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<BrowserBoundKeyStore> browser_bound_key_store_;
  base::RepeatingCallback<std::vector<uint8_t>(size_t)>
      random_bytes_as_vector_callback_;
#endif  // BUILDFLAG(IS_ANDROID)

  base::WeakPtrFactory<SecurePaymentConfirmationService> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_SERVICE_H_
