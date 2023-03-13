// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/precedence_over_autocomplete_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

void LogHtmlTypesForAutofilledFieldWithStreetNameOrHouseNumberPredictions(
    const AutofillField& field) {
  DCHECK(IsStreetNameOrHouseNumberType(field.server_type()) ||
         IsStreetNameOrHouseNumberType(field.heuristic_type()));
  AutocompleteValueForStructuredAddressPredictedFieldsMetric
      autocomplete_metric;
  switch (field.html_type()) {
    case HtmlFieldType::kAddressLine1:
    case HtmlFieldType::kAddressLine2:
      autocomplete_metric =
          AutocompleteValueForStructuredAddressPredictedFieldsMetric::
              kAddressLine1And2;
      break;
    case HtmlFieldType::kUnrecognized:
      autocomplete_metric =
          AutocompleteValueForStructuredAddressPredictedFieldsMetric::
              kUnrecognized;
      break;
    case HtmlFieldType::kUnspecified:
      autocomplete_metric =
          AutocompleteValueForStructuredAddressPredictedFieldsMetric::
              kUnspecified;
      break;
    default:
      autocomplete_metric =
          AutocompleteValueForStructuredAddressPredictedFieldsMetric::
              kOtherRecognized;
      break;
  }
  auto emit_histogram = [&](const ServerFieldType current_field_type,
                            const base::StringPiece current_prediction_type) {
    if (!IsStreetNameOrHouseNumberType(current_field_type)) {
      return;
    }
    base::StringPiece current_field_type_str =
        current_field_type == ADDRESS_HOME_STREET_NAME ? "StreetName"
                                                       : "HouseNumber";
    for (auto field_type : {base::StringPiece("StreetNameOrHouseNumber"),
                            current_field_type_str}) {
      for (auto prediction_type :
           {base::StringPiece("HeuristicOrServer"), current_prediction_type}) {
        base::UmaHistogramEnumeration(
            base::StrCat({"Autofill.AutocompleteAttributeForFieldsWith.",
                          field_type, ".", prediction_type, ".Prediction"}),
            autocomplete_metric);
      }
    }
  };
  emit_histogram(field.server_type(), "Server");
  emit_histogram(field.heuristic_type(), "Heuristic");
}

void LogEditedAutofilledFieldWithStreetNameOrHouseNumberPrecedenceAtSubmission(
    const AutofillField& field) {
  DCHECK(field.Type().GetStorableType() == field.heuristic_type() ||
         field.Type().GetStorableType() == field.server_type());
  DCHECK(IsStreetNameOrHouseNumberType(field.Type().GetStorableType()));
  DCHECK(field.html_type() != HtmlFieldType::kUnspecified);
  for (const char* current_precedence_type :
       {"HeuristicOrServer",
        field.Type().GetStorableType() == field.heuristic_type() ? "Heuristic"
                                                                 : "Server"}) {
    for (const char* current_field_type :
         {"StreetNameOrHouseNumber",
          field.Type().GetStorableType() == ADDRESS_HOME_STREET_NAME
              ? "StreetName"
              : "HouseNumber"}) {
      for (const char* current_autocomplete_status :
           {"Specified", field.html_type() == HtmlFieldType::kUnrecognized
                             ? "Unrecognized"
                             : "Recognized"}) {
        base::UmaHistogramEnumeration(
            base::StrCat(
                {"Autofill.", current_precedence_type,
                 "Precedence.OverAutocompleteForStructuredAddressFields.",
                 current_field_type, ".AutocompleteIs.",
                 current_autocomplete_status}),
            field.previously_autofilled()
                ? AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_EDITED
                : AutofillMetrics::AutofilledFieldUserEditingStatusMetric::
                      AUTOFILLED_FIELD_WAS_NOT_EDITED);
      }
    }
  }
}

}  // namespace autofill::autofill_metrics
