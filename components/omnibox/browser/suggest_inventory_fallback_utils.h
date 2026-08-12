// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGEST_INVENTORY_FALLBACK_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGEST_INVENTORY_FALLBACK_UTILS_H_

#include <string>
#include <vector>

#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "third_party/omnibox_proto/suggest_inventory.pb.h"

class AutocompleteProvider;
class AutocompleteProviderClient;

namespace omnibox {

inline constexpr size_t kDefaultFallbackNumSuggestions = 5;
inline constexpr int kDefaultFallbackSuggestRelevance = 300;

// Returns `num_suggestions` randomized UTF-16 fallback prompt strings for
// the specified `inventory` and `{} if no fallback prompts exist. Suggest
// inventory uses the `azi` param and is supplied by the server for currently
// for NTP Action Chips.
std::vector<std::pair<std::u16string, std::u16string>>
GetFallbackPromptsForSuggestInventory(
    SuggestInventory inventory,
    size_t num_suggestions = kDefaultFallbackNumSuggestions);

// Generates fallback AutocompleteMatch entries of type SEARCH_SUGGEST for the
// given input's suggest_inventory. Populates relevance, classification, search
// terms args, and destination URL using the client's default search provider.
std::vector<AutocompleteMatch> MaybeCreateFallbackMatchesForSuggestInventory(
    AutocompleteProvider* provider,
    AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    size_t num_suggestions = kDefaultFallbackNumSuggestions);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGEST_INVENTORY_FALLBACK_UTILS_H_
