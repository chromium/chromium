// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_

#include <stddef.h>

#include "base/containers/span.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"

namespace autofill {
class AutofillField;
enum class FillingProduct;
struct Suggestion;

namespace autofill_metrics {

// Log the number of Autofill suggestions presented to the user when
// displaying the autofill popup, grouped by `FillingProduct` and excluding
// management footer options.
void LogSuggestionsCount(base::span<const Suggestion> suggestions);

// Log the number of email suggestions shown to the user when merging
// Autocomplete and Address suggestions.
// TODO(crbug.com/506033768): Remove metric when feature is launched.
void LogMergedEmailSuggestionCounts(size_t num_address_suggestions,
                                    size_t num_autocomplete_suggestions);

// LINT.IfChange(EmailSuggestionAcceptedStatus)

enum class EmailSuggestionAcceptedStatus {
  kAddressOnly = 0,
  kAutocompleteOnly = 1,
  kMixedAddressSelected = 2,
  kMixedAutocompleteSelected = 3,

  kMaxValue = kMixedAutocompleteSelected,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillEmailSuggestionAcceptedStatus)

// Log the accepted suggestion type for email suggestions to evaluate merged
// Autocomplete and Address suggestions.
// TODO(crbug.com/506033768): Remove metric (including enum
// `EmailSuggestionAcceptedStatus`) when feature is launched.
void LogMergedEmailAcceptedSuggestionType(
    SuggestionType accepted_suggestion_type,
    base::span<const SuggestionType> shown_suggestion_types);

// Log the index of the selected Autofill suggestion in the popup.
void LogSuggestionAcceptedIndex(
    int index,
    FillingProduct filling_product,
    bool off_the_record,
    base::span<const SuggestionType> shown_suggestion_types);

// Logs metrics related to an autofill on typing suggestion being accepted.
void LogAddressAutofillOnTypingSuggestionAccepted(
    FieldType field_type_used,
    const AutofillField* autofill_trigger_field);

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
