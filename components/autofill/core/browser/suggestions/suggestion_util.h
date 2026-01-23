// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_

#include <vector>

namespace autofill {

class AutofillField;
struct Suggestion;

// Returns true if suggestions should be suppressed on `field` because of it
// having an unrecognized HTML autocomplete attribute.
bool SuppressSuggestionsForAutocompleteUnrecognizedField(
    const AutofillField& field,
    bool suppress_if_ac_unrecognized);

// Updates and returns `current_suggestions` such that all suggestions apart
// from `selected_suggestion` are deactivated. `selected_suggestion` is marked
// as "loading".
std::vector<Suggestion> PrepareLoadingStateSuggestions(
    std::vector<Suggestion> current_suggestions,
    const Suggestion& selected_suggestion);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_UTIL_H_
