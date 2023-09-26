// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/address_rewriter_in_profile_subset_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace autofill::autofill_metrics {

void LogProfilesDifferOnAddressLineOnly(bool has_different_address) {
  base::UmaHistogramBoolean("Autofill.ProfilesDifferOnAddressLineOnly",
                            has_different_address);
}

void LogUserAcceptedPreviouslyHiddenProfileSuggestion() {
  base::RecordAction(
      base::UserMetricsAction("Autofill_AcceptedPreviouslyHiddenSuggestion"));
}

}  // namespace autofill::autofill_metrics
