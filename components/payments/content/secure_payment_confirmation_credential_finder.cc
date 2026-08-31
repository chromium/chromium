// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_credential_finder.h"

#include <set>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/secure_payment_confirmation_credential.h"
#include "components/payments/core/secure_payment_confirmation_metrics.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "components/webauthn/core/browser/webauthn_security_utils.h"
#include "url/origin.h"

namespace payments {

namespace {
using MatchedCredentials =
    SecurePaymentConfirmationCredentialFinder::MatchedCredentials;

// Determine if a given origin that is calling SPC with a given RP ID requires
// the credentials to be third-party enabled (i.e., the calling party is not the
// RP ID).
bool RequiresThirdPartyPaymentBit(const url::Origin& caller_origin,
                                  const std::string& relying_party_id) {
  return !webauthn::OriginIsAllowedToClaimRelyingPartyId(relying_party_id,
                                                         caller_origin);
}

// Encapsulates the strategy for where to query for SPC credentials. This
// differs based on feature flags set and availability of OS APIs and the web
// data service.
struct QueryStrategy {
  const bool query_os_store;
  const bool query_web_database;

  // Helper method to return the number of queries required for this strategy.
  size_t num_queries() const {
    return (query_os_store ? 1 : 0) + (query_web_database ? 1 : 0);
  }

  static QueryStrategy Determine(bool require_third_party_payment_bit,
                                 bool has_queryable_os_store,
                                 bool has_web_data_service) {
    switch (features::kCredentialDiscoveryModeParam.Get()) {
      case features::CredentialDiscoveryMode::kOsOnly:
        return QueryStrategy(has_queryable_os_store,
                             /*query_web_database=*/false);

      case features::CredentialDiscoveryMode::kHybrid:
        // TODO(crbug.com/40868539): Add support for the third-party case for
        // hybrid mode.
        return QueryStrategy(
            !require_third_party_payment_bit && has_queryable_os_store,
            has_web_data_service);

      case features::CredentialDiscoveryMode::kUserDatabaseOnly:
        return QueryStrategy(/*query_os_store=*/false, has_web_data_service);
    }
  }

 private:
  QueryStrategy(bool query_os_store, bool query_web_database)
      : query_os_store(query_os_store),
        query_web_database(query_web_database) {}
};

// Records metrics for hybrid discovery mode. If both stores were queried,
// record metrics on:
//
// 1. Whether the OS API is providing uplift; that is, would we have failed to
//    match this request if not for the OS APIs?
//
// 2. Whether there are any orphaned credentials in the user profile database,
//    that the OS APIs do not return (e.g., if the user deleted the passkey).
void RecordHybridDiscoveryMetrics(
    const MatchedCredentials& os_credentials,
    const MatchedCredentials& web_db_credentials) {
  if (!os_credentials.has_value() || !web_db_credentials.has_value()) {
    return;
  }

  std::set<std::vector<uint8_t>> os_ids;
  for (const auto& cred : *os_credentials) {
    os_ids.insert(cred->credential_id);
  }

  bool has_valid_db_match = false;
  bool has_orphans = false;
  for (const auto& cred : *web_db_credentials) {
    if (os_ids.contains(cred->credential_id)) {
      has_valid_db_match = true;
    } else {
      has_orphans = true;
    }
  }

  // Uplift represents cases where the OS store enabled a payment that would
  // have failed with the database alone, so should only be logged if there was
  // no database match.
  //
  // TODO(crbug.com/40868539): Once we stop writing newly enrolled credentials
  // to the local database (i.e., when the OS authenticator supports storing
  // the payment bit), retrieving an OS credential with the payment bit when
  // the database is empty will be expected rather than true uplift. We will
  // need to adjust the uplift calculation accordingly.
  const bool has_uplift = !os_ids.empty() && !has_valid_db_match;

  RecordOSStoreUplift(has_uplift);
  RecordWebDatabaseHasOrphanedCredentials(has_orphans);
}

}  // namespace

SecurePaymentConfirmationCredentialFinder::QueryResult::QueryResult(
    QuerySource source,
    MatchedCredentials credentials)
    : source(source), credentials(std::move(credentials)) {}
SecurePaymentConfirmationCredentialFinder::QueryResult::~QueryResult() =
    default;
SecurePaymentConfirmationCredentialFinder::QueryResult::QueryResult(
    QueryResult&&) = default;
SecurePaymentConfirmationCredentialFinder::QueryResult&
SecurePaymentConfirmationCredentialFinder::QueryResult::operator=(
    QueryResult&&) = default;

SecurePaymentConfirmationCredentialFinder::
    SecurePaymentConfirmationCredentialFinder() = default;
SecurePaymentConfirmationCredentialFinder::
    ~SecurePaymentConfirmationCredentialFinder() {
  VLOG(1) << "SecurePaymentConfirmationCredentialFinder::~"
             "SecurePaymentConfirmationCredentialFinder: Cancelling "
          << requests_.size() << " pending requests";
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
  VLOG(1) << "SecurePaymentConfirmationCredentialFinder::"
             "GetMatchingCredentials"
          << " [credential_ids count: " << credential_ids.size()
          << ", relying_party_id: " << relying_party_id
          << ", authenticator: " << (authenticator ? "valid" : "null")
          << ", web_data_service: " << (web_data_service ? "valid" : "null")
          << "]";

  const bool require_third_party_payment_bit =
      RequiresThirdPartyPaymentBit(caller_origin, relying_party_id);

  const bool has_queryable_os_store =
      authenticator && authenticator->IsGetMatchingCredentialIdsSupported();

  const QueryStrategy strategy = QueryStrategy::Determine(
      require_third_party_payment_bit, has_queryable_os_store,
      /*has_web_data_service=*/web_data_service != nullptr);

  if (strategy.num_queries() == 0) {
    VLOG(1) << "SecurePaymentConfirmationCredentialFinder::"
               "GetMatchingCredentials: No queryable location available";
    std::move(result_callback).Run(std::nullopt);
    return;
  }

  QueryCallback barrier_callback = base::BarrierCallback<QueryResult>(
      strategy.num_queries(),
      base::BindOnce(
          &SecurePaymentConfirmationCredentialFinder::OnAllQueriesComplete,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));

  if (strategy.query_os_store) {
    VLOG(1) << "SecurePaymentConfirmationCredentialFinder::"
               "GetMatchingCredentials: Querying OS Credential Store";
    authenticator->GetMatchingCredentialIds(
        relying_party_id, credential_ids, require_third_party_payment_bit,
        base::BindOnce(&SecurePaymentConfirmationCredentialFinder::
                           OnGetMatchingCredentialIdsFromStore,
                       weak_ptr_factory_.GetWeakPtr(), barrier_callback,
                       relying_party_id));
  }

  if (strategy.query_web_database) {
    VLOG(1) << "SecurePaymentConfirmationCredentialFinder::"
               "GetMatchingCredentials: Querying web data service";
    WebDataServiceBase::Handle handle =
        web_data_service->GetSecurePaymentConfirmationCredentials(
            credential_ids, relying_party_id,
            base::BindOnce(&SecurePaymentConfirmationCredentialFinder::
                               OnGetMatchingCredentialsFromWebDataService,
                           weak_ptr_factory_.GetWeakPtr(), barrier_callback));
    requests_[handle] = web_data_service;
  }
}

void SecurePaymentConfirmationCredentialFinder::
    OnGetMatchingCredentialsFromWebDataService(
        QueryCallback callback,
        WebDataServiceBase::Handle handle,
        std::unique_ptr<WDTypedResult> result) {
  VLOG(1) << "SecurePaymentConfirmationCredentialFinder::"
             "OnGetMatchingCredentialsFromWebDataService called for handle: "
          << handle;
  MatchedCredentials credentials = std::nullopt;

  auto iterator = requests_.find(handle);
  if (iterator == requests_.end()) {
    VLOG(1) << "SecurePaymentConfirmationCredentialFinder::"
               "OnGetMatchingCredentialsFromWebDataService: Handle not found "
               "in requests_";
  } else {
    requests_.erase(iterator);

    if (!result) {
      VLOG(1) << "SecurePaymentConfirmationCredentialFinder::"
                 "OnGetMatchingCredentialsFromWebDataService: Result is null";
    } else if (result->GetType() != SECURE_PAYMENT_CONFIRMATION) {
      VLOG(1) << "SecurePaymentConfirmationCredentialFinder::"
                 "OnGetMatchingCredentialsFromWebDataService: Result type "
              << result->GetType() << " is not SECURE_PAYMENT_CONFIRMATION";
    } else {
      credentials = static_cast<WDResult<
          std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>>>*>(
                        result.get())
                        ->GetValue();
    }
  }

  callback.Run(QueryResult(QuerySource::kWebDatabase, std::move(credentials)));
}

void SecurePaymentConfirmationCredentialFinder::
    OnGetMatchingCredentialIdsFromStore(
        QueryCallback callback,
        std::string relying_party_id,
        std::vector<std::vector<uint8_t>> matching_credentials) {
  std::vector<std::unique_ptr<SecurePaymentConfirmationCredential>> credentials;
  credentials.reserve(matching_credentials.size());
  for (std::vector<uint8_t>& credential_id : matching_credentials) {
    credentials.emplace_back(
        std::make_unique<SecurePaymentConfirmationCredential>(
            std::move(credential_id), relying_party_id,
            /*user_id=*/std::vector<uint8_t>()));
  }

  callback.Run(QueryResult(QuerySource::kOSStore, std::move(credentials)));
}

void SecurePaymentConfirmationCredentialFinder::OnAllQueriesComplete(
    SecurePaymentConfirmationCredentialFinderCallback result_callback,
    std::vector<QueryResult> results) {
  MatchedCredentials os_credentials;
  MatchedCredentials web_db_credentials;

  for (auto& result : results) {
    if (result.source == QuerySource::kOSStore) {
      os_credentials = std::move(result.credentials);
    } else {
      web_db_credentials = std::move(result.credentials);
    }
  }

  RecordHybridDiscoveryMetrics(os_credentials, web_db_credentials);

  // The OS store results are authoritative if queried, even if empty (e.g. if
  // the user deleted the credential from the OS authenticator). We only use
  // the web database credentials if the OS store was not queried (e.g. in
  // database-only mode, or for third-party SPC in hybrid mode).
  std::move(result_callback)
      .Run(os_credentials.has_value() ? std::move(os_credentials)
                                      : std::move(web_db_credentials));
}

}  // namespace payments
