// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/converge_to_extreme_length_address_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogAddressUpdateLengthConvergenceStatus(bool convergence_status) {
  base::UmaHistogramBoolean("Autofill.NewerStreetAddressWithSameStatusIsChosen",
                            convergence_status);
}

}  // namespace autofill::autofill_metrics
