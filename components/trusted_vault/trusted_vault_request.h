// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_REQUEST_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_REQUEST_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sync/driver/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "google_apis/gaia/core_account_id.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace syncer {

// Allows calling VaultService API using proto-over-http.
class TrustedVaultRequest : public TrustedVaultConnection::Request {
 public:
  enum class HttpStatus {
    // Reported when server returns http status code 200 or 204.
    kSuccess,
    // Reported when server returns http status code 400 (bad request).
    kBadRequest,
    // Reported when server returns http status code 404 (not found).
    kNotFound,
    // Reported when server returns http status code 409 (conflict).
    kConflict,
    // Reported when access token fetch attempt was failed due to transient auth
    // error.
    kTransientAccessTokenFetchError,
    // Reported when access token fetch attempt failed due to permanent auth
    // error.
    kPersistentAccessTokenFetchError,
    // Reported when access token fetch attempt was cancelled due to primary
    // account change.
    kPrimaryAccountChangeAccessTokenFetchError,
    // Reported when network error occurs.
    kNetworkError,
    // Reported when other http errors occur.
    kOtherError
  };

  enum class HttpMethod { kGet, kPost };

  using CompletionCallback =
      base::OnceCallback<void(HttpStatus status,
                              const std::string& response_body)>;

  // |callback| will be run upon completion and it's allowed to delete this
  // object upon |callback| call. For GET requests, |serialized_request_proto|
  // must be null. For |POST| requests, it can be either way (optional payload).
  // |url_loader_factory| must not be null.
  // |max_retry_duration| specifies for how long the request can be retried in
  // case of transient errors. There will be no retries when it is set to zero.
  TrustedVaultRequest(
      const CoreAccountId& account_id,
      HttpMethod http_method,
      const GURL& request_url,
      const absl::optional<std::string>& serialized_request_proto,
      base::TimeDelta max_retry_duration,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher,
      TrustedVaultURLFetchReasonForUMA reason_for_uma);
  TrustedVaultRequest(const TrustedVaultRequest& other) = delete;
  TrustedVaultRequest& operator=(const TrustedVaultRequest& other) = delete;
  ~TrustedVaultRequest() override;

  // Attempts to fetch access token and sends the request if fetch was
  // successful or populates error into ResultCallback otherwise. Should be
  // called at most once.
  void FetchAccessTokenAndSendRequest(CompletionCallback callback);

 private:
  void OnAccessTokenFetched(
      TrustedVaultAccessTokenFetcher::AccessTokenInfoOrError
          access_token_info_or_error);
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> CreateURLLoader(
      const std::string& access_token) const;

  bool CanRetry() const;
  void ScheduleRetry();
  void Retry();

  // Running |completion_callback_| may cause destroying of this object, so all
  // callers of this method must not run any code afterwards.
  void RunCompletionCallbackAndMaybeDestroySelf(
      HttpStatus status,
      const std::string& response_body);

  const CoreAccountId account_id_;
  const HttpMethod http_method_;
  const GURL request_url_;
  const absl::optional<std::string> serialized_request_proto_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher_;
  const TrustedVaultURLFetchReasonForUMA reason_for_uma_;
  const base::TimeTicks max_retry_time_;

  net::BackoffEntry backoff_entry_;

  CompletionCallback completion_callback_;

  // Initialized lazily upon successful access token fetch.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<TrustedVaultRequest> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_REQUEST_H_
