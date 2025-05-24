// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

class GoogleServiceAuthError;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace autofill::payments {

// Util class to help fetch the latest access token for Google Payments server.
class PaymentsAccessTokenFetcher {
 public:
  using FinishCallback = base::OnceCallback<void(
      const std::variant<GoogleServiceAuthError, std::string>&)>;

  explicit PaymentsAccessTokenFetcher(
      signin::IdentityManager& identity_manager);
  PaymentsAccessTokenFetcher(const PaymentsAccessTokenFetcher&) = delete;
  PaymentsAccessTokenFetcher& operator=(const PaymentsAccessTokenFetcher&) =
      delete;
  ~PaymentsAccessTokenFetcher();

  // Returns the access token. If `invalidate_old` is true, or there is no
  // access token cached, initiates an access token fetching request. `callback`
  // will be invoked when fetching finishes.
  void GetAccessToken(bool invalidate_old, FinishCallback callback);

  // Exposed for testing.
  void set_access_token_for_testing(const std::string& access_token) {
    access_token_ = access_token;
  }

 private:
  // Function invoked when access token fetching is finished.
  void AccessTokenFetchFinished(GoogleServiceAuthError error,
                                signin::AccessTokenInfo access_token_info);

  const raw_ref<signin::IdentityManager> identity_manager_;

  // The access token fetcher. Set when fetching starts, reset when fetching is
  // finished.
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;

  // The callback invoked after fetching finishes.
  FinishCallback callback_;

  // The access token, or empty if not fetched.
  std::string access_token_;

  base::WeakPtrFactory<PaymentsAccessTokenFetcher> weak_ptr_factory_{this};
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_ACCESS_TOKEN_FETCHER_H_
