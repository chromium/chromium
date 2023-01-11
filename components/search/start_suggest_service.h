// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_START_SUGGEST_SERVICE_H_
#define COMPONENTS_SEARCH_START_SUGGEST_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

class AutocompleteSchemeClassifier;

class SearchProviderObserver;

class TemplateURLService;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class QuerySuggestion {
 public:
  bool operator==(const QuerySuggestion& other) const {
    return query == other.query && destination_url == other.destination_url;
  }
  bool operator!=(const QuerySuggestion& other) const {
    return !(this == &other);
  }

  // Query suggestion.
  std::u16string query;
  // Destination url of query.
  GURL destination_url;
};

using QuerySuggestions = std::vector<QuerySuggestion>;

// Service that retrieves query suggestions for the Start/NTP surface when
// Google is the default search provider.
class StartSuggestService : public KeyedService {
 public:
  StartSuggestService(
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<AutocompleteSchemeClassifier> scheme_classifier,
      const std::string& application_country,
      const std::string& application_locale,
      const GURL& request_initiator_url);
  ~StartSuggestService() override;
  StartSuggestService(const StartSuggestService&) = delete;
  StartSuggestService& operator=(const StartSuggestService&) = delete;

  using SuggestResultCallback = base::OnceCallback<void(QuerySuggestions)>;
  // If Google is the default search provider, asynchronously requests
  // query suggestions from the server and returns them to the caller in
  // `callback`. If `fetch_from_server` is true, any saved suggestions from a
  // previous fetch are bypassed and a new request will be sent.
  void FetchSuggestions(const TemplateURLRef::SearchTermsArgs& args,
                        SuggestResultCallback callback,
                        bool fetch_from_server = false);

 protected:
  // Called when the default search provider changes.
  void SearchProviderChanged();

  // Returns the server request URL.
  GURL GetRequestURL(const TemplateURLRef::SearchTermsArgs& search_terms_args);
  // Returns the destination url for `query` for `search_provider`.
  GURL GetQueryDestinationURL(const std::u16string& query,
                              const TemplateURL* search_provider);

  virtual SearchProviderObserver* search_provider_observer();

 private:
  // Handles request response from the server.
  void SuggestResponseLoaded(network::SimpleURLLoader* loader,
                             SuggestResultCallback callback,
                             std::unique_ptr<std::string> response);
  void SuggestionsParsed(SuggestResultCallback callback,
                         data_decoder::DataDecoder::ValueOrError result);

  // Cannot be null. Must outlive `this`.
  raw_ptr<TemplateURLService> template_url_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<AutocompleteSchemeClassifier> scheme_classifier_;

  // The country locale used by this client.
  const std::string application_country_;

  // The language locale used by this client.
  const std::string application_locale_;

  // Indicates what page is initiating the requests by this service.
  const GURL request_initiator_url_;

  // Used to ensure requests are only made if Google is the default search
  // engine.
  std::unique_ptr<SearchProviderObserver> search_provider_observer_;

  // Holds all SimpleURLLoader instances started by this service that have not
  // received responses yet.
  std::vector<std::unique_ptr<network::SimpleURLLoader>> loaders_;

  // Stores last fetched suggestions.
  std::map<std::string, QuerySuggestions> suggestions_cache_;

  base::WeakPtrFactory<StartSuggestService> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SEARCH_START_SUGGEST_SERVICE_H_
