// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/webdata/common/web_data_service_base.h"

class WDTypedResult;

namespace url {
class Origin;
}

namespace webauthn {
class InternalAuthenticator;
}

namespace payments {

class WebPaymentsWebDataService;
struct SecurePaymentConfirmationCredential;

// Wraps retrieval and matching of SPC credentials. Depending on the platform,
// may query either or both of OS provided APIs and the user-profile database.
class SecurePaymentConfirmationCredentialFinder {
 public:
  SecurePaymentConfirmationCredentialFinder();
  virtual ~SecurePaymentConfirmationCredentialFinder();

  using MatchedCredentials = std::optional<
      std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>;

  using SecurePaymentConfirmationCredentialFinderCallback =
      base::OnceCallback<void(MatchedCredentials credentials)>;

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

 private:
  // The source where we found a given credential, either via OS APIs or the
  // user-profile database.
  enum class QuerySource {
    kOSStore,
    kWebDatabase,
  };

  // Holds the result of a query for matching credentials, including the source
  // and the returned credentials. Used to compare between OS and user-profile
  // database queries for platforms that support both.
  struct QueryResult {
    QueryResult(QuerySource source, MatchedCredentials credentials);
    ~QueryResult();
    QueryResult(QueryResult&&);
    QueryResult& operator=(QueryResult&&);

    QuerySource source;
    MatchedCredentials credentials;
  };

  using QueryCallback = base::RepeatingCallback<void(QueryResult)>;

  void OnGetMatchingCredentialsFromWebDataService(
      QueryCallback callback,
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result);

  void OnGetMatchingCredentialIdsFromStore(
      QueryCallback callback,
      std::string relying_party_id,
      std::vector<std::vector<uint8_t>> matching_credentials);

  void OnAllQueriesComplete(
      SecurePaymentConfirmationCredentialFinderCallback result_callback,
      std::vector<QueryResult> results);

  // On platforms where we are using the user profile database, this map holds
  // in-progress requests to the database.
  std::map<WebDataServiceBase::Handle,
           scoped_refptr<payments::WebPaymentsWebDataService>>
      requests_;

  base::WeakPtrFactory<SecurePaymentConfirmationCredentialFinder>
      weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_CREDENTIAL_FINDER_H_
