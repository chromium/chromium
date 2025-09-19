// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_credential_finder.h"

#include "base/feature_list.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/webauthn_security_utils.h"
#include "url/origin.h"

namespace payments {

namespace {
// Determine if a given origin that is calling SPC with a given RP ID requires
// the credentials to be third-party enabled (i.e., the calling party is not the
// RP ID).
bool RequiresThirdPartyPaymentBit(const url::Origin& caller_origin,
                                  const std::string& relying_party_id) {
  return !content::OriginIsAllowedToClaimRelyingPartyId(relying_party_id,
                                                        caller_origin);
}
}  // namespace

SecurePaymentConfirmationCredentialFinder::
    SecurePaymentConfirmationCredentialFinder() = default;
SecurePaymentConfirmationCredentialFinder::
    ~SecurePaymentConfirmationCredentialFinder() {
  std::ranges::for_each(requests_, [&](const auto& pair) {
    if (pair.second) {
      pair.second->CancelRequest(pair.first);
    }
  });
}

void SecurePaymentConfirmationCredentialFinder::GetMatchingCredentials(
    const std::vector<std::vector<uint8_t>>& credential_ids,
    const std::string& relying_party_id,
    const url::Origin& caller_origin,
    webauthn::InternalAuthenticator* authenticator,
    scoped_refptr<payments::WebPaymentsWebDataService> web_data_service,
    SecurePaymentConfirmationCredentialFinderCallback result_callback) {
  // If we have credential-store level support for SPC, we can query the store
  // directly. Otherwise, we have to rely on the user profile database.
  //
  // Currently, credential store APIs are only available on Android.
  if (base::FeatureList::IsEnabled(
          features::kSecurePaymentConfirmationUseCredentialStoreAPIs)) {
    // If we are relying on underlying credential-store level support for SPC,
    // but it isn't available, ensure that canMakePayment() will return false by
    // returning failure here.
    //
    // This helps websites avoid a failure scenario when SPC appears to be
    // available, but in practice it is non-functional due to lack of platform
    // support.
    if (!authenticator->IsGetMatchingCredentialIdsSupported()) {
      std::move(result_callback).Run(std::nullopt);
      return;
    }

    const bool require_third_party_payment_bit =
        RequiresThirdPartyPaymentBit(caller_origin, relying_party_id);

    authenticator->GetMatchingCredentialIds(
        relying_party_id, std::move(credential_ids),
        require_third_party_payment_bit,
        base::BindOnce(&SecurePaymentConfirmationCredentialFinder::
                           OnGetMatchingCredentialIdsFromStore,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(result_callback), relying_party_id));
  } else {
    WebDataServiceBase::Handle handle =
        web_data_service->GetSecurePaymentConfirmationCredentials(
            std::move(credential_ids), relying_party_id,
            base::BindOnce(&SecurePaymentConfirmationCredentialFinder::
                               OnGetMatchingCredentialsFromWebDataService,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(result_callback)));
    requests_[handle] = web_data_service;
  }
}

void SecurePaymentConfirmationCredentialFinder::
    OnGetMatchingCredentialsFromWebDataService(
        SecurePaymentConfirmationCredentialFinderCallback callback,
        WebDataServiceBase::Handle handle,
        std::unique_ptr<WDTypedResult> result) {
  auto iterator = requests_.find(handle);
  if (iterator == requests_.end()) {
    return;
  }

  requests_.erase(iterator);

  if (result && result->GetType() == SECURE_PAYMENT_CONFIRMATION) {
    std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>
        credentials = static_cast<WDResult<std::vector<
            std::unique_ptr<SecurePaymentConfirmationCredential>>>*>(
                          result.get())
                          ->GetValue();
    std::move(callback).Run(std::move(credentials));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void SecurePaymentConfirmationCredentialFinder::
    OnGetMatchingCredentialIdsFromStore(
        SecurePaymentConfirmationCredentialFinderCallback callback,
        std::string relying_party_id,
        std::vector<std::vector<uint8_t>> matching_credentials) {
  std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>> credentials;
  for (std::vector<uint8_t>& credential_id : matching_credentials) {
    credentials.emplace_back(
        std::make_unique<SecurePaymentConfirmationCredential>(
            std::move(credential_id), relying_party_id,
            /*user_id=*/std::vector<uint8_t>()));
  }
  std::move(callback).Run(std::move(credentials));
}

}  // namespace payments
