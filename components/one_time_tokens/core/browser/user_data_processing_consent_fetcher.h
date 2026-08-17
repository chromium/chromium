// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_USER_DATA_PROCESSING_CONSENT_FETCHER_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_USER_DATA_PROCESSING_CONSENT_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
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

class OneTimeTokenLogSink;

// Fetches user data processing consent states from the backend.
class UserDataProcessingConsentFetcher {
 public:
  using Callback =
      base::OnceCallback<void(std::optional<UserDataProcessingConsentStates>)>;

  UserDataProcessingConsentFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager,
      OneTimeTokenLogSink* log_sink = nullptr);
  ~UserDataProcessingConsentFetcher();

  UserDataProcessingConsentFetcher(const UserDataProcessingConsentFetcher&) =
      delete;
  UserDataProcessingConsentFetcher& operator=(
      const UserDataProcessingConsentFetcher&) = delete;

  // Starts the request to fetch consent states.
  void Start(Callback callback);

 private:
  void StartAccessTokenFetch();
  void OnAccessTokenFetched(GoogleServiceAuthError error,
                            signin::AccessTokenInfo info);
  void StartNetworkRequest(signin::AccessTokenInfo info);
  void OnResponseBytesReceived(std::optional<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ref<signin::IdentityManager> identity_manager_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  Callback callback_;
  raw_ptr<OneTimeTokenLogSink> log_sink_ = nullptr;

  base::WeakPtrFactory<UserDataProcessingConsentFetcher> weakptr_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_USER_DATA_PROCESSING_CONSENT_FETCHER_H_
