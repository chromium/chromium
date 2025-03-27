// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CONTEXTUAL_SEARCH_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_CONTEXTUAL_SEARCH_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/base_search_provider.h"

class AutocompleteProviderClient;
class AutocompleteProviderListener;

namespace network {
class SimpleURLLoader;
}

// Autocomplete provider for searches based on page context, which includes
// page content, URL, possibly a screenshot, etc. Although some contextual
// suggestions may be shown without additional query input, this is functionally
// distinct from the ZeroSuggestProvider. It does its main work when explicitly
// invoked via the '@page' keyword mode, and also surfaces action matches for
// empty/zero inputs to help the user find their way into the '@page' scope.
class ContextualSearchProvider : public BaseSearchProvider {
 public:
  ContextualSearchProvider(AutocompleteProviderClient* client,
                           AutocompleteProviderListener* listener);
  ContextualSearchProvider(const ContextualSearchProvider&) = delete;
  ContextualSearchProvider& operator=(const ContextualSearchProvider&) = delete;

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

 protected:
  ~ContextualSearchProvider() override;

  // BaseSearchProvider:
  bool ShouldAppendExtraParams(
      const SearchSuggestionParser::SuggestResult& result) const override;
  void RecordDeletionResult(bool success) override {}

  // Sends request to remote suggest server. Invoked after all inputs
  // are ready, including page context.
  void StartSuggestRequest(AutocompleteInput input);

  // Called when the suggest network request has completed.
  void SuggestRequestCompleted(AutocompleteInput input,
                               const network::SimpleURLLoader* source,
                               const int response_code,
                               std::unique_ptr<std::string> response_body);

  // Uses |results| and |input| to populate |matches_| and its associated
  // metadata.
  void ConvertSuggestResultsToAutocompleteMatches(
      const SearchSuggestionParser::Results& results,
      const AutocompleteInput& input);

  // Populates `matches_` with special matches that help the user find their
  // way into the '@page' scope.
  void AddPageSearchActionMatches();

  // Adds a default match for verbatim input, or keyword instructions if there
  // is no input yet. This is the match that holds the omnibox in keyword mode
  // when no other matches are available yet.
  void AddDefaultMatch(std::u16string_view input_text);

  // Keyword taken from most recently started autocomplete input.
  std::u16string input_keyword_;

  // Loader used to retrieve suggest results.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<ContextualSearchProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CONTEXTUAL_SEARCH_PROVIDER_H_
