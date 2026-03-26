// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCHER_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCHER_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace one_time_tokens {

// A holder object for all the necessary state to make a single request to the
// Gmail OTP endpoint.
class EmailOneTimeTokenFetcher {
 public:
  using ServerResponseCallback = base::OnceCallback<void(
      base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>;

  EmailOneTimeTokenFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string encrypted_message_reference);
  ~EmailOneTimeTokenFetcher();

  // Starts the request to the Gmail OTP endpoint.
  void Start(ServerResponseCallback callback);

 private:
  // Starts the network request to the Gmail OTP endpoint.
  void StartOneTimeTokenServiceCall();

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

  // Retain internal copy of encrypted_message_reference.
  std::string encrypted_message_reference_;

  // A final callback for when the request completes.
  ServerResponseCallback callback_;

  // Weak pointer factory (must be last member in class).
  base::WeakPtrFactory<EmailOneTimeTokenFetcher> weakptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCHER_H_
