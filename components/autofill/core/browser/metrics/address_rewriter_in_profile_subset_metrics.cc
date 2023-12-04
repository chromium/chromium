// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/address_rewriter_in_profile_subset_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"

namespace autofill::autofill_metrics {

void LogPreviouslyHiddenProfileSuggestionNumber(size_t hidden_profiles_number) {
  base::UmaHistogramCounts100("Autofill.PreviouslyHiddenSuggestionNumber",
                              hidden_profiles_number);
}

void LogUserAcceptedPreviouslyHiddenProfileSuggestion(bool previously_hidden) {
  base::UmaHistogramBoolean("Autofill.AcceptedPreviouslyHiddenSuggestion",
                            previously_hidden);
}

}  // namespace autofill::autofill_metrics
