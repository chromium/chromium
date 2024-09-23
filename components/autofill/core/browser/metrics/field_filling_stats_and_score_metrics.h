// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"

namespace autofill::autofill_metrics {

// The filling status of an autofilled field.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FieldFillingStatus {
  // The field was filled and accepted.
  kAccepted = 0,
  // The field was filled and corrected to a value of the same type.
  kCorrectedToSameType = 1,
  // The field was filled and corrected to a value of a different type.
  kCorrectedToDifferentType = 2,
  // The field was filled and corrected to a value of an unknown type.
  kCorrectedToUnknownType = 3,
  // The field was filled and the value was cleared afterwards.
  kCorrectedToEmpty = 4,
  // The field was manually filled to a value of the same type as the
  // field was predicted to.
  kManuallyFilledToSameType = 5,
  // The field was manually filled to a value of a different type as the field
  // was predicted to.
  kManuallyFilledToDifferentType = 6,
  // The field was manually filled to a value of an unknown type.
  kManuallyFilledToUnknownType = 7,
  // The field was left empty.
  kLeftEmpty = 8,
  kMaxValue = kLeftEmpty
};

// Helper struct to count the `FieldFillingStatus` for a form group like
// addresses and credit cards.
struct FormGroupFillingStats {
  // Please have a look at AutofillMetrics::FieldFillingStatus for the meaning
  // of the different fields.
  size_t num_accepted = 0;
  size_t num_corrected_to_same_type = 0;
  size_t num_corrected_to_different_type = 0;
  size_t num_corrected_to_unknown_type = 0;
  size_t num_corrected_to_empty = 0;
  size_t num_manually_filled_to_same_type = 0;
  size_t num_manually_filled_to_different_type = 0;
  size_t num_manually_filled_to_unknown_type = 0;
  size_t num_left_empty = 0;

  size_t TotalCorrected() const {
    return num_corrected_to_same_type + num_corrected_to_different_type +
           num_corrected_to_unknown_type + num_corrected_to_empty;
  }

  size_t TotalManuallyFilled() const {
    return num_manually_filled_to_different_type +
           num_manually_filled_to_unknown_type +
           num_manually_filled_to_same_type;
  }

  size_t TotalUnfilled() const {
    return TotalManuallyFilled() + num_left_empty;
  }

  size_t TotalFilled() const { return num_accepted + TotalCorrected(); }

  size_t Total() const { return TotalFilled() + TotalUnfilled(); }

  void AddFieldFillingStatus(FieldFillingStatus status);
};

// Returns the filling status of `field`.
FieldFillingStatus GetFieldFillingStatus(const AutofillField& field);

// Merge `first` into `second` by summing each attribute from
// `FormGroupFillingStats`.
// TODO(crbug.com/40274514): Remove this on cleanup.
void MergeFormGroupFillingStats(const FormGroupFillingStats& first,
                                FormGroupFillingStats& second);

// Returns the `FormGroupFillingStats` corresponding to the fields in
// `form_structure` that match `form_type`. This method does not log to UMA but
// only returns the statistics of a submitted form. `FormGroupFillingStats` is
// UMA logged in `LogQualityMetrics()`.
autofill_metrics::FormGroupFillingStats GetFormFillingStatsForFormType(
    FormType form_type,
    const FormStructure& form_structure);

// Logs the `filling_stats` of the fields within a `form_type`, and the
// `filling_stats` of ac=unrecognized fields. The filling status
// consistent of the number of accepted, corrected or and unfilled fields. See
// the .cc file for details.
void LogFieldFillingStatsAndScore(const FormStructure& form);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FIELD_FILLING_STATS_AND_SCORE_METRICS_H_
