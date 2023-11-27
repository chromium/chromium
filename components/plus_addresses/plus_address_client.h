// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_

#include <list>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/time/default_clock.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Clock;
}

namespace network {
class SimpleURLLoader;
}

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace plus_addresses {

// This endpoint is used for most plus-address operations.
constexpr char kServerPlusProfileEndpoint[] = "v1/profiles";
constexpr char kServerReservePlusAddressEndpoint[] = "v1/profiles/reserve";
constexpr char kServerCreatePlusAddressEndpoint[] = "v1/profiles/create";

// A move-only class for communicating with a remote plus-address server.
class PlusAddressClient {
 public:
  using TokenReadyCallback =
      base::OnceCallback<void(absl::optional<std::string>)>;
  PlusAddressClient(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PlusAddressClient();
  PlusAddressClient(PlusAddressClient&&);
  PlusAddressClient& operator=(PlusAddressClient&&);

  // Initiates a request to get a plus address for use on `origin` and only
  // runs `callback` with a plus address if the request to the server
  // completes successfully and returns the expected response.
  //
  // TODO (crbug.com/1467623): Should callback be run if the request fails?
  void CreatePlusAddress(const url::Origin& origin,
                         PlusAddressCallback callback);

  // Initiates a request to get a plus address for use on `origin` and runs
  // `on_completed` when the request is completed.
  void ReservePlusAddress(const url::Origin& origin,
                          PlusAddressRequestCallback on_completed);

  // Initiates a request to confirm `plus_address` for use on `origin` and runs
  // `on_completed` when the request is completed.
  void ConfirmPlusAddress(const url::Origin& origin,
                          const std::string& plus_address,
                          PlusAddressRequestCallback on_completed);

  // Initiates a request to get all plus addresses from the remote enterprise-
  // specified server and only runs callback with them if the request to
  // the server completes successfully and returns the expected response.
  void GetAllPlusAddresses(PlusAddressMapCallback callback);

  // Initiates a request for a new OAuth token. If the request succeeds, this
  // runs `on_fetched` with the retrieved token. Must be run on the UI thread.
  void GetAuthToken(TokenReadyCallback on_fetched);

  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  absl::optional<GURL> GetServerUrlForTesting() const { return server_url_; }

 private:
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  void CreatePlusAddressInternal(const url::Origin& origin,
                                 PlusAddressCallback callback,
                                 absl::optional<std::string> auth_token);
  void ReservePlusAddressInternal(const url::Origin& origin,
                                  PlusAddressRequestCallback on_completed,
                                  absl::optional<std::string> auth_token);
  void ConfirmPlusAddressInternal(const url::Origin& origin,
                                  const std::string& plus_address,
                                  PlusAddressRequestCallback on_completed,
                                  absl::optional<std::string> auth_token);
  void GetAllPlusAddressesInternal(PlusAddressMapCallback callback,
                                   absl::optional<std::string> auth_token);

  // Only used by CreatePlusAddress.
  void OnCreatePlusAddressComplete(UrlLoaderList::iterator it,
                                   base::Time request_start,
                                   PlusAddressCallback on_completed,
                                   std::unique_ptr<std::string> response);

  // This is shared by the Reserve and Confirm PlusAddress methods since
  // they both use `loaders_for_creation_` and have the same return type.
  void OnReserveOrConfirmPlusAddressComplete(
      UrlLoaderList::iterator it,
      PlusAddressNetworkRequestType type,
      base::Time request_start,
      PlusAddressRequestCallback on_completed,
      std::unique_ptr<std::string> response);
  void OnGetAllPlusAddressesComplete(base::Time request_start,
                                     PlusAddressMapCallback callback,
                                     std::unique_ptr<std::string> response);
  // Runs callback and any pending_callbacks_ blocked on the token.
  void OnTokenFetched(TokenReadyCallback callback,
                      GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);

  // The IdentityManager instance for the signed-in user.
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Used to make HTTP requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // List of loaders used by the creation flow (CreatePlusAddress). We use a
  // list of loaders instead of a single one to handle several requests made
  // quickly across different tabs.
  std::list<std::unique_ptr<network::SimpleURLLoader>> loaders_for_creation_;
  // A loader used infrequently for calls to GetAllPlusAddresses which keeps
  // the PlusAddressService synced with the remote server.
  std::unique_ptr<network::SimpleURLLoader> loader_for_sync_;

  absl::optional<GURL> server_url_;
  signin::ScopeSet scopes_;
  // Stores callbacks that raced to get an auth token to run them once ready.
  base::queue<TokenReadyCallback> pending_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_
