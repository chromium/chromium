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
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
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
  void SecurePaymentConfirmationAvailability(
      SecurePaymentConfirmationAvailabilityCallback callback) override;

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
  void SetPasskeyBrowserBinderForTesting(
      std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder);
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
      std::optional<PasskeyBrowserBinder::UnboundKey> browser_bound_key,
      ::blink::mojom::AuthenticatorStatus authenticator_status,
      ::blink::mojom::MakeCredentialAuthenticatorResponsePtr response,
      ::blink::mojom::WebAuthnDOMExceptionDetailsPtr maybe_exception_details);

  bool IsCurrentStateValid() const;
  void RecordFirstSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult result);
  void Reset();

  State state_ = State::kIdle;
  scoped_refptr<PaymentManifestWebDataService> web_data_service_;
  std::unique_ptr<webauthn::InternalAuthenticator> authenticator_;
  std::optional<WebDataServiceBase::Handle> data_service_request_handle_;
  StorePaymentCredentialCallback storage_callback_;
  std::optional<WebDataServiceBase::Handle>
      set_browser_bound_key_request_handle_;
  bool is_system_prompt_result_recorded_ = false;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder_;
#endif  // BUILDFLAG(IS_ANDROID)

  base::WeakPtrFactory<SecurePaymentConfirmationService> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_SERVICE_H_
