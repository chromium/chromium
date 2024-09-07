// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_IMPL_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_IMPL_H_

#include <list>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "url/gurl.h"

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace plus_addresses {

// This endpoint is used for most plus-address operations.
inline constexpr std::string_view kServerPlusProfileEndpoint = "v1/profiles";
inline constexpr std::string_view kServerReservePlusAddressEndpoint =
    "v1/profiles/reserve";
inline constexpr std::string_view kServerCreatePlusAddressEndpoint =
    "v1/profiles/create";
inline constexpr std::string_view kServerPreallocatePlusAddressEndpoint =
    "v1/emailAddresses/reserve";

class PlusAddressHttpClientImpl : public PlusAddressHttpClient {
 public:
  PlusAddressHttpClientImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  PlusAddressHttpClientImpl(const PlusAddressHttpClientImpl&) = delete;
  PlusAddressHttpClientImpl(PlusAddressHttpClientImpl&&) = delete;
  PlusAddressHttpClientImpl& operator=(const PlusAddressHttpClientImpl&) =
      delete;
  PlusAddressHttpClientImpl& operator=(PlusAddressHttpClientImpl&&) = delete;
  ~PlusAddressHttpClientImpl() override;

  // PlusAddressHttpClient:
  void ReservePlusAddress(const url::Origin& origin,
                          bool refresh,
                          PlusAddressRequestCallback on_completed) override;
  void ConfirmPlusAddress(const url::Origin& origin,
                          const PlusAddress& plus_address,
                          PlusAddressRequestCallback on_completed) override;
  void PreallocatePlusAddresses(
      PreallocatePlusAddressesCallback callback) override;
  void Reset() override;

 private:
  friend class PlusAddressHttpClientImplTestApi;

  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;
  using TokenReadyCallback =
      base::OnceCallback<void(std::optional<std::string>)>;

  // Initiates a request for a new OAuth token. If the request succeeds, this
  // runs `on_fetched` with the retrieved token. Must be run on the UI thread.
  void GetAuthToken(TokenReadyCallback on_fetched);

  // The actual implementations of the interface methods. The overridden
  // interface methods (`ReservePlusAddress`, ...) call `GetAuthToken` with the
  // methods below passed in as the `TokenReadyCallback`.
  void ReservePlusAddressInternal(const url::Origin& origin,
                                  bool refresh,
                                  PlusAddressRequestCallback on_completed,
                                  std::optional<std::string> auth_token);
  void ConfirmPlusAddressInternal(const url::Origin& origin,
                                  const PlusAddress& plus_address,
                                  PlusAddressRequestCallback on_completed,
                                  std::optional<std::string> auth_token);
  void PreallocatePlusAddressesInternal(
      PreallocatePlusAddressesCallback callback,
      std::optional<std::string> auth_token);

  // This is shared by the `ReservePlusAddress` and `ConfirmPlusAddress` methods
  // since they both use `loaders_for_creation_` and have the same return type.
  void OnReserveOrConfirmPlusAddressComplete(
      UrlLoaderList::iterator it,
      PlusAddressNetworkRequestType type,
      base::TimeTicks request_start,
      PlusAddressRequestCallback on_completed,
      std::unique_ptr<std::string> response);

  void OnPreallocationComplete(UrlLoaderList::iterator it,
                               base::TimeTicks request_start,
                               PreallocatePlusAddressesCallback on_completed,
                               std::unique_ptr<std::string> response);

  // Runs callback and any pending_callbacks_ blocked on the token.
  void OnTokenFetched(TokenReadyCallback callback,
                      GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);

  // Creates a resource request for a given `endpoint`, `method` and
  // `auth_token`.
  std::unique_ptr<network::ResourceRequest> CreateRequest(
      std::string_view endpoint,
      std::string_view method,
      std::string_view auth_token) const;

  // The IdentityManager instance for the current profile.
  const raw_ref<signin::IdentityManager> identity_manager_;

  // Used to make HTTP requests.
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const std::optional<GURL> server_url_;

  const signin::ScopeSet scopes_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores callbacks that raced to get an auth token to run them once ready.
  base::queue<TokenReadyCallback> pending_callbacks_;

  // Loaders used for Create, Reserve, and Preallocate calls.
  UrlLoaderList loaders_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_IMPL_H_
