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
  PlusAddressClient(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PlusAddressClient();
  PlusAddressClient(PlusAddressClient&&);
  PlusAddressClient& operator=(PlusAddressClient&&);

  // Initiates a request to get a plus address for use on `site` and only
  // runs `callback` with a plus address if the request to the server
  // completes successfully and returns the expected response.
  //
  // TODO (crbug.com/1467623): Should callback be run if the request fails?
  void CreatePlusAddress(const std::string& site, PlusAddressCallback callback);

  // Initiates a request to get a plus address for use on `site` and only
  // runs `callback` with a plus address if the request to the server
  // completes successfully and returns the expected response.
  //
  // TODO (crbug.com/1467623): Should callback be run if the request fails?
  void ReservePlusAddress(const std::string& site,
                          PlusAddressCallback callback);

  // Initiates a request to confirm `plus_address` for use on `site` and only
  // runs `callback` with the plus address if the request to the server
  // completes successfully and returns the expected response.
  //
  // TODO (crbug.com/1467623): Should callback be run if the request fails?
  void ConfirmPlusAddress(const std::string& site,
                          const std::string& plus_address,
                          PlusAddressCallback callback);

  // Initiates a request to get all plus addresses from the remote enterprise-
  // specified server and only runs callback with them if the request to
  // the server completes successfully and returns the expected response.
  void GetAllPlusAddresses(PlusAddressMapCallback callback);

  // Initiates a request for a new OAuth token. If the request succeeds, this
  // stores the token in `access_token_info_` and runs `on_fetched`.
  void GetAuthToken(base::OnceClosure on_fetched);

  void SetAccessTokenInfoForTesting(signin::AccessTokenInfo info) {
    access_token_info_ = info;
  }
  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  absl::optional<GURL> GetServerUrlForTesting() const { return server_url_; }

 private:
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // This is shared by the Create, Reserve, and ConfirmPlusAddress methods since
  // they all use `loaders_for_creation_` and have the same return type.
  void OnCreateOrReservePlusAddressComplete(
      UrlLoaderList::iterator it,
      PlusAddressNetworkRequestType type,
      base::Time request_start,
      PlusAddressCallback callback,
      std::unique_ptr<std::string> response);
  void OnGetAllPlusAddressesComplete(base::Time request_start,
                                     PlusAddressMapCallback callback,
                                     std::unique_ptr<std::string> response);
  // Initiates a network request for an OAuth token, and may only be
  // called by GetAuthToken. This also must be run on the UI thread.
  void RequestAuthToken();
  void OnTokenFetched(GoogleServiceAuthError error,
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
  signin::AccessTokenInfo access_token_info_;
  GoogleServiceAuthError access_token_request_error_;
  signin::ScopeSet scopes_;
  // Stores callbacks to be run once `access_token_info_` is retrieved.
  base::queue<base::OnceClosure> pending_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_
