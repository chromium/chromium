// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_util.h"

#include <algorithm>
#include <vector>

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Returns true if `suggestion` is equal to `target` or if `target` is a
// descendant of `suggestion` at any depth in `suggestion.children`.
bool IsOrHasDescendant(const Suggestion& suggestion, const Suggestion& target) {
  return suggestion == target ||
         std::ranges::any_of(suggestion.children,
                             [&target](const Suggestion& child) {
                               return IsOrHasDescendant(child, target);
                             });
}

// Recursively deactivates `suggestion` and all of its descendants by setting
// their acceptability to `kUnselectableAndUnacceptable` and ensuring their
// loading state is false.
void DisableSuggestionAndDescendants(Suggestion& suggestion) {
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  suggestion.is_loading = Suggestion::IsLoading(false);
  for (Suggestion& child : suggestion.children) {
    DisableSuggestionAndDescendants(child);
  }
}

}  // namespace

AutocompleteUnrecognizedBehavior GetAcUnrecognizedBehavior(
    const AutofillClient& autofill_client) {
  return autofill_client.IsTabInActorMode()
             ? AutocompleteUnrecognizedBehavior::kSuggestionsAllowed
             : AutocompleteUnrecognizedBehavior::kSuggestionsSuppressed;
}

bool SuppressSuggestionsForAutocompleteUnrecognizedField(
    const AutofillField& field,
    AutocompleteUnrecognizedBehavior behavior) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  return field.ShouldSuppressSuggestionsAndFillingByDefault(behavior);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

std::vector<Suggestion> PrepareLoadingStateSuggestions(
    std::vector<Suggestion> current_suggestions,
    const Suggestion& selected_suggestion) {
  for (Suggestion& suggestion : current_suggestions) {
    using enum Suggestion::Acceptability;
    if (IsOrHasDescendant(suggestion, selected_suggestion)) {
      suggestion.acceptability = kSelectableButUnacceptable;
      suggestion.is_loading = Suggestion::IsLoading(true);
    } else {
      suggestion.acceptability = kUnselectableAndUnacceptable;
      suggestion.is_loading = Suggestion::IsLoading(false);
    }
    for (Suggestion& child : suggestion.children) {
      DisableSuggestionAndDescendants(child);
    }
  }
  return current_suggestions;
}

Suggestion CreateUndoSuggestion() {
  Suggestion suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM),
                        SuggestionType::kUndo);
  suggestion.icon = Suggestion::Icon::kUndo;
  // TODO(crbug.com/40266549): update "Clear Form" a11y announcement to "Undo"
  suggestion.acceptance_a11y_announcement =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  return suggestion;
}

}  // namespace autofill
