// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_

#include <vector>

#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

class AutofillField;
struct Suggestion;

// `AutocompleteUnrecognizedBehavior` describes the general behavior (as per
// `AutofillClient`) whether fields with unrecognized autocomplete value can
// have suppressed suggestions in general. The concrete behavior is influenced
// by the concrete `AutofillField` and the operating system.
// See `SuppressSuggestionsForAutocompleteUnrecognizedField` below for
// determining the behavior for a specific `AutofillField`.
enum class AutocompleteUnrecognizedBehavior {
  // Suggestions are suppressed for autocomplete=unrecognized fields.
  kSuggestionsSuppressed = 0,
  // Suggestions are allowed for autocomplete=unrecognized fields as long as
  // `kAutofillEnableSkippingUnrecognizedAttribute` is enabled.
  kSuggestionsAllowed = 1,
  kMaxValue = kSuggestionsAllowed,
};

// Based on the current state of `autofill_client`, determines if fields with
// an unrecognized autocomplete attribute, should have suggestions suppressed.
AutocompleteUnrecognizedBehavior GetAcUnrecognizedBehavior(
    const AutofillClient& autofill_client);

// Returns true if suggestions should be suppressed on `field` because of it
// having an unrecognized HTML autocomplete attribute.
bool SuppressSuggestionsForAutocompleteUnrecognizedField(
    const AutofillField& field,
    AutocompleteUnrecognizedBehavior behavior);

// Updates and returns `current_suggestions` such that all suggestions apart
// from `selected_suggestion` are deactivated. `selected_suggestion` is marked
// as "loading".
std::vector<Suggestion> PrepareLoadingStateSuggestions(
    std::vector<Suggestion> current_suggestions,
    const Suggestion& selected_suggestion);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_
