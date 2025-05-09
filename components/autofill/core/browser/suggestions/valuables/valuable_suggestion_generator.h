// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_

#include <vector>

#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "url/gurl.h"

namespace autofill {

// Generates loyalty card suggestions for given `origin`. Loyalty cards are
// extracted from the `valuables_manager`.
//
// The suggestions are generated in the extracted order of cards. If any of
// loyalty card merchant domains match the given `url`, the respective
// suggestions are moved to the top.
std::vector<Suggestion> GetLoyaltyCardSuggestions(
    const ValuablesDataManager& valuables_manager,
    const GURL& url);

// Extends `email_suggestions` with loyalty cards suggestions placed in a
// submenu.
//
// Loyalty cards suggestions are retrieved and follow the same logic as
// `GetLoyaltyCardSuggestions()`. The passed `email_suggestions` must be at
// least 3 suggestions long (email, separator and 'manage' suggestion).
void ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
    std::vector<Suggestion>& email_suggestions,
    const ValuablesDataManager& valuables_manager,
    const GURL& url);

// Appends loyalty cards suggestions to given `autocomplete_suggestions`.
//
// Loyalty cards suggestions are retrieved and follow the same logic as
// `GetLoyaltyCardSuggestions()`. The passed `autocomplete_suggestions` must not
// be empty.
void ExtendAutocompleteSuggestionsWithLoyaltyCardSuggestions(
    std::vector<Suggestion>& autocomplete_suggestions,
    const ValuablesDataManager& valuables_manager,
    const GURL& url);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
