// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class GoogleServiceAuthError;

namespace network {
struct ResourceRequest;
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

// Identifier expected by the server in the body of suggestion requests.
constexpr char kEnterpriseSearchAggregatorExperimentId[] = "103277467";

// A service to fetch suggestions from the search aggregator endpoint URL.
class EnterpriseSearchAggregatorSuggestionsService : public KeyedService {
 public:
  EnterpriseSearchAggregatorSuggestionsService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~EnterpriseSearchAggregatorSuggestionsService() override;
  EnterpriseSearchAggregatorSuggestionsService(
      const EnterpriseSearchAggregatorSuggestionsService&) = delete;
  EnterpriseSearchAggregatorSuggestionsService& operator=(
      const EnterpriseSearchAggregatorSuggestionsService&) = delete;

  // TODO(crbug.com/385756623): Factor out callback methods so it can be used
  //   across document_suggestions_service and
  //   enterprise_search_aggregator_suggestions_service.
  using CreationCallback =
      base::RepeatingCallback<void(network::ResourceRequest* request)>;
  using StartCallback = base::RepeatingCallback<void(
      int request_index,
      std::unique_ptr<network::SimpleURLLoader> loader,
      const std::string& request_body)>;
  using CompletionCallback =
      base::RepeatingCallback<void(const network::SimpleURLLoader* source,
                                   int request_index,
                                   std::optional<std::string> response_body)>;

  // Creates one request for each list within `suggestion_types`. Each request
  // will request types in `suggestion_types[i]`.
  void CreateEnterpriseSearchAggregatorSuggestionsRequest(
      const std::u16string& query,
      const GURL& suggest_url,
      std::vector<int> callback_indexes,
      std::vector<std::vector<int>> suggestion_types,
      CreationCallback creation_callback,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  // Stops creating the request. Already created requests aren't affected.
  void StopCreatingEnterpriseSearchAggregatorSuggestionsRequest();

 private:
  // Called when an access token request completes (successfully or not).
  void AccessTokenAvailable(
      std::vector<std::unique_ptr<network::ResourceRequest>> requests,
      const std::u16string& query,
      std::vector<int> callback_indexes,
      std::vector<std::vector<int>> suggestion_types,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      StartCallback start_callback,
      CompletionCallback completion_callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // TODO(crbug.com/385756623): Factor out this method so it can be used across
  //   document_suggestions_service and
  //   enterprise_search_aggregator_suggestions_service.
  void StartDownloadAndTransferLoader(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      int request_index,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // This will outlive this instance because of the factory dependencies.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Helper for fetching OAuth2 access tokens. Non-null when we have a token
  // available, or while a token fetch is in progress.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_H_
