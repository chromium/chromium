// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/wallet_usage_data_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class WalletUsageDataMetricsTest : public AutofillMetricsBaseTest,
                                   public testing::Test {
 public:
  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

// Tests that we correctly log the number of stored virtual card usage data.
TEST_F(WalletUsageDataMetricsTest, LogStoredVirtualCardUsageMetrics) {
  std::vector<std::unique_ptr<VirtualCardUsageData>> virtual_card_usage_data;
  VirtualCardUsageData virtual_card_usage_data1 =
      test::GetVirtualCardUsageData1();
  VirtualCardUsageData virtual_card_usage_data2 =
      test::GetVirtualCardUsageData2();
  virtual_card_usage_data.push_back(
      std::make_unique<VirtualCardUsageData>(virtual_card_usage_data1));
  virtual_card_usage_data.push_back(
      std::make_unique<VirtualCardUsageData>(virtual_card_usage_data2));

  base::HistogramTester histogram_tester;
  autofill_metrics::LogStoredVirtualCardUsageCount(
      virtual_card_usage_data.size());

  // Validate the count metrics.
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardUsageData.StoredUsageDataCount", 2, 1);
}

}  // namespace autofill::autofill_metrics
