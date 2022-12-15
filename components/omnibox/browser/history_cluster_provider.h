// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_CLUSTER_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_CLUSTER_PROVIDER_H_

#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"

class AutocompleteProvider;
class AutocompleteProviderClient;

// `HistoryClusterProvider` adds suggestions to the history journey page if
// any `SearchProvider` suggestions matched a cluster keyword. Inherits
// `AutocompleteProviderListener` in order to listen to `SearchProvider`
// updates. Uses `SearchProvider` suggestions, as a proxy for what the user may
// be typing if they're typing a query. Doesn't use other search providers'
// (i.e. `VoiceSearchProvider` and `ZeroSuggestProvider`) suggestions for
// simplicity.
class HistoryClusterProvider : public AutocompleteProvider,
                               public AutocompleteProviderListener {
 public:
  HistoryClusterProvider(AutocompleteProviderClient* client,
                         AutocompleteProviderListener* listener,
                         AutocompleteProvider* search_provider,
                         AutocompleteProvider* history_url_provider,
                         AutocompleteProvider* history_quick_provider);

  // Updates `match->action` to have the `OmniboxAction`, and updates
  // `provider_suggestion_groups_map` to contain the right groups.
  // `matching_text` is necessary to pass the query down to the `OmniboxAction`
  // so it can pre-populate the query field in the Journeys searchbox.
  static void CompleteHistoryClustersMatch(
      const std::string& matching_text,
      history::ClusterKeywordData matched_keyword_data,
      AutocompleteMatch* match);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  // AutocompleteProviderListener:
  // `HistoryClusterProvider` listens to `SearchProvider` updates.
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

 private:
  ~HistoryClusterProvider() override = default;

  // Check if `search_provider_`, `history_url_provider_`, and
  // `history_quick_provider_` are all done.
  bool AllProvidersDone();

  // Iterates `search_provider_->matches()` and check if any can be used to
  // create a history cluster match. Returns whether any matches were created.
  bool CreateMatches();

  // Creates a `AutocompleteMatch`.
  AutocompleteMatch CreateMatch(
      const AutocompleteMatch& search_match,
      history::ClusterKeywordData matched_keyword_data);

  // The `AutocompleteInput` passed to `Start()`.
  AutocompleteInput input_;

  // These are never null. The providers are used to detect nav intent for which
  // no matches will be provided. Other providers can also provide search and
  // navigations suggestion, but these are the dominant sources, both in volume
  // and high scores (which is what nav intent considers).
  const raw_ptr<AutocompleteProviderClient> client_;
  const raw_ptr<AutocompleteProvider> search_provider_;
  const raw_ptr<AutocompleteProvider> history_url_provider_;
  const raw_ptr<AutocompleteProvider> history_quick_provider_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_CLUSTER_PROVIDER_H_
