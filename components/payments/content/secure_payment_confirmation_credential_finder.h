// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace url {
class Origin;
}

namespace webauthn {
class InternalAuthenticator;
}

namespace payments {

class WebPaymentsWebDataService;
struct SecurePaymentConfirmationCredential;

// Wraps retrieval and matching of SPC credentials, from either the user profile
// database or OS-level APIs.
class SecurePaymentConfirmationCredentialFinder
    : public WebDataServiceConsumer {
 public:
  SecurePaymentConfirmationCredentialFinder();
  ~SecurePaymentConfirmationCredentialFinder() override;

  using SecurePaymentConfirmationCredentialFinderCallback =
      base::OnceCallback<void(
          std::optional<
              std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>
              credentials)>;

  // Retrieve available SPC credentials that match the input `credential_ids`
  // and `relying_party_id`, and which if necessary have the third-party payment
  // bit (i.e., if `relying_party_id` and `caller_origin` are different).
  //
  // The `callback` will be called with the resulting credentials, or
  // std::nullopt if an error was encountered. The callback may be called either
  // synchronously or asynchronously.
  virtual void GetMatchingCredentials(
      const std::vector<std::vector<uint8_t>>& credential_ids,
      const std::string& relying_party_id,
      const url::Origin& caller_origin,
      webauthn::InternalAuthenticator* authenticator,
      scoped_refptr<payments::WebPaymentsWebDataService> web_data_service,
      SecurePaymentConfirmationCredentialFinderCallback result_callback);

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

 private:
  // On platforms where we have credential-store level support for retrieving
  // credentials (i.e., rather than using the user profile database), this
  // callback will be called with the retrieved and matching credential ids.
  //
  // |relying_party_id| and |matching_credentials| are always std::move'd in,
  // and so are not const-ref.
  void OnGetMatchingCredentialIdsFromStore(
      SecurePaymentConfirmationCredentialFinderCallback callback,
      std::string relying_party_id,
      std::vector<std::vector<uint8_t>> matching_credentials);

  // On platforms where we are using the user profile database, this map holds
  // in-progress requests to the database mapped to the callback which should be
  // called with the result.
  std::map<WebDataServiceBase::Handle,
           std::pair<SecurePaymentConfirmationCredentialFinderCallback,
                     scoped_refptr<payments::WebPaymentsWebDataService>>>
      requests_;

  base::WeakPtrFactory<SecurePaymentConfirmationCredentialFinder>
      weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_
