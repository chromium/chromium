// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/quality_metrics_filling.h"

#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

namespace {

// Heuristic used for filtering fields that are probably not fillable. The
// assumption is that autofilled values typically should have lengths well below
// 150 and that extremely long texts are outliers and should not influence the
// metrics a lot.
constexpr size_t kAutomationRateFieldSizeThreshold = 150;

constexpr std::string_view kUmaDataUtilizationAllTypes =
    "Autofill.DataUtilization.AllFieldTypes.";
constexpr std::string_view kUmaDataUtilizationSelectedTypes =
    "Autofill.DataUtilization.SelectedFieldTypes.";
// Variants for histograms "Autofill.DataUtilization*".
constexpr std::string_view kAggregateVariant = "Aggregate";
constexpr std::string_view kGarbageVariant = "Garbage";
constexpr std::string_view kHadPredictionVariant = "HadPrediction";
constexpr std::string_view kNoPredictionVariant = "NoPrediction";
constexpr std::string_view kGarbageHadPredictionVariant =
    "GarbageHadPrediction";

// Field types whose associated values typically are small numbers (< 100). When
// determining the possible types of a submitted field, the small numbers have a
// high chance of causing false positive matches.
constexpr DenseSet<FieldType> kFieldTypesRepresentingSmallNumbers = {
    CREDIT_CARD_EXP_MONTH,     CREDIT_CARD_EXP_2_DIGIT_YEAR,
    PHONE_HOME_COUNTRY_CODE,   PHONE_HOME_NUMBER_PREFIX,
    ADDRESS_HOME_HOUSE_NUMBER, ADDRESS_HOME_APT_NUM,
    ADDRESS_HOME_FLOOR};

// Records the percentage of input text field characters that were autofilled.
void LogAutomationRate(const FormStructure& form) {
  size_t total_length_autofilled_fields = 0;
  size_t total_length = 0;
  for (const auto& field : form.fields()) {
    if (!field->IsTextInputElement()) {
      continue;
    }
    // The field value at form submission should have changed since page load.
    if (!field->initial_value_changed().value_or(true)) {
      continue;
    }
    size_t field_size = field->value(ValueSemantics::kCurrent).size();
    // Skip fields containing too many characters to reduce distortion by
    // fields that are likely not autofillable.
    if (field_size > kAutomationRateFieldSizeThreshold) {
      continue;
    }
    if (field->is_autofilled()) {
      total_length_autofilled_fields += field_size;
    }
    total_length += field_size;
  }
  if (total_length > 0) {
    for (const auto form_type : GetFormTypesForLogging(form)) {
      base::UmaHistogramPercentage(
          base::StrCat({"Autofill.AutomationRate.",
                        FormTypeNameForLoggingToStringView(form_type)}),
          100 * total_length_autofilled_fields / total_length);
    }
  }
}

int GetFieldTypeAutofillDataUtilization(
    FieldType field_type,
    AutofillDataUtilization data_utilization) {
  static_assert(FieldType::MAX_VALID_FIELD_TYPE <= (UINT16_MAX >> 6),
                "Autofill::FieldType value needs more than 10 bits.");

  // Technically only 1 bit is required at this time. Reserving more bits for
  // potential future expansion.
  static_assert(static_cast<int>(AutofillDataUtilization::kMaxValue) <=
                    (UINT16_MAX >> 10),
                "AutofillDataUtilization value needs more than 6 bits");

  return (field_type << 6) | static_cast<int>(data_utilization);
}

// Records, for fields that were submitted with values that were found in the
// user's stored address profiles / credit cards, whether the field value was
// autofilled or manually entered by the user. Note that fields that were
// autofilled and then edited by the user or JavaScript count as "manually
// entered". Note that fields that were submitted with a prefilled value
// don't get recorded. Emitted on form submission.
void LogDataUtilization(const FormStructure& form) {
  for (const auto& field : form.fields()) {
    // A pre-filled field value should have changed since page load. Otherwise,
    // no reporting is necessary.
    if (field->initial_value_changed().has_value() &&
        !field->initial_value_changed().value()) {
      continue;
    }
    // Determine fillable possible types.
    DenseSet<FieldType> fillable_possible_types;
    for (FieldType possible_type : field->possible_types()) {
      if (IsFillableFieldType(possible_type)) {
        fillable_possible_types.insert(possible_type);
      }
    }
    if (fillable_possible_types.empty()) {
      continue;
    }
    // Determine if "SelectedFieldTypes" variants should be logged.
    const bool kLogSelectedTypes = !fillable_possible_types.contains_any(
        kFieldTypesRepresentingSmallNumbers);

    const AutofillDataUtilization sample =
        field->is_autofilled() ? AutofillDataUtilization::kAutofilled
                               : AutofillDataUtilization::kNotAutofilled;

    for (std::string_view histogram_base :
         {kUmaDataUtilizationAllTypes, kUmaDataUtilizationSelectedTypes}) {
      if (histogram_base == kUmaDataUtilizationSelectedTypes &&
          !kLogSelectedTypes) {
        continue;
      }
      // Emit "Aggregate" variants.
      base::UmaHistogramEnumeration(
          base::StrCat({histogram_base, kAggregateVariant}), sample);

      // Emit "Garbage" variants.
      const bool kAutocompleteStateIsGarbage =
          AutofillMetrics::AutocompleteStateForSubmittedField(*field) ==
          AutofillMetrics::AutocompleteState::kGarbage;
      if (kAutocompleteStateIsGarbage) {
        base::UmaHistogramEnumeration(
            base::StrCat({histogram_base, kGarbageVariant}), sample);
      }

      // Emit "HadPrediction" and "NoPrediction" variants.
      const bool kHadPrediction =
          field->Type().GetStorableType() > FieldType::EMPTY_TYPE;
      const std::string_view kPredictionVariant =
          kHadPrediction ? kHadPredictionVariant : kNoPredictionVariant;
      base::UmaHistogramEnumeration(
          base::StrCat({histogram_base, kPredictionVariant}), sample);

      // Emit "GarbageHadPrediction" variants.
      if (kHadPrediction && kAutocompleteStateIsGarbage) {
        base::UmaHistogramEnumeration(
            base::StrCat({histogram_base, kGarbageHadPredictionVariant}),
            sample);
      }
    }

    // Emit breakdown by possible type.
    for (FieldType type : fillable_possible_types) {
      base::UmaHistogramSparse(
          "Autofill.DataUtilization.ByPossibleType",
          GetFieldTypeAutofillDataUtilization(type, sample));
    }
  }
}

}  // namespace

void LogFillingQualityMetrics(const FormStructure& form) {
  LogAutomationRate(form);
  LogDataUtilization(form);
}

}  // namespace autofill::autofill_metrics
