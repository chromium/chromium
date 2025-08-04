// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_

#include <vector>

#include "components/autofill/core/browser/suggestions/suggestion.h"

class GURL;

namespace autofill {

class ValuablesDataManager;

// Generates loyalty card suggestions for a given `url`. Loyalty cards are
// extracted from the `valuables_manager`.
std::vector<Suggestion> GetSuggestionsForLoyaltyCards(
    const ValuablesDataManager& valuables_manager,
    const GURL& url,
    bool trigger_field_is_autofilled);

// Extends `email_suggestions` with loyalty cards suggestions.
void ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
    const ValuablesDataManager& valuables_manager,
    const GURL& url,
    bool trigger_field_is_autofilled,
    std::vector<Suggestion>& email_suggestions);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
