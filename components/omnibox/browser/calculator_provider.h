// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CALCULATOR_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_CALCULATOR_PROVIDER_H_

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/provider_state_service.h"

class SearchProvider;
class AutocompleteInput;
class AutocompleteProviderClient;

// Caches recent calculator matches from the search provider and displays them
// if the user is likely still in 'calculator mode'. Thus, users can reference
// their earlier calculations without resorting to copy/paste or multiple tabs.
// Details:
// - Cache size is `max_matches` (finch param, default 5).
// - Enters calc mode when the search provider has a calc suggestion.
// - Leaves calc mode when the search provider hasn't had a calc suggestion for
//   the last `num_non_calc_inputs` (finch param, default 3) inputs. This helps
//   with stability, because while '1+2' and '1+2+3' will have search calc
//   suggestions, '1+2+' won't.
// - Typing '1+2+3+4' will show a suggestion for the last input only:
//   '1+2+3+4=10'; and not for each intermediate calc suggestion: '1+2=3',
//   '1+2+3=6', & '1+2+3+4=10'.
// - Editing an input will not replace the original calc suggestion. E.g.
//   '1+2<backspace>3' will show both '1+2=3' and '1+3=4'.
// - Cached calc suggestions are scored `score`+i (finch param, default 900).
//   This helps ensure they're sequential. The current calc suggestions can be
//   deduped with a higher scoring search calc suggestion.
class CalculatorProvider : public AutocompleteProvider,
                           public AutocompleteProviderListener {
 public:
  CalculatorProvider(AutocompleteProviderClient* client,
                     AutocompleteProviderListener* listener,
                     SearchProvider* search_provider);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;

  // AutocompleteProviderListener:
  // `CalculatorProvider` listens to `SearchProvider` updates.
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

 private:
  ~CalculatorProvider() override;

  // Searches the search provider's matches for a calc match.
  void UpdateFromSearch();

  // Adds `match` to `cache_` and updates other state accordingly.
  void AddMatchToCache(AutocompleteMatch match);

  // Adds `cache_` to `matches_`.
  void AddMatches();

  // Returns true if this input should show calc matches.
  bool Show();

  // The recent search calc suggestions.
  std::vector<ProviderStateService::CachedAutocompleteMatch>& Cache();

  // The current input.
  std::u16string input_;
  // The last input that had a search calc suggestion.
  std::u16string last_calc_input_;
  // How old `last_calc_input_` is.
  size_t since_last_calculator_suggestion_ = SIZE_MAX;
  // Whether the current input is prefixed by the previous input. E.g. '1+' ->
  // '1+1'.
  bool grew_input_ = false;
  // Whether the previous input is prefixed by the current input. E.g. '1+1' ->
  // '1+'.
  bool shrunk_input_ = false;

  raw_ptr<AutocompleteProviderClient> client_;

  // Never null.
  const raw_ptr<SearchProvider> search_provider_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CALCULATOR_PROVIDER_H_
