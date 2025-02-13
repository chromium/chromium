// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_H_

#include <memory>
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
      base::OnceCallback<void(network::ResourceRequest* request)>;
  using StartCallback =
      base::OnceCallback<void(std::unique_ptr<network::SimpleURLLoader> loader,
                              const std::string& request_body)>;
  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              std::unique_ptr<std::string> response_body)>;

  void CreateEnterpriseSearchAggregatorSuggestionsRequest(
      const std::u16string& query,
      const GURL& suggest_url,
      CreationCallback creation_callback,
      StartCallback start_callback,
      CompletionCallback completion_callback,
      bool in_keyword_mode);

 private:
  // Called when an access token request completes (successfully or not).
  void AccessTokenAvailable(std::unique_ptr<network::ResourceRequest> request,
                            std::string request_body,
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
