// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/studies/hats_surveys_util.h"

#include <memory>
#include <ranges>
#include <string>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

// Converts `filling_stats` to a key-value representation, where the key
// is the "stats category" and the value is the number of fields that match
// such category. This is used to show users a survey that will measure the
// perception of Autofill.
HatsSurveyStringData FormFillingStatsToSurveyStringData(
    const autofill_metrics::FormGroupFillingStats& filling_stats) {
  return {
      {"Accepted fields", base::NumberToString(filling_stats.num_accepted)},
      {"Corrected to same type",
       base::NumberToString(filling_stats.num_corrected_to_same_type)},
      {"Corrected to a different type",
       base::NumberToString(filling_stats.num_corrected_to_different_type)},
      {"Corrected to an unknown type",
       base::NumberToString(filling_stats.num_corrected_to_unknown_type)},
      {"Corrected to empty",
       base::NumberToString(filling_stats.num_corrected_to_empty)},
      {"Manually filled to same type",
       base::NumberToString(filling_stats.num_manually_filled_to_same_type)},
      {"Manually filled to a different type",
       base::NumberToString(
           filling_stats.num_manually_filled_to_different_type)},
      {"Manually filled to an unknown type",
       base::NumberToString(filling_stats.num_manually_filled_to_unknown_type)},
      {"Total corrected", base::NumberToString(filling_stats.TotalCorrected())},
      {"Total filled", base::NumberToString(filling_stats.TotalFilled())},
      {"Total unfilled", base::NumberToString(filling_stats.TotalUnfilled())},
      {"Total manually filled",
       base::NumberToString(filling_stats.TotalManuallyFilled())},
      {"Total number of fields", base::NumberToString(filling_stats.Total())}};
}

size_t CountFormType(const FormStructure& form, FormType type) {
  return std::ranges::count_if(
      form.fields(), [type](const std::unique_ptr<AutofillField>& field) {
        return field->Type().GetFormTypes().contains(type);
      });
}

// Returns the product specific data (PSD) of the survey. If the conditions of
// the survey are not satisfied, returns `std::nullopt`.
std::optional<HatsSurveyStringData> GetUserPerceptionSurveyData(
    const FormStructure& submitted_form,
    FormType type,
    size_t min_number_of_fields_of_type,
    const base::Feature& feature) {
  const autofill_metrics::FormGroupFillingStats filling_stats =
      autofill_metrics::GetFormFillingStatsForFormType(type, submitted_form);

  if (CountFormType(submitted_form, type) >= min_number_of_fields_of_type &&
      filling_stats.TotalFilled() > 0 &&
      base::FeatureList::IsEnabled(feature)) {
    return FormFillingStatsToSurveyStringData(filling_stats);
  }
  return std::nullopt;
}

}  // namespace

void MaybeTriggerFormSubmissionHatsSurveys(
    AutofillClient& client,
    const FormStructure& submitted_form) {
  // The minimum required number of fields for a user perception survey for
  // addresses is 4. This makes sure that for example forms that only contain
  // a single email field do not prompt a survey. Such survey answer would
  // likely taint our analysis.
  if (std::optional<HatsSurveyStringData> survey_data =
          GetUserPerceptionSurveyData(
              submitted_form, FormType::kAddressForm,
              /*min_number_of_fields_of_type=*/4,
              features::kAutofillAddressUserPerceptionSurvey)) {
    client.TriggerUserPerceptionOfAutofillSurvey(FillingProduct::kAddress,
                                                 *survey_data);
    return;
  }

  if (std::optional<HatsSurveyStringData> survey_data =
          GetUserPerceptionSurveyData(
              submitted_form, FormType::kCreditCardForm,
              /*min_number_of_fields_of_type=*/1,
              features::kAutofillCreditCardUserPerceptionSurvey)) {
    client.TriggerUserPerceptionOfAutofillSurvey(FillingProduct::kCreditCard,
                                                 *survey_data);
    return;
  }
}

}  // namespace autofill
