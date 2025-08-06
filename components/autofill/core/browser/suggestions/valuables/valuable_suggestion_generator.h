// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_

#include <vector>

#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

class GURL;

namespace autofill {

class ValuablesDataManager;

// Generates loyalty card suggestions for the value of trigger `field` and the
// last committed primary main frame URL obtained from `client`. Loyalty cards
// are extracted from the `ValuablesDataManager` using `client`.
// TODO(crbug.com/409962888): Remove after new suggestion generation logic is
// launched.
std::vector<Suggestion> GetSuggestionsForLoyaltyCards(
    const FormData& form,
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const AutofillClient& client);

// Extends `email_suggestions` with loyalty cards suggestions.
void ExtendEmailSuggestionsWithLoyaltyCardSuggestions(
    const ValuablesDataManager& valuables_manager,
    const GURL& url,
    bool trigger_field_is_autofilled,
    std::vector<Suggestion>& email_suggestions);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_VALUABLES_VALUABLE_SUGGESTION_GENERATOR_H_
