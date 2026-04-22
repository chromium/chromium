// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCHER_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCHER_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace one_time_tokens {

// Header name and value for user-facing criticality.
inline constexpr char kOneTimeTokenServiceCriticalityHeaderName[] =
    "x-goog-ext-174067345-bin";
inline constexpr char kOneTimeTokenServiceCriticalityHeaderValue[] = "CgIIAg==";

// A holder object for all the necessary state to make a single request to the
// Gmail OTP endpoint.
class EmailOneTimeTokenFetcher {
 public:
  using ServerResponseCallback = base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>;

  EmailOneTimeTokenFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager,
      std::string encrypted_message_reference);
  ~EmailOneTimeTokenFetcher();

  // Starts the request to the Gmail OTP endpoint.
  void Start(ServerResponseCallback callback);

 private:
  // Finalizes the fetch by invoking the callback with the |result| and
  // notifying the backend that this fetcher can be destroyed.
  // IMPORTANT: This method must be called last, as the object will be deleted
  // during the callback execution.
  void InvokeCallbackAndDestroySelf(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError> result);

  // Starts fetching the access token.
  void StartAccessTokenFetch();

  // Callback for when the access token fetch completes.
  void OnAccessTokenFetched(GoogleServiceAuthError error,
                            signin::AccessTokenInfo info);

  // Starts the network request to the Gmail OTP endpoint.
  void StartOneTimeTokenServiceCall(signin::AccessTokenInfo info);

  // Callback for when the network request to the Gmail OTP endpoint completes.
  void OnResponseBytesFromOneTimeTokenService(
      std::optional<std::string> response_body);

  // Parses the response proto and extracts the OneTimeToken value from it.
  base::expected<OneTimeToken, OneTimeTokenRetrievalError>
  ExtractOneTimeTokenValueFromResponse(const std::string& response_body);

  // Shared URL loader factory for the network request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Simple URL loader required for the network request to the Gmail OTP
  // endpoint.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Identity manager for the authentication.
  // IdentityManager is a KeyedService, and GmailOtpBackend (the only user of
  // this class) is dependent on IdentityManager, so IdentityManager will
  // outlive it.
  raw_ref<signin::IdentityManager> identity_manager_;

  // Access token fetcher for the authentication.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Retain internal copy of encrypted_message_reference.
  std::string encrypted_message_reference_;

  // A final callback for when the request completes.
  ServerResponseCallback callback_;

  // Weak pointer factory (must be last member in class).
  base::WeakPtrFactory<EmailOneTimeTokenFetcher> weakptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCHER_H_
