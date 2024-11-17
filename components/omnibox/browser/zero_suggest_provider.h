// Copyright 2012 The Chromium Authors
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
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "components/omnibox/browser/base_search_provider.h"

class AutocompleteProviderListener;
class PrefRegistrySimple;

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
  // The result type that can be processed by ZeroSuggestProvider.
  // Public for testing purposes and for use in LocalHistoryZeroSuggestProvider.
  enum class ResultType {
    kNone = 0,

    // The remote endpoint is queried for zero-prefix suggestions. The endpoint
    // is sent the user's authentication state, but not the current page URL.
    kRemoteNoURL = 1,

    // The emote endpoint is queried for zero-prefix suggestions. The endpoint
    // is sent both the user's authentication state and the current page URL.
    kRemoteSendURL = 2,
  };

  // Creates and returns an instance of this provider.
  static ZeroSuggestProvider* Create(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener);

  // Registers a preference used to cache the zero suggest response.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the type of results that should be generated for the given context
  // and their eligibility.
  // This method is static to avoid depending on the provider state.
  static std::pair<ResultType, bool> GetResultTypeAndEligibility(
      const AutocompleteProviderClient* client,
      const AutocompleteInput& input);

  // AutocompleteProvider:
  void StartPrefetch(const AutocompleteInput& input) override;
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results,
            bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

  // Returns the list of experiment stats corresponding to |matches_|. Will be
  // logged to SearchboxStats as part of a GWS experiment, if any.
  const SearchSuggestionParser::ExperimentStatsV2s& experiment_stats_v2s()
      const {
    return experiment_stats_v2s_;
  }

  ResultType GetResultTypeRunningForTesting() const {
    return result_type_running_;
  }

  ZeroSuggestProvider(AutocompleteProviderClient* client,
                      AutocompleteProviderListener* listener);
  ZeroSuggestProvider(const ZeroSuggestProvider&) = delete;
  ZeroSuggestProvider& operator=(const ZeroSuggestProvider&) = delete;

 protected:
  ~ZeroSuggestProvider() override;

 private:
  // BaseSearchProvider:
  bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const override;
  void RecordDeletionResult(bool success) override;

  // Called when the non-prefetch network request has completed.
  // `input` and `result_type` are bound to this callback. The former is the
  // input for which the request was made and the latter indicates the result
  // type being received in this callback.
  void OnURLLoadComplete(const AutocompleteInput& input,
                         const ResultType result_type,
                         const network::SimpleURLLoader* source,
                         const int response_code,
                         std::unique_ptr<std::string> response_body);
  // Called when the prefetch network request has completed.
  // `input` and `result_type` are bound to this callback. The former is the
  // input for which the request was made and the latter indicates the result
  // type being received in this callback.
  void OnPrefetchURLLoadComplete(const AutocompleteInput& input,
                                 const ResultType result_type,
                                 const network::SimpleURLLoader* source,
                                 const int response_code,
                                 std::unique_ptr<std::string> response_body);

  // Called by `debouncer_`.
  void RunZeroSuggestPrefetch(const AutocompleteInput& input,
                              const ResultType result_type);

  // Returns an AutocompleteMatch for a navigational suggestion |navigation|.
  AutocompleteMatch NavigationToMatch(
      const SearchSuggestionParser::NavigationResult& navigation);

  // Called either in Start() with |results| populated from the cached response,
  // where |matches_| are empty; or in OnURLLoadComplete() with |results|
  // populated from the remote response, where |matches_| may not be empty.
  //
  // Uses |results| and |input| to populate |matches_| and its associated
  // metadata. Also logs how many results were received. Note that an empty
  // result set will clear |matches_|.
  void ConvertSuggestResultsToAutocompleteMatches(
      const SearchSuggestionParser::Results& results,
      const AutocompleteInput& input);

  // The result type that is currently being retrieved and processed for
  // non-prefetch requests.
  // Set in Start() and used in Stop() for logging purposes.
  ResultType result_type_running_{ResultType::kNone};

  // Loader used to retrieve results for non-prefetch requests.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // Loader used to retrieve results for ZPS prefetch requests on NTP.
  std::unique_ptr<network::SimpleURLLoader> ntp_prefetch_loader_;

  // Loader used to retrieve results for ZPS prefetch requests on SRP/Web.
  std::unique_ptr<network::SimpleURLLoader> srp_web_prefetch_loader_;

  // Debouncer used to throttle the frequency of ZPS prefetch requests (to
  // minimize the performance impact on the remote Suggest service).
  std::unique_ptr<AutocompleteProviderDebouncer> debouncer_;

  // The list of experiment stats corresponding to |matches_|.
  SearchSuggestionParser::ExperimentStatsV2s experiment_stats_v2s_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<ZeroSuggestProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PROVIDER_H_
