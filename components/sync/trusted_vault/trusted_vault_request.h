// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_REQUEST_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_REQUEST_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/driver/trusted_vault_histograms.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

struct CoreAccountId;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
}  // namespace signin

namespace syncer {

class TrustedVaultAccessTokenFetcher;

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
    // Reported when access token fetch attempt was failed and request wasn't
    // sent.
    kAccessTokenFetchingFailure,
    // Reported when other network and http errors occur.
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
  TrustedVaultRequest(
      HttpMethod http_method,
      const GURL& request_url,
      const absl::optional<std::string>& serialized_request_proto,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TrustedVaultURLFetchReasonForUMA reason_for_uma);
  TrustedVaultRequest(const TrustedVaultRequest& other) = delete;
  TrustedVaultRequest& operator=(const TrustedVaultRequest& other) = delete;
  ~TrustedVaultRequest() override;

  // Attempts to fetch access token and sends the request if fetch was
  // successful or populates error into ResultCallback otherwise. Should be
  // called at most once.
  void FetchAccessTokenAndSendRequest(
      const CoreAccountId& account_id,
      TrustedVaultAccessTokenFetcher* access_token_fetcher,
      CompletionCallback callback);

 private:
  void OnAccessTokenFetched(
      absl::optional<signin::AccessTokenInfo> access_token_info);
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> CreateURLLoader(
      const std::string& access_token) const;

  // Running |completion_callback_| may cause destroying of this object, so all
  // callers of this method must not run any code afterwards.
  void RunCompletionCallbackAndMaybeDestroySelf(
      HttpStatus status,
      const std::string& response_body);

  const HttpMethod http_method_;
  const GURL request_url_;
  const absl::optional<std::string> serialized_request_proto_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const TrustedVaultURLFetchReasonForUMA reason_for_uma_;

  CompletionCallback completion_callback_;

  // Initialized lazily upon successful access token fetch.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<TrustedVaultRequest> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_REQUEST_H_
