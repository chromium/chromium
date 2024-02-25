// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_FRONTEND_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_FRONTEND_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}

namespace trusted_vault {

// Allows fetching access token for primary account on UI thread.
class TrustedVaultAccessTokenFetcherFrontend
    : public signin::IdentityManager::Observer {
 public:
  // |identity_manager| must not be null and must outlive this object.
  explicit TrustedVaultAccessTokenFetcherFrontend(
      signin::IdentityManager* identity_manager);
  TrustedVaultAccessTokenFetcherFrontend(
      const TrustedVaultAccessTokenFetcherFrontend& other) = delete;
  TrustedVaultAccessTokenFetcherFrontend& operator=(
      const TrustedVaultAccessTokenFetcherFrontend& other) = delete;
  ~TrustedVaultAccessTokenFetcherFrontend() override;

  // Returns WeakPtr to |this|, pointer will be invalidated inside dtor.
  base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend> GetWeakPtr();

  // Asynchronously fetches an access token for |account_id|. If |account_id|
  // doesn't represent current primary account, |callback| is called immediately
  // with std::nullopt. If primary account changes before access token fetched,
  // |callback| is called with std::nullopt.
  void FetchAccessToken(const CoreAccountId& account_id,
                        TrustedVaultAccessTokenFetcher::TokenCallback callback);

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  // Updates |primary_account_| and runs |pending_requests_| in case
  // |primary_account_| was changed.
  void UpdatePrimaryAccountIfNeeded();

  // Starts new access fetch. |ongoing_access_token_fetch_| must be null when
  // calling this method.
  void StartAccessTokenFetch();

  // Handles access token fetch completion. Runs |pending_requests_| with
  // |access_token_info| on success and with std::nullopt otherwise.
  void OnAccessTokenFetchCompleted(GoogleServiceAuthError error,
                                   signin::AccessTokenInfo access_token_info);

  // Helper method to run and clear |pending_requests_|.
  void FulfillPendingRequests(
      TrustedVaultAccessTokenFetcher::AccessTokenInfoOrError
          access_token_info_or_error);

  // Never null.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Pending request for an access token. Non-null iff there is a request
  // ongoing.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      ongoing_access_token_fetch_;

  // Current primary account.
  CoreAccountId primary_account_;

  // Contains callbacks passed to FetchAccessToken which are not yet satisfied.
  std::vector<TrustedVaultAccessTokenFetcher::TokenCallback> pending_requests_;

  base::WeakPtrFactory<TrustedVaultAccessTokenFetcherFrontend>
      weak_ptr_factory_{this};
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_ACCESS_TOKEN_FETCHER_FRONTEND_H_
