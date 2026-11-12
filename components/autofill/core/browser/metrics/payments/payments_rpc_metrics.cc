// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/payments_rpc_metrics.h"

#include "base/notreached.h"

namespace autofill::autofill_metrics {

PaymentsRpcResultMetric GetPaymentsRpcResultMetric(
    payments::PaymentsAutofillClient::PaymentsRpcResult result) {
  using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
  switch (result) {
    case PaymentsRpcResult::kSuccess:
      return PaymentsRpcResultMetric::kSuccess;
    case PaymentsRpcResult::kClientSideTimeout:
      return PaymentsRpcResultMetric::kTimeout;
    case PaymentsRpcResult::kNone:
    case PaymentsRpcResult::kTryAgainFailure:
    case PaymentsRpcResult::kPermanentFailure:
    case PaymentsRpcResult::kNetworkError:
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      return PaymentsRpcResultMetric::kFailure;
  }
  NOTREACHED();
}

}  // namespace autofill::autofill_metrics
