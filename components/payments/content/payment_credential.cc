// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#endif
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/secure_payment_confirmation_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/common/features.h"
#endif
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace payments {

PaymentCredential::PaymentCredential(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::PaymentCredential> receiver,
    scoped_refptr<PaymentManifestWebDataService> web_data_service,
    std::unique_ptr<webauthn::InternalAuthenticator> authenticator)
    : DocumentService(render_frame_host, std::move(receiver)),
      web_data_service_(web_data_service),
      authenticator_(std::move(authenticator)) {}

PaymentCredential::~PaymentCredential() {
  Reset();
}

void PaymentCredential::StorePaymentCredential(
    const std::vector<uint8_t>& credential_id,
    const std::string& rp_id,
    const std::vector<uint8_t>& user_id,
    StorePaymentCredentialCallback callback) {
  if (state_ != State::kIdle || !IsCurrentStateValid() ||
      credential_id.empty() || rp_id.empty() || user_id.empty()) {
    Reset();
    std::move(callback).Run(
        mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_CREDENTIAL);
    return;
  }

  RecordFirstSystemPromptResult(
      SecurePaymentConfirmationEnrollSystemPromptResult::kAccepted);

  // If credential-store level APIs are available, the credential information
  // will already have been stored during creation.
  if (base::FeatureList::IsEnabled(
          features::kSecurePaymentConfirmationUseCredentialStoreAPIs)) {
    Reset();
    std::move(callback).Run(mojom::PaymentCredentialStorageStatus::SUCCESS);
    return;
  }

  storage_callback_ = std::move(callback);
  state_ = State::kStoringCredential;
  data_service_request_handle_ =
      web_data_service_->AddSecurePaymentConfirmationCredential(
          std::make_unique<SecurePaymentConfirmationCredential>(credential_id,
                                                                rp_id, user_id),
          /*consumer=*/this);
}

void PaymentCredential::MakePaymentCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    MakePaymentCredentialCallback callback) {
  // TODO(crbug.com/377278827): Include a browser bound key in the client data
  // JSON by creating the browser bound key then provide it to the authenticator
  // (via SetPaymentOptions() or similar).
  authenticator_->MakeCredential(
      std::move(options),
      base::BindOnce(&PaymentCredential::OnAuthenticatorMakeCredential,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentCredential::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  if (state_ != State::kStoringCredential || !IsCurrentStateValid() ||
      data_service_request_handle_ != h) {
    Reset();
    return;
  }

  auto callback = std::move(storage_callback_);
  Reset();

  std::move(callback).Run(
      result && static_cast<WDResult<bool>*>(result.get())->GetValue()
          ? mojom::PaymentCredentialStorageStatus::SUCCESS
          : mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_CREDENTIAL);
}

// Handles the authenticator make credential callback by adding the browser
// bound signature, then running the callback.
void PaymentCredential::OnAuthenticatorMakeCredential(
    PaymentCredential::MakePaymentCredentialCallback callback,
    ::blink::mojom::AuthenticatorStatus authenticator_status,
    ::blink::mojom::MakeCredentialAuthenticatorResponsePtr response,
    ::blink::mojom::WebAuthnDOMExceptionDetailsPtr maybe_exception_details) {
#if BUILDFLAG(IS_ANDROID)
  if (response &&
      base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    if (!browser_bound_key_store_) {
      browser_bound_key_store_ = GetBrowserBoundKeyStoreInstance();
    }
    if (browser_bound_key_store_) {
      std::unique_ptr<BrowserBoundKey> browser_bound_key =
          browser_bound_key_store_->GetOrCreateBrowserBoundKeyForCredentialId(
              response->info->raw_id);
      if (browser_bound_key) {
        std::vector<uint8_t> signature_output =
            browser_bound_key->Sign(response->info->client_data_json);
        response->payment =
            blink::mojom::AuthenticationExtensionsPaymentResponse::New();
        response->payment->browser_bound_signatures.push_back(
            std::move(signature_output));
      }
    }
  }
#endif
  std::move(callback).Run(authenticator_status, std::move(response),
                          std::move(maybe_exception_details));
}

bool PaymentCredential::IsCurrentStateValid() const {
  if (!content::IsFrameAllowedToUseSecurePaymentConfirmation(
          &render_frame_host()) ||
      !web_data_service_) {
    return false;
  }

  switch (state_) {
    case State::kIdle:
      return !storage_callback_ && !data_service_request_handle_;

    case State::kStoringCredential:
      return storage_callback_ && data_service_request_handle_;
  }
}

void PaymentCredential::RecordFirstSystemPromptResult(
    SecurePaymentConfirmationEnrollSystemPromptResult result) {
  if (!is_system_prompt_result_recorded_) {
    is_system_prompt_result_recorded_ = true;
    RecordEnrollSystemPromptResult(result);
  }
}

void PaymentCredential::Reset() {
  // Callbacks must either be run or disconnected before being destroyed, so
  // run them if they are still connected.
  if (storage_callback_) {
    std::move(storage_callback_)
        .Run(mojom::PaymentCredentialStorageStatus::FAILED_TO_STORE_CREDENTIAL);
  }

  if (web_data_service_ && data_service_request_handle_) {
    web_data_service_->CancelRequest(data_service_request_handle_.value());
  }

  data_service_request_handle_.reset();
  is_system_prompt_result_recorded_ = false;
  state_ = State::kIdle;
}

}  // namespace payments
