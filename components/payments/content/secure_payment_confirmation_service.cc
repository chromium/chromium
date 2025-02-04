// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_service.h"

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "components/payments/content/browser_binding/browser_bound_key.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/secure_payment_confirmation_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "crypto/random.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "third_party/blink/public/common/features.h"
#endif

namespace payments {
namespace {
#if BUILDFLAG(IS_ANDROID)
// The length of the random browser bound key identifiers.
constexpr size_t kBrowserBoundKeyIdLength = 32;
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace

SecurePaymentConfirmationService::SecurePaymentConfirmationService(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::SecurePaymentConfirmationService> receiver,
    scoped_refptr<PaymentManifestWebDataService> web_data_service,
    std::unique_ptr<webauthn::InternalAuthenticator> authenticator)
    : DocumentService(render_frame_host, std::move(receiver)),
      web_data_service_(web_data_service),
      authenticator_(std::move(authenticator)) {
#if BUILDFLAG(IS_ANDROID)
  random_bytes_as_vector_callback_ =
      base::BindRepeating(crypto::RandBytesAsVector);
#endif
}

SecurePaymentConfirmationService::~SecurePaymentConfirmationService() {
  Reset();
}

void SecurePaymentConfirmationService::IsSecurePaymentConfirmationAvailable(
    IsSecurePaymentConfirmationAvailableCallback callback) {
  if (!base::FeatureList::IsEnabled(::features::kSecurePaymentConfirmation)) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/40258712): This method should next check that the 'payment'
  // permission policy is set. However, SecurePaymentConfirmationService is only
  // installed by the factory if the policy is set, so it is not possible for
  // that check to ever be false here. Instead, the renderer-side implementation
  // of isSecurePaymentConfirmationAvailable checks for the permission policy
  // before trying to call to the browser (or else it would hit a mojo error).
  //
  // This all technically works, but it would probably be cleaner to refactor
  // SecurePaymentConfirmationService to always be available and to properly
  // handle calls if the 'payment' policy is not present.

  // The remaining checks in this method are for the underlying platform
  // authenticator. If the kSecurePaymentConfirmationDebug method is enabled we
  // may not have a real authenticator, so early-exit here. This is never
  // expected to be hit in production, as it is a debug flag only.
  if (base::FeatureList::IsEnabled(
          ::features::kSecurePaymentConfirmationDebug)) {
    std::move(callback).Run(true);
    return;
  }

  if (!authenticator_) {
    std::move(callback).Run(false);
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kSecurePaymentConfirmationUseCredentialStoreAPIs) &&
      !authenticator_->IsGetMatchingCredentialIdsSupported()) {
    std::move(callback).Run(false);
    return;
  }

  // At this point the only remaining check is that the user verifying
  // authenticator is available, so we can pass our callback directly.
  authenticator_->IsUserVerifyingPlatformAuthenticatorAvailable(
      std::move(callback));
}

void SecurePaymentConfirmationService::StorePaymentCredential(
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

void SecurePaymentConfirmationService::MakePaymentCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    MakePaymentCredentialCallback callback) {
  std::string relying_party_id;
  std::optional<std::vector<uint8_t>> browser_bound_key_id;
  std::unique_ptr<BrowserBoundKey> browser_bound_key;
#if BUILDFLAG(IS_ANDROID)
  if (options &&
      base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    relying_party_id = options->relying_party.id;
    // To set the browser bound key in the client data json before creating the
    // credential, the browser bound key must be created with a random id since
    // the credential id is not known until after creating the credential.
    browser_bound_key_id =
        random_bytes_as_vector_callback_.Run(kBrowserBoundKeyIdLength);
    if (browser_bound_key_store_ == nullptr) {
      browser_bound_key_store_ = GetBrowserBoundKeyStoreInstance();
    }
    if (browser_bound_key_store_) {
      // TODO(crbug.com/384940850): Regenerate the browser bound key identifier
      // if a browser bound key with the same identifier already exists.
      // TODO(crbug.com/377278827): Provide the browser bound public key
      // credential parameters from the payment extensions to the key store.
      browser_bound_key =
          browser_bound_key_store_->GetOrCreateBrowserBoundKeyForCredentialId(
              *browser_bound_key_id,
              options->payment_browser_bound_key_parameters.value_or(
                  options->public_key_parameters));
    }
    if (browser_bound_key) {
      auto payment_options = ::blink::mojom::PaymentOptions::New();
      payment_options->total = mojom::PaymentCurrencyAmount::New();
      payment_options->instrument =
          ::blink::mojom::PaymentCredentialInstrument::New();
      payment_options->browser_bound_public_key =
          browser_bound_key->GetPublicKeyAsCoseKey();
      authenticator_->SetPaymentOptions(std::move(payment_options));
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)
  authenticator_->MakeCredential(
      std::move(options),
      base::BindOnce(
          &SecurePaymentConfirmationService::OnAuthenticatorMakeCredential,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          std::move(relying_party_id), std::move(browser_bound_key_id),
          std::move(browser_bound_key)));
}

#if BUILDFLAG(IS_ANDROID)
void SecurePaymentConfirmationService::SetBrowserBoundKeyStoreForTesting(
    std::unique_ptr<BrowserBoundKeyStore> browser_bound_key_store) {
  browser_bound_key_store_ = std::move(browser_bound_key_store);
}

void SecurePaymentConfirmationService::SetRandomBytesAsVectorForTesting(
    base::RepeatingCallback<std::vector<uint8_t>(size_t)> callback) {
  random_bytes_as_vector_callback_ = std::move(callback);
}
#endif  // BUILDFLAG(IS_ANDROID)

void SecurePaymentConfirmationService::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
#if BUILDFLAG(IS_ANDROID)
  if (set_browser_bound_key_request_handle_ &&
      base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    if (set_browser_bound_key_request_handle_ == h) {
      // TODO(crbug.com/384954763): Delete the browser bound key from the key
      // store if the result was false (not successful).
      set_browser_bound_key_request_handle_ = std::nullopt;
      return;
    }
  }
#endif
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
void SecurePaymentConfirmationService::OnAuthenticatorMakeCredential(
    SecurePaymentConfirmationService::MakePaymentCredentialCallback callback,
    std::string relying_party,
    std::optional<std::vector<uint8_t>> browser_bound_key_id,
    std::unique_ptr<BrowserBoundKey> browser_bound_key,
    ::blink::mojom::AuthenticatorStatus authenticator_status,
    ::blink::mojom::MakeCredentialAuthenticatorResponsePtr response,
    ::blink::mojom::WebAuthnDOMExceptionDetailsPtr maybe_exception_details) {
#if BUILDFLAG(IS_ANDROID)
  if (response &&
      base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    if (browser_bound_key_id && browser_bound_key) {
      std::vector<uint8_t> signature_output =
          browser_bound_key->Sign(response->info->client_data_json);
      response->payment =
          blink::mojom::AuthenticationExtensionsPaymentResponse::New();
      response->payment->browser_bound_signature = std::move(signature_output);
      web_data_service_->SetBrowserBoundKey(
          response->info->raw_id, std::move(relying_party),
          std::move(*browser_bound_key_id), this);
    }
  }
#endif
  std::move(callback).Run(authenticator_status, std::move(response),
                          std::move(maybe_exception_details));
}

bool SecurePaymentConfirmationService::IsCurrentStateValid() const {
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

void SecurePaymentConfirmationService::RecordFirstSystemPromptResult(
    SecurePaymentConfirmationEnrollSystemPromptResult result) {
  if (!is_system_prompt_result_recorded_) {
    is_system_prompt_result_recorded_ = true;
    RecordEnrollSystemPromptResult(result);
  }
}

void SecurePaymentConfirmationService::Reset() {
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
