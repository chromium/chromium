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
std::vector<Suggestion> GetLoyaltyCardSuggestions(
    const ValuablesDataManager& valuables_manager,
    const GURL& url);

// Extends `email_suggestions` with loyalty cards suggestions.
void ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
    std::vector<Suggestion>& email_suggestions,
    const ValuablesDataManager& valuables_manager,
    const GURL& url);
}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
