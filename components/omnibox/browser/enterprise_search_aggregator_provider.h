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

  EnterpriseSearchAggregatorProvider(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(AutocompleteStopReason stop_reason) override;

 private:
  friend class FakeEnterpriseSearchAggregatorProvider;

  // Tracks state for `Request` declared below.
  enum class RequestState {
    kNotStarted,
    kStarted,
    kCompleted,
  };

  // When parsing response JSONs, we need to track not only the parsed matches
  // but also how many results the response included. We can't simply use
  // `matches.size()`, because some response results might be filtered out.
  // `RequestParsed` is a helper to track those 2.
  struct RequestParsed {
    RequestParsed();
    RequestParsed(std::vector<AutocompleteMatch> matches, size_t result_count);
    RequestParsed(RequestParsed&&) noexcept;
    RequestParsed(const RequestParsed&) = delete;
    ~RequestParsed();
    RequestParsed& operator=(RequestParsed&&) noexcept;
    RequestParsed& operator=(const RequestParsed&) = delete;

    // When `multiple_requests` is false, the single request will be parsed in 3
    // calls to `ParseResultList()`. `Append()` simply merges the `matches` and
    // sums the `result_count`s.
    void Append(RequestParsed parsed);

    std::vector<AutocompleteMatch> matches;
    size_t result_count = 0;
  };

  // The provider makes multiple async requests in parallel. This helper handles
  // the callbacks, logging, and caching for each request.
  class Request {
   public:
    explicit Request(std::vector<SuggestionType> types);
    ~Request();
    Request(Request&&);
    Request(const Request&) = delete;

    // Whether this request should be made. E.g. query requests are not allowed
    // unscoped.
    bool Allowed(bool in_keyword_mode) const;
    // Clears most state. Conditionally doesn't clear `matches` in order to
    // support caching. `Reset(false)` should be called before `OnStart()` is
    // called. `Reset(true)` will immediately clear cached matches; it should
    // only be called if the request will not be started and completed; e.g.
    // unscoped query request. Will log the previous request if it's being
    // interrupted; i.e. `OnCompleted()` hadn't been called.
    void Reset(bool clear_cached_matches);
    // Called when the real request starts; after the auth request completes.
    // Should be called before `OnCompleted()` is called.
    void OnStart(std::unique_ptr<network::SimpleURLLoader> loader);
    // Called when the real request completes. Will replace cached matches and
    // log.
    void OnCompleted(RequestParsed parsed);

    // Logs how long it has been since a request started at `start_time`. Sliced
    // by request type and completion.
    static void LogResponseTime(const std::string& type_histogram_suffix,
                                bool interrupted,
                                base::TimeTicks start_time);
    // Logs how many results a response contained. Sliced by request type. Not
    // logged for interrupted requests.
    static void LogResultCount(const std::string& type_histogram_suffix,
                               int count);

    const std::vector<SuggestionType> Types() const;
    // Map `types_` to the integers the backend understands. Some types like
    // `CONTENT` map to multiple backend types. And `types_` itself is a vector
    // Hence this returns a vector.
    std::vector<int> BackendSuggestionTypes() const;
    RequestState State() const;
    base::TimeTicks StartTime() const;
    const std::vector<AutocompleteMatch>& Matches() const;
    int ResultCount() const;

   private:
    // Log all of this request's metrics on completion or interruption; i.e.
    // response time and result count.
    void Log(bool interrupted) const;
    // Map `types_` to a histogram suffix for slicing.
    std::string TypeHistogramSuffix() const;

    // The type of suggestions to request.
    // TODO(manukh): After launching multiple_requests, this can be a single
    //   value instead of a vector.
    const std::vector<SuggestionType> types_;
    // State of request. `start_time_` and `loader_` can't differentiate between
    // `kNotStarted` and `kCompleted`, so this explicit `state_` is necessary.
    RequestState state_ = RequestState::kCompleted;
    // Start time of ongoing request. Null before requests start and after they
    // complete.
    base::TimeTicks start_time_;
    // Not null for ongoing requests. Null before requests start and after they
    // complete.
    std::unique_ptr<network::SimpleURLLoader> loader_;
    RequestParsed parsed_;
  };

  ~EnterpriseSearchAggregatorProvider() override;

  // Determines whether the profile/session/window meet the feature
  // prerequisites.
  bool IsProviderAllowed(const AutocompleteInput& input);

  // Called by `debouncer_`, queued when `Start()` is called.
  void Run();

  // Callback for when the loader is available with a valid token. Takes
  // ownership of the loader.
  void RequestStarted(int request_index,
                      std::unique_ptr<network::SimpleURLLoader> loader);

  // Called when the network request for suggestions has completed.
  // `request_index` corresponds to the type of request sent:
  void RequestCompleted(int request_index,
                        const network::SimpleURLLoader* source,
                        int response_code,
                        std::optional<std::string> response_body);

  // Callback for parsing the response JSON string into `base::Value` in an
  // isolated process.
  void OnJsonParsedIsolated(int request_index,
                            base::expected<base::Value, std::string> result);

  // Called after parsing the response JSON string into `base::Value`, either in
  // the main or an isolated process. Will use the `Parse*()` methods below to
  // further parse the `base::Value` into `RequestParsed` and update
  // `requests[request_index]` with the `RequestParsed.
  void HandleParsedJson(int request_index,
                        const std::optional<base::Value::Dict>& response_value);

  // Parses the response `base::Value` into `RequestParsed`.
  RequestParsed ParseEnterpriseSearchAggregatorSearchResults(
      const std::vector<SuggestionType>& suggestion_types,
      const base::Value::Dict& root_val);

  // Helper method to parse query, people, and content suggestions.
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
  RequestParsed ParseResultList(std::set<std::u16string> input_words,
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
  // a sign the user wanted this suggestion. `GetMatchDescription()` and
  // `GetMatchContents()`, and `GetEmailUsernames()` should be passed to
  // `GetStrongScoringFields()`.
  std::vector<std::string> GetStrongScoringFields(
      const base::Value::Dict& result,
      SuggestionType suggestion_type,
      const std::string& contents,
      const std::string& description,
      const std::vector<std::u16string> email_usernames) const;
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

  // Aggregates the `RequestParsed`s of `requests_` into `matches_` and notifies
  // the provider listener. Called multiple times in each autocomplete pass;
  // once immediately when `Run()` is called to display the cached matches; then
  // again as each request completes.
  void AggregateMatches();

  // Called when all `requests_` complete or are interrupted.
  void LogAllRequests(bool interrupted);

  // Owned by AutocompleteController.
  const raw_ptr<AutocompleteProviderClient> client_;

  // Used to ensure that we don't send multiple requests in quick succession.
  std::unique_ptr<AutocompleteProviderDebouncer> debouncer_;

  // Saved when starting a new autocomplete request so that they can be
  // retrieved when responses return asynchronously.
  AutocompleteInput adjusted_input_;
  raw_ptr<const TemplateURL> template_url_;

  raw_ptr<TemplateURLService> template_url_service_;

  // See comment for `Request`. `requests_` are initialized in the provider
  // constructor and reused throughout the provider's lifetime. This is
  // necessary to cache results beyond a single autocomplete pass.
  std::vector<Request> requests_;

  base::WeakPtrFactory<EnterpriseSearchAggregatorProvider> weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
