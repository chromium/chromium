// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PAYMENTS_RPC_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PAYMENTS_RPC_METRICS_H_

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::autofill_metrics {

// The result of a Payments RPC.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PaymentsRpcResultMetric)
enum class PaymentsRpcResultMetric {
  kSuccess = 0,
  kFailure = 1,
  kTimeout = 2,
  kMaxValue = kTimeout,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillPaymentsRpcResultMetric)

// Converts `result` to the `PaymentsRpcResultMetric` to log.
PaymentsRpcResultMetric GetPaymentsRpcResultMetric(
    payments::PaymentsAutofillClient::PaymentsRpcResult result);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_PAYMENTS_RPC_METRICS_H_
