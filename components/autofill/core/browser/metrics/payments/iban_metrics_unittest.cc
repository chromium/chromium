// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class IbanMetricsTest : public AutofillMetricsBaseTest, public testing::Test {
 public:
  IbanMetricsTest() = default;
  ~IbanMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

TEST_F(IbanMetricsTest, LogStoredIbanMetrics) {
  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::Days(30);

  std::vector<std::unique_ptr<Iban>> local_ibans;
  local_ibans.reserve(5);

  // Create 2 in-use IBANs, one with nickname and the other not.
  Iban iban_in_use_0 = test::GetLocalIban();
  iban_in_use_0.set_use_date(one_month_ago);
  iban_in_use_0.set_use_count(10);
  local_ibans.push_back(std::make_unique<Iban>(std::move(iban_in_use_0)));

  Iban iban_in_use_1 = test::GetLocalIban();
  iban_in_use_1.set_use_date(one_month_ago);
  iban_in_use_1.set_use_count(10);
  iban_in_use_1.set_nickname(u"My doctor's IBAN");
  local_ibans.push_back(std::make_unique<Iban>(std::move(iban_in_use_1)));

  // Create 3 in-disuse IBANs.
  for (int i = 0; i < 3; ++i) {
    Iban iban_in_disuse = test::GetLocalIban();
    iban_in_disuse.set_use_date(now - base::Days(200));
    iban_in_disuse.set_use_count(10);
    local_ibans.push_back(std::make_unique<Iban>(std::move(iban_in_disuse)));
  }

  base::HistogramTester histogram_tester;
  autofill_metrics::LogStoredIbanMetrics(local_ibans,
                                         kDisusedDataModelTimeDelta);

  // Validate the count metrics.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredIbanCount.Local.WithNickname", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredIbanCount.Local", 1);

  // Validate the disused count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredIbanDisusedCount.Local", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredIbanDisusedCount.Local", 3,
                                     1);

  // Validate the days-since-last-use metrics.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 5);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 30, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 200, 3);
}

}  // namespace autofill::autofill_metrics
