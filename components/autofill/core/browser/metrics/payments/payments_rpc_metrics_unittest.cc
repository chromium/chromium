// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/payments_rpc_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

using PaymentsRpcResult =
    ::autofill::payments::PaymentsAutofillClient::PaymentsRpcResult;

}  // namespace

// Tests that a successful RPC maps to `kSuccess`.
TEST(PaymentsRpcMetricsTest, GetPaymentsRpcResultMetric_Success) {
  EXPECT_EQ(GetPaymentsRpcResultMetric(PaymentsRpcResult::kSuccess),
            PaymentsRpcResultMetric::kSuccess);
}

// Tests that a client-side timeout maps to `kTimeout`.
TEST(PaymentsRpcMetricsTest, GetPaymentsRpcResultMetric_Timeout) {
  EXPECT_EQ(GetPaymentsRpcResultMetric(PaymentsRpcResult::kClientSideTimeout),
            PaymentsRpcResultMetric::kTimeout);
}

// Tests that all other RPC results map to `kFailure`.
TEST(PaymentsRpcMetricsTest, GetPaymentsRpcResultMetric_Failure) {
  for (PaymentsRpcResult result :
       {PaymentsRpcResult::kNone, PaymentsRpcResult::kTryAgainFailure,
        PaymentsRpcResult::kPermanentFailure, PaymentsRpcResult::kNetworkError,
        PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
        PaymentsRpcResult::kVcnRetrievalPermanentFailure}) {
    EXPECT_EQ(GetPaymentsRpcResultMetric(result),
              PaymentsRpcResultMetric::kFailure);
  }
}

}  // namespace autofill::autofill_metrics
