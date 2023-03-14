// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PRECEDENCE_OVER_AUTOCOMPLETE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PRECEDENCE_OVER_AUTOCOMPLETE_METRICS_H_

#include "components/autofill/core/browser/autofill_field.h"

namespace autofill::autofill_metrics {

// These values are persisted to UMA logs. Entries should not be renumbered
// and numeric values should never be reused. These are the categories of
// interests for looking at what are the autocomplete values that were used to
// annotate fields that are being predicted by the server or heuristics as
// street name or house number.
enum class AutocompleteValueForStructuredAddressPredictedFieldsMetric {
  kAddressLine1And2 = 0,
  kOtherRecognized = 1,
  kUnrecognized = 2,
  kUnspecified = 3,
  kMaxValue = kUnspecified
};

// Records for an autofilled field with street name or house number heuristic
// or server predictions, the category of values of autocomplete attribute
// used to represent that field. The categories can be found in the
// `AutocompleteValueForStructuredAddressPredictedFieldsMetric` enum.
void LogHtmlTypesForAutofilledFieldWithStreetNameOrHouseNumberPredictions(
    const AutofillField& field);

// Records if an autofilled field that was determined by heuristic or server
// predictions as a street name or a house number, and whose street name or
// house number prediction was granted precedence over the autocomplete
// attribute, was accepted or corrected by the user.
void LogEditedAutofilledFieldWithStreetNameOrHouseNumberPrecedenceAtSubmission(
    const AutofillField& field);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PRECEDENCE_OVER_AUTOCOMPLETE_METRICS_H_
