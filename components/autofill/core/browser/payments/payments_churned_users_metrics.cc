// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_churned_users_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogPaymentsChurnedUsersUiResult(PaymentsUiClosedReason closed_reason) {
  base::UmaHistogramEnumeration("Autofill.PaymentsChurnedUsersUi.Result",
                                closed_reason);
}

void LogPaymentsChurnedUsersUiShowResult(
    PaymentsChurnedUsersUiShowResult result) {
  base::UmaHistogramEnumeration("Autofill.PaymentsChurnedUsersUi.ShowResult",
                                result);
}

}  // namespace autofill::autofill_metrics
