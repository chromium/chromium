// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"

namespace network {
class SimpleURLLoader;
}

class AutocompleteInput;
class AutocompleteProviderClient;
class AutocompleteProviderDebouncer;
class AutocompleteProviderListener;
class TemplateURL;
class TemplateURLService;

class EnterpriseSearchAggregatorProvider : public AutocompleteProvider {
 public:
  EnterpriseSearchAggregatorProvider(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;

 private:
  friend class FakeEnterpriseSearchAggregatorProvider;

  ~EnterpriseSearchAggregatorProvider() override;

  // The types of suggestions provided by the response body.
  enum class SuggestionType {
    QUERY,
    PEOPLE,
    CONTENT,
  };

  // Determines whether the profile/session/window meet the feature
  // prerequisites.
  bool IsProviderAllowed(const AutocompleteInput& input);

  // Called by `debouncer_`, queued when `Start()` is called.
  void Run();

  // Callback for when the loader is available with a valid token. Takes
  // ownership of the loader.
  void RequestStarted(std::unique_ptr<network::SimpleURLLoader> loader);

  // Called when the network request for suggestions has completed.
  void RequestCompleted(const network::SimpleURLLoader* source,
                        const int response_code,
                        std::unique_ptr<std::string> response_body);

  // The function updates `matches_` with data parsed from `json_data`.
  // The update is not performed if `json_data` is invalid.
  // Returns whether `matches_` changed.
  bool UpdateResults(const std::string& json_data);

  // Parses enterprise search aggregator response JSON.
  void ParseEnterpriseSearchAggregatorSearchResults(
      const base::Value& root_val);

  // Helper method to parse query, people, and content suggestions.
  // Example:
  //   Given a response with one query suggestion:
  //    {
  //     "querySuggestions": [{
  //       "suggestion": "hello",
  //       "dataStore": [project/1]
  //      }]
  //     }.
  // `matches` would contain one `match` with the following properties:
  //  - `match.type` = `AutocompleteMatchType::SEARCH_SUGGEST`,
  //  - `match.contents` = "hello",
  //  - `match.description` = "",
  //  - `match.destination_url` = `template_url->url()`,
  //  - `match.fill_to_edit` = `template_url->url()`,
  //  - `match.image_url` = `icon_url` from EnterpriseSearchAggregatorSettings
  //  policy,
  //  - `match.relevance` = 1001.
  void ParseResultList(const base::Value::List* results,
                       const TemplateURL* template_url,
                       SuggestionType suggestion_type,
                       bool is_navigation);

  std::string GetUrl(const base::Value::Dict& result,
                     const TemplateURLRef& url_ref,
                     SuggestionType suggestion_type) const;

  // Helper method to get `description` based on `suggestion_type` for
  // `CreateMatch()`.
  std::string GetMatchDescription(const base::Value::Dict& result,
                                  SuggestionType suggestion_type) const;

  // Helper method to get `contents` based on `suggestion_type` for
  // `CreateMatch()`.
  std::string GetMatchContents(const base::Value::Dict& result,
                               SuggestionType suggestion_type) const;

  // Helper to create a match.
  AutocompleteMatch CreateMatch(const AutocompleteInput& input,
                                const std::u16string& keyword,
                                SuggestionType suggestion_type,
                                bool is_navigation,
                                int relevance,
                                const std::string& destination_url,
                                const std::u16string& description,
                                const std::u16string& contents);

  // Owned by AutocompleteController.
  const raw_ptr<AutocompleteProviderClient> client_;

  // Used to ensure that we don't send multiple requests in quick succession.
  std::unique_ptr<AutocompleteProviderDebouncer> debouncer_;

  // Saved when starting a new autocomplete request so that it can be retrieved
  // when responses return asynchronously.
  AutocompleteInput input_;

  // Loader used to retrieve results.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  raw_ptr<TemplateURLService> template_url_service_;

  base::WeakPtrFactory<EnterpriseSearchAggregatorProvider> weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
