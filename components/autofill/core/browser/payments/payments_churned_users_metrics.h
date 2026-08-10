// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_METRICS_H_

#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"

namespace autofill::autofill_metrics {

// Logs the result of the payments churned users bubble.
void LogPaymentsChurnedUsersBubbleResult(PaymentsUiClosedReason closed_reason);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_METRICS_H_
