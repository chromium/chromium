// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_NETWORK_ANNOTATION_INDEX_CLIENT_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_NETWORK_ANNOTATION_INDEX_CLIENT_H_

#include <list>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
struct ResourceRequest;
}  // namespace network

namespace signin {
class IdentityManager;
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace multistep_filter {

class MultistepFilterLogRouter;
struct FilterAnnotation;
struct FilterSuggestionCandidate;

// `NetworkAnnotationIndexClient` serves as the dedicated network and
// translation layer between the `multistep_filter` component and the remote
// `SiteAutomationIndexServer`.
//
// This class abstracts away the complexities of network communication and
// Protocol Buffer handling from the core `multistep_filter` logic. It
// achieves this by:
//  - Accepting standard C++ types as input and serializing them into the
//    specific Protocol Buffer format required by the backend API.
//  - Managing asynchronous network requests, including internal handling of
//    network state, timeouts, and HTTP response codes.
//  - Deserializing the raw Protocol Buffer byte stream received in the
//    response.
//  - Extracting client-relevant data from the deserialized proto and
//    packaging it into clean, lightweight C++ structs for callers.
class NetworkAnnotationIndexClient : public AnnotationIndexClient {
 public:
  static std::unique_ptr<NetworkAnnotationIndexClient> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      MultistepFilterLogRouter* log_router);

  NetworkAnnotationIndexClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      MultistepFilterLogRouter* log_router);
  ~NetworkAnnotationIndexClient() override;

  // AnnotationIndexClient overrides:
  void GetFilterSuggestionCandidates(
      const GURL& url,
      base::span<const FilterAnnotation> filter_annotations,
      base::OnceCallback<
          void(std::optional<std::vector<FilterSuggestionCandidate>>)> callback,
      int64_t navigation_id) override;

  void GetSupportedTasks(
      const GURL& url,
      base::OnceCallback<void(std::vector<std::string>)> callback,
      int64_t navigation_id) override;

  void ExtractFilterAnnotation(
      const GURL& url,
      base::OnceCallback<void(std::optional<FilterAnnotation>)> callback,
      int64_t navigation_id) override;

 private:
  friend class NetworkAnnotationIndexClientTestApi;

  using SimpleURLLoaderList =
      std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // Centralized helper to launch a network request. It creates the loader,
  // stores it in `active_url_loaders_` to keep it alive, and dispatches the
  // network request. It forwards the raw response to the provided callback.
  void ExecuteRequest(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      base::OnceCallback<void(std::optional<std::string>, int)> callback,
      int64_t navigation_id,
      std::string host);

  // Invoked when `SimpleURLLoader` finishes. Cleans up the specific loader
  // from `active_url_loaders_` and forwards the raw response to the parser.
  void OnSimpleURLLoaderComplete(
      SimpleURLLoaderList::iterator loader_it,
      base::OnceCallback<void(std::optional<std::string>, int)> callback,
      int64_t navigation_id,
      std::string host,
      std::optional<std::string> response_body);

  // Returns the base URL for the `SiteAutomationIndexServer` server APIs.
  GURL GetIndexServerApiBaseUrl() const;

  // Callback invoked when the access token is fetched.
  void OnAccessTokenFetched(
      base::UnguessableToken fetcher_id,
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      base::OnceCallback<void(std::optional<std::string>, int)> callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info,
      int64_t navigation_id,
      std::string host);

  // Starts the URL loader to perform the network request.
  void StartLoader(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      base::OnceCallback<void(std::optional<std::string>, int)> callback,
      int64_t navigation_id,
      std::string host);

  // The factory used to instantiate network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Holds all currently active network requests. Removing a loader from this
  // list immediately cancels its underlying network traffic.
  SimpleURLLoaderList active_url_loaders_;

  // The identity manager used to fetch access tokens.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // Holds all currently active access token fetchers.
  std::map<base::UnguessableToken, std::unique_ptr<signin::AccessTokenFetcher>>
      active_fetchers_;

  // Log router for the internals page.
  raw_ptr<MultistepFilterLogRouter> log_router_;

  // This should be kept at the end so that it is the first member to be
  // destroyed.
  base::WeakPtrFactory<NetworkAnnotationIndexClient> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_NETWORK_ANNOTATION_INDEX_CLIENT_H_
