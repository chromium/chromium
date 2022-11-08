// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill {

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
  size_t num_manually_filled_to_differt_type = 0;
  size_t num_manually_filled_to_unknown_type = 0;
  size_t num_left_empty = 0;

  size_t TotalCorrected() const {
    return num_corrected_to_same_type + num_corrected_to_different_type +
           num_corrected_to_unknown_type + num_corrected_to_empty;
  }

  size_t TotalManuallyFilled() const {
    return num_manually_filled_to_differt_type +
           num_manually_filled_to_unknown_type +
           num_manually_filled_to_same_type;
  }

  size_t TotalUnfilled() const {
    return TotalManuallyFilled() + num_left_empty;
  }

  size_t TotalFilled() const { return num_accepted + TotalCorrected(); }

  size_t Total() const { return TotalFilled() + TotalUnfilled(); }

  void AddFieldFillingStatus(AutofillMetrics::FieldFillingStatus status);
};

// Returns the filling status of `field`.
AutofillMetrics::FieldFillingStatus GetFieldFillingStatus(
    const AutofillField& field);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_UTILS_H_
