// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_util.h"

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {

bool SuppressSuggestionsForAutocompleteUnrecognizedField(
    const AutofillField& field,
    bool suppress_if_ac_unrecognized) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  return field.ShouldSuppressSuggestionsAndFillingByDefault(
      suppress_if_ac_unrecognized);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

std::vector<Suggestion> PrepareLoadingStateSuggestions(
    std::vector<Suggestion> current_suggestions,
    const Suggestion& selected_suggestion) {
  for (Suggestion& suggestion : current_suggestions) {
    using enum Suggestion::Acceptability;
    if (suggestion == selected_suggestion) {
      suggestion.acceptability = kUnacceptable;
      suggestion.is_loading = Suggestion::IsLoading(true);
    } else {
      suggestion.acceptability = kUnacceptableWithDeactivatedStyle;
    }
  }
  return current_suggestions;
}

}  // namespace autofill
