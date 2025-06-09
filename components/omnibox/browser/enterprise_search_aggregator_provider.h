// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

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
  using SuggestionType = AutocompleteMatch::EnterpriseSearchAggregatorType;

  // Relevance along with info for `AutocompleteMatch::additional_info`.
  struct RelevanceData {
    int relevance;
    size_t strong_word_matches;
    size_t weak_word_matches;
    std::string source;
  };

  // Holds the matches and loader for a single request.
  struct SearchAggregatorRequest {
    SearchAggregatorRequest();
    ~SearchAggregatorRequest();

    SearchAggregatorRequest(SearchAggregatorRequest&&);

    SearchAggregatorRequest(const SearchAggregatorRequest&) = delete;

    std::vector<AutocompleteMatch> matches;
    std::unique_ptr<network::SimpleURLLoader> loader;
    // Can't use `loader != nullptr` as a proxy for `done` because loader is
    // null both before the request starts and after the request completes.
    bool done = false;
    // Only used for logging. Can't use `matches.size()` as it may contain a
    // filtered down set of results from the response.
    int result_count = 0;
  };

  // The number of requests to make if we are making multiple requests.
  static const int kNumMultipleRequests = 3;

  EnterpriseSearchAggregatorProvider(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(AutocompleteStopReason stop_reason) override;

 private:
  friend class FakeEnterpriseSearchAggregatorProvider;

  ~EnterpriseSearchAggregatorProvider() override;

  // Determines whether the profile/session/window meet the feature
  // prerequisites.
  bool IsProviderAllowed(const AutocompleteInput& input);

  // Called by `debouncer_`, queued when `Start()` is called.
  void Run(const AutocompleteInput& input);

  // Callback for when the loader is available with a valid token. Takes
  // ownership of the loader.
  void RequestStarted(int request_index,
                      std::unique_ptr<network::SimpleURLLoader> loader);

  // Called when the network request for suggestions has completed.
  // `request_index` corresponds to the type of request sent:
  // - 0 for people suggesions
  // - 1 for content suggestions
  // - 2 for query suggestions.
  void RequestCompleted(int request_index,
                        const network::SimpleURLLoader* source,
                        int response_code,
                        std::unique_ptr<std::string> response_body);

  // The function updates `matches_` with data parsed from `response_value`.
  // The update is not performed if `response_value` is invalid.
  virtual void UpdateResults(
      int request_index,
      const std::optional<base::Value::Dict>& response_value,
      int response_code);

  // Callback for handling parsed json from response.
  void OnJsonParsedIsolated(int request_index,
                            base::expected<base::Value, std::string> result);

  // Parses enterprise search aggregator response JSON and updates `matches_`.
  void ParseEnterpriseSearchAggregatorSearchResults(
      int request_index,
      const base::Value::Dict& root_val);

  // Helper method to parse query, people, and content suggestions and populate
  // `matches_`.
  // - `input_words` is used for scoring matches.
  // - `suggestion_type` is used for selecting which JSON fields to look for,
  // scoring matches, and creating the match.
  // - `is_navigation` is used for creating the match.
  // Example:
  //   Given a `results` with one query suggestion:
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
  void ParseResultList(int request_index,
                       std::set<std::u16string> input_words,
                       const base::Value::List* results,
                       SuggestionType suggestion_type,
                       bool is_navigation);

  // Helper method to get `destination_url` based on `suggestion_type` for
  // `CreateMatch()`.
  std::string GetMatchDestinationUrl(const base::Value::Dict& result,
                                     SuggestionType suggestion_type) const;

  // Helper method to get `description` based on `suggestion_type` for
  // `CreateMatch()`.
  std::string GetMatchDescription(const base::Value::Dict& result,
                                  SuggestionType suggestion_type) const;

  // Helper method to get `contents` based on `suggestion_type` for
  // `CreateMatch()`.
  std::string GetMatchContents(const base::Value::Dict& result,
                               SuggestionType suggestion_type) const;

  // Helper method to get a localized metadata string depending on which of
  // `update_time`, `owner`, and `content_type_description` exist.
  std::u16string GetLocalizedContentMetadata(
      const std::u16string& update_time,
      const std::u16string& owner,
      const std::u16string& content_type_description) const;

  // Helper method to get user-readable (e.g. 'chromium is awesome
  // document') fields that can be used to compare input similarity.
  // Non-user-readable fields (e.g. 'doc_id=123/locations/global') should be
  // excluded because the input matching that would be a coincidence and not
  // a sign the user wanted this suggestion. Does not return fields already
  // returned by `GetMatchDescription()` and `GetMatchContents()`.
  std::vector<std::string> GetStrongScoringFields(
      const base::Value::Dict& result,
      SuggestionType suggestion_type) const;
  std::vector<std::string> GetWeakScoringFields(
      const base::Value::Dict& result,
      SuggestionType suggestion_type) const;

  // Helper to create a match.
  AutocompleteMatch CreateMatch(SuggestionType suggestion_type,
                                bool is_navigation,
                                RelevanceData relevance_data,
                                const std::string& destination_url,
                                const std::string& image_url,
                                const std::string& icon_url,
                                const std::u16string& description,
                                const std::u16string& contents,
                                const std::u16string& fill_into_edit);

  // Helper function for setting the time when the request started.
  void SetTimeRequestSent();

  // Helper function for logging request times sliced by whether the request was
  // interrupted or not.
  void LogResponseTime(bool interrupted);

  // Helper function for logging the number of results received from the
  // request.
  void LogResultCounts(std::string histogram_suffix, size_t result_count);

  // Owned by AutocompleteController.
  const raw_ptr<AutocompleteProviderClient> client_;

  // Used to ensure that we don't send multiple requests in quick succession.
  std::unique_ptr<AutocompleteProviderDebouncer> debouncer_;

  // Saved when starting a new autocomplete request so that they can be
  // retrieved when responses return asynchronously.
  AutocompleteInput adjusted_input_;
  raw_ptr<const TemplateURL> template_url_;

  raw_ptr<TemplateURLService> template_url_service_;

  // The most recent set of requests.
  std::vector<SearchAggregatorRequest> requests_;

  base::WeakPtrFactory<EnterpriseSearchAggregatorProvider> weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
