// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_churned_users_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogPaymentsChurnedUsersBubbleResult(PaymentsUiClosedReason closed_reason) {
  base::UmaHistogramEnumeration("Autofill.PaymentsChurnedUsersBubble.Result",
                                closed_reason);
}

void LogPaymentsChurnedUsersBubbleShowResult(
    PaymentsChurnedUsersBubbleShowResult result) {
  base::UmaHistogramEnumeration(
      "Autofill.PaymentsChurnedUsersBubble.ShowResult", result);
}

}  // namespace autofill::autofill_metrics
