// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_

#include <vector>

#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "url/gurl.h"

namespace autofill {

// Generates suggestions for given `origin` and all given `loyalty_cards`.
//
// The suggestions are generated in order of given `loyalty_cards`.
// If any of loyalty card merchant domains matches given `origin`
// respective suggestion is moved to the top.
std::vector<Suggestion> GetLoyaltyCardSuggestions(
    base::span<const LoyaltyCard> loyalty_cards,
    const GURL& url);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
