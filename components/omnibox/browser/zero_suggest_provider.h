// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the zero-suggest autocomplete provider. This experimental
// provider is invoked when the user focuses in the omnibox prior to editing,
// and generates search query suggestions based on the current URL.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PROVIDER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/search_provider.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteProviderListener;
class PrefRegistrySimple;

namespace base {
class Value;
}

namespace network {
class SimpleURLLoader;
}


// Autocomplete provider for searches based on the current URL.
//
// The controller will call Start() when the user focuses the omnibox. After
// construction, the autocomplete controller repeatedly calls Start() with some
// user input, each time expecting to receive an updated set of matches.
//
// TODO(jered): Consider deleting this class and building this functionality
// into SearchProvider after dogfood and after we break the association between
// omnibox text and suggestions.
class ZeroSuggestProvider : public BaseSearchProvider {
 public:
  // ZeroSuggestProvider is processing one of the following type of results
  // at any time. Exposed as public for testing purposes.
  enum ResultType {
    NONE,

    // A remote endpoint (usually the default search provider) is queried for
    // suggestions. The endpoint is sent the user's authentication state, but
    // not sent the current URL.
    REMOTE_NO_URL,

    // A remote endpoint (usually the default search provider) is queried for
    // suggestions. The endpoint is sent the user's authentication state and
    // the current URL.
    REMOTE_SEND_URL,
  };

  // Returns the type of results that should be generated for the given context.
  // If `bypass_request_eligibility_checks` is false, checks whether the
  // external conditions for REMOTE_NO_URL and REMOTE_SEND_URL variants are met;
  // Logs eligibility UMA metrics, if applicable. Must be called exactly once
  // with `bypass_request_eligibility_checks` set to false,
  // otherwise the meaning of the metrics being logged would change.
  // This method is static to avoid depending on the provider state.
  static ResultType TypeOfResultToRun(const AutocompleteProviderClient* client,
                                      const AutocompleteInput& input,
                                      bool bypass_request_eligibility_checks);

  // Called on Start(), confirms whether zero-prefix suggestions are allowed in
  // the given context and logs eligibility UMA metrics. `result_type_to_run`
  // must not be nullptr. It will be set to the result type that should be
  // generated for the given context.
  // Must be called exactly once, on Start(), otherwise the meaning of the
  // the metrics being logged would change.
  // This method is static to avoid depending on the provider state.
  static bool AllowZeroPrefixSuggestions(
      const AutocompleteProviderClient* client,
      const AutocompleteInput& input,
      ResultType* result_type_to_run);

  // Creates and returns an instance of this provider.
  static ZeroSuggestProvider* Create(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener);

  // Registers a preference used to cache the zero suggest response.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // AutocompleteProvider:
  void StartPrefetch(const AutocompleteInput& input) override;
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results,
            bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

  // Sets |field_trial_triggered_| to false.
  void ResetSession() override;

  // Returns the list of experiment stats corresponding to |matches_|. Will be
  // logged to SearchboxStats as part of a GWS experiment, if any.
  const SearchSuggestionParser::ExperimentStatsV2s& experiment_stats_v2s()
      const {
    return experiment_stats_v2s_;
  }

  ResultType GetResultTypeRunningForTesting() const {
    return result_type_running_;
  }

 private:
  ZeroSuggestProvider(AutocompleteProviderClient* client,
                      AutocompleteProviderListener* listener);

  ~ZeroSuggestProvider() override;

  ZeroSuggestProvider(const ZeroSuggestProvider&) = delete;
  ZeroSuggestProvider& operator=(const ZeroSuggestProvider&) = delete;

  // BaseSearchProvider:
  const TemplateURL* GetTemplateURL(bool is_keyword) const override;
  const AutocompleteInput GetInput(bool is_keyword) const override;
  bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const override;
  void RecordDeletionResult(bool success) override;

  // Called when the non-prefetch network request has completed.
  // `result_type` is bound to this callback and indicate the result type being
  // received in this callback.
  void OnURLLoadComplete(ResultType result_type,
                         const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);
  // Called when the prefetch network request has completed.
  // `input` and `result_type` are bound to this callback. The former is the
  // input the request was made for and the latter indicates the result type
  // being received in this callback.
  void OnPrefetchURLLoadComplete(const AutocompleteInput& input,
                                 ResultType result_type,
                                 const network::SimpleURLLoader* source,
                                 std::unique_ptr<std::string> response_body);

  // Called when the remote response is received. Stores the response json in
  // the user prefs, if successfully parsed and if applicable based on
  // |result_type|.
  //
  // Returns the successfully parsed response if it is eligible to be converted
  // to |matches_| or nullptr otherwise.
  std::unique_ptr<base::Value> StoreRemoteResponse(
      const std::string& response_json,
      const AutocompleteInput& input,
      ResultType result_type,
      bool is_prefetch);

  // Called on Start().
  //
  // Returns the response stored in the user prefs, if applicable based on
  // |result_type| or nullptr otherwise.
  std::unique_ptr<base::Value> ReadStoredResponse(ResultType result_type);

  // Returns an AutocompleteMatch for a navigational suggestion |navigation|.
  AutocompleteMatch NavigationToMatch(
      const SearchSuggestionParser::NavigationResult& navigation);

  // Called on Start() with the cached response (where |matches_| is empty), or
  // when the remote response is received and is eligible to be converted to
  // |matches_| (where |matches_| may not be empty).
  //
  // If the given response can be successfully parsed, converts it to a set of
  // AutocompleteMatches and populates |matches_| as well as its associated
  // metadata, if applicable. Also logs how many results were received.
  //
  // Returns whether the response was successfully converted to |matches_|.
  // Note that this does not imply |matches_| were populated with the response.
  // An empty result set in the response will clear |matches_| and return true.
  bool ConvertResponseToAutocompleteMatches(
      std::unique_ptr<base::Value> response);

  // The result type that is currently being retrieved and processed for
  // non-prefetch requests.
  // Set in Start() and used in Stop() for logging purposes.
  ResultType result_type_running_{NONE};

  // The input for which suggestions are being retrieved and processed for both
  // prefetch and non-prefetch requests.
  // Set in Start() and StartPrefetch() and used in GetInput() for parsing the
  // response.
  AutocompleteInput input_;

  // Loader used to retrieve results for non-prefetch requests.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Loader used to retrieve results for prefetch requests.
  std::unique_ptr<network::SimpleURLLoader> prefetch_loader_;

  // The list of experiment stats corresponding to |matches_|.
  SearchSuggestionParser::ExperimentStatsV2s experiment_stats_v2s_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<ZeroSuggestProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PROVIDER_H_
