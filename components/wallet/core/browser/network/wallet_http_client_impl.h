// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_IMPL_H_
#define COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_IMPL_H_

#include <list>

#include "base/containers/queue.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace wallet {

class WalletRequest;

class WalletHttpClientImpl : public WalletHttpClient {
 public:
  WalletHttpClientImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~WalletHttpClientImpl() override;

  WalletHttpClientImpl(const WalletHttpClientImpl&) = delete;
  WalletHttpClientImpl& operator=(const WalletHttpClientImpl&) = delete;

  // WalletHttpClient:
  void UpsertPublicPass(Pass pass, UpsertPublicPassCallback callback) override;
  void UpsertPrivatePass(PrivatePass pass,
                         UpsertPrivatePassCallback callback) override;
  void GetUnmaskedPass(std::string_view pass_id,
                       GetUnmaskedPassCallback callback) override;

 private:
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;
  using TokenReadyCallback =
      base::OnceCallback<void(std::optional<std::string>)>;

  void SendRequest(std::unique_ptr<WalletRequest> request);

  // Initiates a request for a new OAuth token. If the request succeeds, this
  // runs `on_fetched` with the retrieved token.
  void GetAuthToken(TokenReadyCallback on_fetched);

  // Called when the access token is fetched. Runs all pending callbacks.
  void OnTokenFetched(GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);

  // Continues sending the request after the token is fetched.
  void SendRequestInternal(std::unique_ptr<WalletRequest> request,
                           std::optional<std::string> access_token);

  // Called when the `SimpleURLLoader` referenced by `it` completes. Removes the
  // loader from `active_loaders_` and runs `response_callback` with the
  // response body or an error.
  void OnSimpleLoaderComplete(UrlLoaderList::iterator it,
                              std::unique_ptr<WalletRequest> request,
                              base::TimeTicks request_start,
                              std::optional<std::string> response_body);

  // The IdentityManager instance for the current profile.
  const raw_ref<signin::IdentityManager> identity_manager_;

  // The factory used to create URLLoaders.
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Owns the ongoing SimpleURLLoaders, keeping them alive until completion.
  UrlLoaderList active_loaders_;

  // Fetches OAuth2 access tokens for the primary account.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Stores callbacks that raced to get an auth token to run them once ready.
  base::queue<TokenReadyCallback> pending_token_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WalletHttpClientImpl> weak_ptr_factory_{this};
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_NETWORK_WALLET_HTTP_CLIENT_IMPL_H_
