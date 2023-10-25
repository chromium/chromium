// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/form_types.h"

namespace autofill::autofill_metrics {

namespace {

// Logs the `filling_stats` of the fields within a `form_type`. The filling
// status consistent of the number of accepted, corrected or and unfilled
// fields.
void LogFieldFillingStats(FormType form_type,
                          const FormGroupFillingStats& filling_stats) {
  std::string histogram_prefix = base::StrCat(
      {"Autofill.FieldFillingStats.", FormTypeToStringView(form_type), "."});

  // Do not acquire metrics if autofill was not used in this form group.
  if (filling_stats.TotalFilled() == 0) {
    return;
  }

  // Counts into those histograms are mutually exclusive.
  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "Accepted"}),
                              filling_stats.num_accepted);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "CorrectedToSameType"}),
      filling_stats.num_corrected_to_same_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "CorrectedToDifferentType"}),
      filling_stats.num_corrected_to_different_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "CorrectedToUnknownType"}),
      filling_stats.num_corrected_to_unknown_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "CorrectedToEmpty"}),
      filling_stats.num_corrected_to_empty);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "ManuallyFilledToSameType"}),
      filling_stats.num_manually_filled_to_same_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "ManuallyFilledToDifferentType"}),
      filling_stats.num_manually_filled_to_differt_type);

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "ManuallyFilledToUnknownType"}),
      filling_stats.num_manually_filled_to_unknown_type);

  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "LeftEmpty"}),
                              filling_stats.num_left_empty);

  // Counts into those histograms are not mutually exclusive and a single field
  // can contribute to multiple of those.
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "TotalCorrected"}),
      filling_stats.TotalCorrected());

  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "TotalFilled"}),
                              filling_stats.TotalFilled());

  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "TotalUnfilled"}),
                              filling_stats.TotalUnfilled());

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, "TotalManuallyFilled"}),
      filling_stats.TotalManuallyFilled());

  base::UmaHistogramCounts100(base::StrCat({histogram_prefix, "Total"}),
                              filling_stats.Total());
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

void LogFieldFillingStatsAndScore(
    const FormGroupFillingStats& address_filling_stats,
    const FormGroupFillingStats& cc_filling_stats) {
  LogFieldFillingStats(FormType::kAddressForm, address_filling_stats);
  LogFieldFillingStats(FormType::kCreditCardForm, cc_filling_stats);

  LogFormFillingScore(FormType::kAddressForm, address_filling_stats);
  LogFormFillingScore(FormType::kCreditCardForm, cc_filling_stats);

  LogFormFillingComplexScore(FormType::kAddressForm, address_filling_stats);
  LogFormFillingComplexScore(FormType::kCreditCardForm, cc_filling_stats);
}

}  // namespace autofill::autofill_metrics
