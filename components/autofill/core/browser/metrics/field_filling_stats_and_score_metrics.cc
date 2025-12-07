// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::autofill_metrics {

namespace {

void LogFieldFillingStats(const std::string& histogram_name,
                          const FormGroupFillingStats& filling_stats) {
  // Do not acquire metrics if autofill was not used.
  if (filling_stats.TotalFilled() == 0) {
    return;
  }
  for (size_t i = 0; i < filling_stats.num_accepted; ++i) {
    base::UmaHistogramEnumeration(histogram_name,
                                  FieldFillingStatus::kAccepted);
  }
  for (size_t i = 0; i < filling_stats.num_corrected_to_same_type; ++i) {
    base::UmaHistogramEnumeration(histogram_name,
                                  FieldFillingStatus::kCorrectedToSameType);
  }
  for (size_t i = 0; i < filling_stats.num_corrected_to_different_type; ++i) {
    base::UmaHistogramEnumeration(
        histogram_name, FieldFillingStatus::kCorrectedToDifferentType);
  }
  for (size_t i = 0; i < filling_stats.num_corrected_to_unknown_type; ++i) {
    base::UmaHistogramEnumeration(histogram_name,
                                  FieldFillingStatus::kCorrectedToUnknownType);
  }
  for (size_t i = 0; i < filling_stats.num_corrected_to_empty; ++i) {
    base::UmaHistogramEnumeration(histogram_name,
                                  FieldFillingStatus::kCorrectedToEmpty);
  }
  for (size_t i = 0; i < filling_stats.num_manually_filled_to_same_type; ++i) {
    base::UmaHistogramEnumeration(
        histogram_name, FieldFillingStatus::kManuallyFilledToSameType);
  }
  for (size_t i = 0; i < filling_stats.num_manually_filled_to_different_type;
       ++i) {
    base::UmaHistogramEnumeration(
        histogram_name, FieldFillingStatus::kManuallyFilledToDifferentType);
  }
  for (size_t i = 0; i < filling_stats.num_manually_filled_to_unknown_type;
       ++i) {
    base::UmaHistogramEnumeration(
        histogram_name, FieldFillingStatus::kManuallyFilledToUnknownType);
  }
  for (size_t i = 0; i < filling_stats.num_left_empty; ++i) {
    base::UmaHistogramEnumeration(histogram_name,
                                  FieldFillingStatus::kLeftEmpty);
  }
}

// Logs a form-wide score for the fields of `form_type` based on the
// field-wise `filling_stats`. The score is calculated as follows:
// S = 2*number(filled and accepted) - 3*number(filled and corrected) + 100
// Note that the score is offset by 100 since UMA cannot log negative numbers
// It is also limited to 200.
// Each filled and accepted field contributes to a positive score of 2, while
// each filled and correct field contributes with a negative score of 3.
// The metric is only recorded if at least one field was accepted or
// corrected.
void LogFormFillingScore(FormType form_type,
                         const FormGroupFillingStats& filling_stats) {
  // Do not acquire metrics if Autofill was not used in this form group.
  if (filling_stats.TotalFilled() == 0) {
    return;
  }

  const int score =
      2 * filling_stats.num_accepted - 3 * filling_stats.TotalCorrected() + 100;

  // Make sure that the score is between 0 and 200 since we are only emitting to
  // a histogram with equally distributed 201 buckets.
  base::UmaHistogramCustomCounts(
      base::StrCat(
          {"Autofill.FormFillingScore.", FormTypeToStringView(form_type)}),
      std::clamp(score, 1, 200), 1, 200, 200);
}

// Similar to LogFormFillingScore but with a different score function:
// S = number(filled and accepted) * 10 + number(corrected)
// This score serves as a 2D histogram to record the number of corrected and
// accepted fields into a single histogram.
// Note that the number of accepted fields is limited to 19 and the number of
// corrected fields is limited to 9.
// A score of 45 would mean that 4 fields have been accepted and 5 corrected.
// The metric is only recorded if at least one field was accepted or
// corrected.
void LogFormFillingComplexScore(FormType form_type,
                                const FormGroupFillingStats& filling_stats) {
  // Do not acquire metrics if Autofill was not used in this form group.
  if (filling_stats.TotalFilled() == 0) {
    return;
  }

  // Limit the number of accepted fields to 19 and the number of corrected
  // fields to 9.
  const size_t value_min = 0;

  const size_t clamped_accepted = std::clamp(
      filling_stats.num_accepted, value_min, static_cast<size_t>(19));
  const size_t clamped_corrected = std::clamp(
      filling_stats.TotalCorrected(), value_min, static_cast<size_t>(9));

  const int complex_score = clamped_accepted * 10 + clamped_corrected;

  // The metric is tracked to an histogram with 199 equally distributed buckets.
  base::UmaHistogramCustomCounts(
      base::StrCat({"Autofill.FormFillingComplexScore.",
                    FormTypeToStringView(form_type)}),
      complex_score, 1, 199, 199);
}

}  // namespace

void FormGroupFillingStats::AddFieldFillingStatus(FieldFillingStatus status) {
  switch (status) {
    case FieldFillingStatus::kAccepted:
      num_accepted++;
      return;
    case FieldFillingStatus::kCorrectedToSameType:
      num_corrected_to_same_type++;
      return;
    case FieldFillingStatus::kCorrectedToDifferentType:
      num_corrected_to_different_type++;
      return;
    case FieldFillingStatus::kCorrectedToUnknownType:
      num_corrected_to_unknown_type++;
      return;
    case FieldFillingStatus::kCorrectedToEmpty:
      num_corrected_to_empty++;
      return;
    case FieldFillingStatus::kManuallyFilledToSameType:
      num_manually_filled_to_same_type++;
      return;
    case FieldFillingStatus::kManuallyFilledToDifferentType:
      num_manually_filled_to_different_type++;
      return;
    case FieldFillingStatus::kManuallyFilledToUnknownType:
      num_manually_filled_to_unknown_type++;
      return;
    case FieldFillingStatus::kLeftEmpty:
      num_left_empty++;
      return;
  }
  NOTREACHED();
}

FieldFillingStatus GetFieldFillingStatus(const AutofillField& field) {
  // TODO: crbug.com/40227496 - This metric treats fields whose value didn't
  // change since page load inconsistently. For example, consider an unchanged,
  // non-empty <input>. If its type is ADDRESS_HOME_{STATE,COUNTRY}, the field
  // is counted as `kManuallyFilled*`; otherwise it's counted as `kLeftEmpty`.
  const bool is_empty = field.value_for_import().empty();
  const bool possible_types_empty =
      !FieldHasMeaningfulPossibleFieldTypes(field);
  const bool possible_types_contain_type = TypeOfFieldIsPossibleType(field);
  if (field.is_autofilled()) {
    return FieldFillingStatus::kAccepted;
  }
  if (field.previously_autofilled()) {
    if (is_empty) {
      return FieldFillingStatus::kCorrectedToEmpty;
    }
    if (possible_types_contain_type) {
      return FieldFillingStatus::kCorrectedToSameType;
    }
    if (possible_types_empty) {
      return FieldFillingStatus::kCorrectedToUnknownType;
    }
    return FieldFillingStatus::kCorrectedToDifferentType;
  }
  if (is_empty) {
    return FieldFillingStatus::kLeftEmpty;
  }
  if (possible_types_contain_type) {
    return FieldFillingStatus::kManuallyFilledToSameType;
  }
  if (possible_types_empty) {
    return FieldFillingStatus::kManuallyFilledToUnknownType;
  }
  return FieldFillingStatus::kManuallyFilledToDifferentType;
}

FormGroupFillingStats GetFormFillingStatsForFormType(
    FormType form_type,
    const FormStructure& form_structure) {
  DCHECK_NE(form_type, FormType::kUnknownFormType);
  FormGroupFillingStats filling_stats_for_form_type;

  for (auto& field : form_structure) {
    if (!field->Type().GetFormTypes().contains(form_type)) {
      continue;
    }
    filling_stats_for_form_type.AddFieldFillingStatus(
        GetFieldFillingStatus(*field));
  }
  return filling_stats_for_form_type;
}

void LogFieldFillingStatsAndScore(const FormStructure& form) {
  // Tracks how many fields are filled, unfilled or corrected.
  FormGroupFillingStats address_field_stats;
  FormGroupFillingStats postal_address_field_stats;
  FormGroupFillingStats cc_field_stats;
  FormGroupFillingStats ac_unrecognized_address_field_stats;
  FormGroupFillingStats unclassified_fields_field_stats;
  const bool is_postal_address_form = internal::IsPostalAddressForm(form);
  for (const std::unique_ptr<AutofillField>& field : form) {
    // For any field that belongs to either an address or a credit card form,
    // collect the type-unspecific field filling statistics.
    // Those are only emitted when autofill was used on at least one field of
    // the form.
    const DenseSet<FormType> form_type_of_field = field->Type().GetFormTypes();
    const bool is_address_form_field =
        form_type_of_field.contains(FormType::kAddressForm);
    const bool is_credit_card_form_field =
        form_type_of_field.contains(FormType::kCreditCardForm);
    if (!is_address_form_field && !is_credit_card_form_field) {
      FieldFillingStatus field_stats = GetFieldFillingStatus(*field);
      unclassified_fields_field_stats.AddFieldFillingStatus(field_stats);
      continue;
    }
    if (is_address_form_field &&
        (field->filling_product() == FillingProduct::kAddress ||
         field->filling_product() == FillingProduct::kNone)) {
      address_field_stats.AddFieldFillingStatus(GetFieldFillingStatus(*field));
      if (is_postal_address_form) {
        postal_address_field_stats.AddFieldFillingStatus(
            GetFieldFillingStatus(*field));
      }
    }
    if (is_credit_card_form_field &&
        (field->filling_product() == FillingProduct::kCreditCard ||
         field->filling_product() == FillingProduct::kNone)) {
      cc_field_stats.AddFieldFillingStatus(GetFieldFillingStatus(*field));
    }
    if (is_address_form_field &&
        (field->filling_product() == FillingProduct::kAddress ||
         field->filling_product() == FillingProduct::kNone) &&
        field->ShouldSuppressSuggestionsAndFillingByDefault()) {
      ac_unrecognized_address_field_stats.AddFieldFillingStatus(
          GetFieldFillingStatus(*field));
    }
  }
  LogFieldFillingStats("Autofill.FieldFillingStats.Address",
                       address_field_stats);
  LogFieldFillingStats("Autofill.FieldFillingStats.PostalAddress",
                       postal_address_field_stats);
  LogFieldFillingStats("Autofill.FieldFillingStats.CreditCard", cc_field_stats);
  LogFieldFillingStats("Autofill.AutocompleteUnrecognized.FieldFillingStats2",
                       ac_unrecognized_address_field_stats);
  LogFormFillingScore(FormType::kAddressForm, address_field_stats);
  LogFormFillingScore(FormType::kCreditCardForm, cc_field_stats);

  LogFormFillingComplexScore(FormType::kAddressForm, address_field_stats);
  LogFormFillingComplexScore(FormType::kCreditCardForm, cc_field_stats);
}

}  // namespace autofill::autofill_metrics
