// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_

#include <cstddef>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

namespace autofill {
class AutofillField;
enum class FillingProduct;

namespace autofill_metrics {

// Log the number of Autofill suggestions for the given
// `filling_product`presented to the user when displaying the autofill popup.
void LogSuggestionsCount(size_t num_suggestions,
                         FillingProduct filling_product);

// Log the index of the selected Autofill suggestion in the popup.
void LogSuggestionAcceptedIndex(int index,
                                FillingProduct filling_product,
                                bool off_the_record);

// Logs metrics related to an autofill on typing suggestion being accepted.
void LogAddressAutofillOnTypingSuggestionAccepted(
    FieldType field_type_used,
    const AutofillField* autofill_trigger_field);

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
