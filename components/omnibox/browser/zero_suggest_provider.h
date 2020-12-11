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

#include "base/compiler_specific.h"
#include "components/history/core/browser/history_types.h"
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

  // Creates and returns an instance of this provider.
  static ZeroSuggestProvider* Create(AutocompleteProviderClient* client,
                                     AutocompleteProviderListener* listener);

  // Registers a preference used to cache zero suggest results.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results,
            bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

  // Sets |field_trial_triggered_| to false.
  void ResetSession() override;

  // Returns the list of experiment stats corresponding to the latest |results_|
  // to be logged to SearchboxStats as part of a GWS experiment, if any.
  const SearchSuggestionParser::ExperimentStats& experiment_stats() const {
    return results_.experiment_stats;
  }

  // Returns the map of suggestion group IDs to headers corresponding to the
  // latest |results_|.
  const SearchSuggestionParser::HeadersMap& headers_map() const {
    return results_.headers_map;
  }

  // Returns the hidden group IDs corresponding to the latest |results_|.
  const std::vector<int> hidden_group_ids() const {
    return results_.hidden_group_ids;
  }

  ResultType GetResultTypeRunningForTesting() const {
    return result_type_running_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ZeroSuggestProviderTest,
                           AllowZeroSuggestSuggestions);
  FRIEND_TEST_ALL_PREFIXES(ZeroSuggestProviderTest, TypeOfResultToRun);
  FRIEND_TEST_ALL_PREFIXES(ZeroSuggestProviderTest,
                           TypeOfResultToRunForContextualWeb);
  FRIEND_TEST_ALL_PREFIXES(ZeroSuggestProviderTest,
                           TestStartWillStopForSomeInput);
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

  // Called when the network request for suggestions has completed.
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body);

  // The function updates |results_| with data parsed from |json_data|.
  //
  // * The update is not performed if |json_data| is invalid.
  // * When the provider is using cached results and |json_data| is non-empty,
  //   this function updates the cached results.
  // * When |results_| contains cached results, these are updated only if
  //   |json_cata| corresponds to an empty list. This is done to ensure that
  //   the display is cleared, as it may be showing cached results that should
  //   not be shown.
  //
  // The return value is true only when |results_| changed.
  bool UpdateResults(const std::string& json_data);

  // Returns an AutocompleteMatch for a navigational suggestion |navigation|.
  AutocompleteMatch NavigationToMatch(
      const SearchSuggestionParser::NavigationResult& navigation);

  // Converts the parsed results to a set of AutocompleteMatches and adds them
  // to |matches_|.  Also update the histograms for how many results were
  // received.
  void ConvertResultsToAutocompleteMatches();

  // Returns an AutocompleteMatch for the current text. The match should be in
  // the top position so that pressing enter has the effect of reloading the
  // page.
  AutocompleteMatch MatchForCurrentText();

  // When the user is in the remote omnibox suggestions field trial, we ask
  // the RemoteSuggestionsService for a loader to retrieve recommendations.
  // When the loader has started, the remote suggestion service then calls
  // back to this function with the |loader| to pass its ownership to |this|.
  void OnRemoteSuggestionsLoaderAvailable(
      std::unique_ptr<network::SimpleURLLoader> loader);

  // Whether zero suggest suggestions are allowed in the given context.
  // Invoked early, confirms all the external conditions for ZeroSuggest are
  // met.
  bool AllowZeroSuggestSuggestions(const AutocompleteInput& input) const;

  // Checks whether we have a set of zero suggest results cached, and if so
  // populates |matches_| with cached results.
  void MaybeUseCachedSuggestions();

  // Returns the type of results that should be generated for the current
  // context.
  // Logs UMA metrics. Should be called exactly once, on Start(), otherwise the
  // meaning of the data logged would change.
  //
  // This method is static for testability and to avoid depending on the
  // provider state.
  static ResultType TypeOfResultToRun(AutocompleteProviderClient* client,
                                      const AutocompleteInput& input,
                                      const GURL& suggest_url);

  AutocompleteProviderListener* listener_;

  // The result type that is currently being processed by provider.
  // When the provider is not running, the result type is set to NONE.
  ResultType result_type_running_;

  // The URL for which a suggestion fetch is pending.
  std::string current_query_;

  // The title of the page for which a suggestion fetch is pending.
  base::string16 current_title_;

  // The type of page the user is viewing (a search results page doing search
  // term replacement, an arbitrary URL, etc.).
  metrics::OmniboxEventProto::PageClassification current_page_classification_ =
      metrics::OmniboxEventProto::INVALID_SPEC;

  // Copy of OmniboxEditModel::permanent_text_.
  base::string16 permanent_text_;

  // Loader used to retrieve results.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // The verbatim match for the current text, which is always a URL.
  AutocompleteMatch current_text_match_;

  // Contains suggest and navigation results as well as relevance parsed from
  // the response for the most recent zero suggest input URL.
  SearchSuggestionParser::Results results_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<ZeroSuggestProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PROVIDER_H_
