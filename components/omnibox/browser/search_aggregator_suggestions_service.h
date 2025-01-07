// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

// A service to fetch suggestions from the search aggregator endpoint URL.
class SearchAggregatorSuggestionsService : public KeyedService {
 public:
  explicit SearchAggregatorSuggestionsService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~SearchAggregatorSuggestionsService() override;
  SearchAggregatorSuggestionsService(
      const SearchAggregatorSuggestionsService&) = delete;
  SearchAggregatorSuggestionsService& operator=(
      const SearchAggregatorSuggestionsService&) = delete;

  // TODO(crbug.com/385756623): Factor out callback methods so it can be used
  //   across document_suggestions_service and
  //   search_aggregator_suggestions_service.
  using CreationCallback =
      base::OnceCallback<void(network::ResourceRequest* request)>;
  using StartCallback =
      base::OnceCallback<void(std::unique_ptr<network::SimpleURLLoader> loader,
                              const std::string& request_body)>;
  using CompletionCallback =
      base::OnceCallback<void(const network::SimpleURLLoader* source,
                              std::unique_ptr<std::string> response_body)>;

  void CreateSearchAggregatorSuggestionsRequest(
      const GURL& suggest_url,
      const std::string& request_body,
      CreationCallback creation_callback,
      StartCallback start_callback,
      CompletionCallback completion_callback);

 private:
  // TODO(crbug.com/385756623): Factor out this method so it can be used across
  //   document_suggestions_service and search_aggregator_suggestions_service.
  void StartDownloadAndTransferLoader(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      StartCallback start_callback,
      CompletionCallback completion_callback);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCH_AGGREGATOR_SUGGESTIONS_SERVICE_H_
