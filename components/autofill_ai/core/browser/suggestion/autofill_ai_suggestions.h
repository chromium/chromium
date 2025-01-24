// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_SUGGESTIONS_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_SUGGESTIONS_H_

#include <vector>

#include "base/containers/span.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"

namespace autofill {

class EntityInstance;
class FormData;
class FormFieldData;
class FormStructure;
struct Suggestion;

}  // namespace autofill

namespace autofill_ai {

class AutofillAiClient;

// Returns true if the type of `autofill_suggestion` should not be added to
// AutofillAi or if `autofill_suggestion` likely matches the cached
// AutofillAi.
// TODO(crbug.com/376016081): Move to anonymous namespace.
bool ShouldSkipAutofillSuggestion(
    AutofillAiClient& client,
    const AutofillAiModelExecutor::PredictionsByGlobalId& cache,
    const autofill::FormData& form,
    const autofill::Suggestion& autofill_suggestion);

// Creates the suggestion that invokes loading predictions when accepted.
std::vector<autofill::Suggestion> CreateTriggerSuggestions();

// Creates the animated suggestion shown while improved predictions are loaded.
std::vector<autofill::Suggestion> CreateLoadingSuggestions();

// Creates filling suggestions listing the ones for AutofillAi
// first and `autofill_suggestions` afterwards. This method should only be
// called when the feature `kAutofillAi` feature flag is enabled.
// TODO(crbug.com/391276120): Delete this method once clean up starts.
std::vector<autofill::Suggestion> CreateFillingSuggestions(
    AutofillAiClient& client,
    const AutofillAiModelExecutor::PredictionsByGlobalId& cache,
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    const std::vector<autofill::Suggestion>& autofill_suggestions);

// Creates filling suggestions using `autofill::EntityInstance`s.
// Should be called only when `AutofillAiWithStructuredData` feature flag is
// enabled.
std::vector<autofill::Suggestion> CreateFillingSuggestionsV2(
    const autofill::FormStructure& form,
    autofill::FieldGlobalId field_global_id,
    base::span<const autofill::EntityInstance> entities);

// Creates a suggestion shown when retrieving AutofillAi wasn't
// successful.
std::vector<autofill::Suggestion> CreateErrorSuggestions();

// Creates suggestions shown when there's nothing to fill (not even by Autofill
// or Autocomplete).
std::vector<autofill::Suggestion> CreateNoInfoSuggestions();

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_SUGGESTIONS_H_
