// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/omnibox_autofill_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogOmniboxAutofillShowChipDecisionPart1(
    OmniboxAutofillShowChipDecisionPart1 metric) {
  base::UmaHistogramEnumeration(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1", metric);
}

}  // namespace autofill::autofill_metrics
