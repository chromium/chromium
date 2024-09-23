// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
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
  base::Time two_months_ago = now - base::Days(60);

  std::vector<std::unique_ptr<Iban>> local_ibans;
  std::vector<std::unique_ptr<Iban>> server_ibans;

  // Create 2 in-use IBANs, one with nickname and the other not. Save as local
  // IBANs.
  Iban iban_in_use_0 = test::GetLocalIban();
  iban_in_use_0.set_use_date(one_month_ago);
  iban_in_use_0.set_use_count(10);
  iban_in_use_0.set_nickname(u"");
  local_ibans.push_back(std::make_unique<Iban>(std::move(iban_in_use_0)));

  Iban iban_in_use_1 = test::GetLocalIban();
  iban_in_use_1.set_use_date(one_month_ago);
  iban_in_use_1.set_use_count(10);
  iban_in_use_1.set_nickname(u"My doctor's IBAN");
  local_ibans.push_back(std::make_unique<Iban>(std::move(iban_in_use_1)));

  // Create 2 in-use IBANs, one with nickname and the other not. Save as server
  // IBANs.
  Iban iban_in_use_2 = test::GetServerIban();
  iban_in_use_2.set_use_date(two_months_ago);
  iban_in_use_2.set_use_count(10);
  iban_in_use_2.set_nickname(u"");
  server_ibans.push_back(std::make_unique<Iban>(std::move(iban_in_use_2)));

  Iban iban_in_use_3 = test::GetServerIban2();
  iban_in_use_3.set_use_date(two_months_ago);
  iban_in_use_3.set_use_count(10);
  iban_in_use_3.set_nickname(u"My doctor's IBAN");
  server_ibans.push_back(std::make_unique<Iban>(std::move(iban_in_use_3)));

  // Create 3 disused IBANs. Saved as both local and server IBANs.
  for (int i = 0; i < 3; ++i) {
    Iban local_iban_in_disuse = test::GetLocalIban();
    local_iban_in_disuse.set_use_date(now - base::Days(200));
    local_iban_in_disuse.set_use_count(10);
    local_iban_in_disuse.set_nickname(u"");
    local_ibans.push_back(
        std::make_unique<Iban>(std::move(local_iban_in_disuse)));

    Iban server_disused_iban = test::GetServerIban();
    server_disused_iban.set_use_date(now - base::Days(250));
    server_disused_iban.set_use_count(10);
    server_disused_iban.set_nickname(u"");
    server_ibans.push_back(std::make_unique<Iban>(server_disused_iban));
  }

  base::HistogramTester histogram_tester;
  autofill_metrics::LogStoredIbanMetrics(local_ibans, server_ibans,
                                         kDisusedDataModelTimeDelta);

  // Validate the count metrics.
  histogram_tester.ExpectUniqueSample("Autofill.StoredIbanCount", 10, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredIbanCount.Local.WithNickname", 1, 1);
  histogram_tester.ExpectUniqueSample("Autofill.StoredIbanCount.Local", 5, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredIbanCount.Server.WithNickname", 1, 1);
  histogram_tester.ExpectUniqueSample("Autofill.StoredIbanCount.Server", 5, 1);

  // Validate the disused count metrics.
  histogram_tester.ExpectUniqueSample("Autofill.StoredIbanDisusedCount.Local",
                                      3, 1);
  histogram_tester.ExpectUniqueSample("Autofill.StoredIbanDisusedCount.Server",
                                      3, 1);

  // Validate the days-since-last-use metrics.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 5);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 30, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 200, 3);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Server", 5);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredIban.Server", 60, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredIban.Server", 250, 3);
}

TEST_F(IbanMetricsTest, LogIbanSaveOfferedCountry) {
  base::HistogramTester histogram_tester;
  autofill_metrics::LogIbanSaveOfferedCountry("FR");
  histogram_tester.ExpectUniqueSample("Autofill.Iban.CountryOfSaveOfferedIban",
                                      Iban::IbanSupportedCountry::kFR, 1);
}

TEST_F(IbanMetricsTest, LogIbanSaveAcceptedCountry) {
  base::HistogramTester histogram_tester;
  autofill_metrics::LogIbanSaveAcceptedCountry("FR");
  histogram_tester.ExpectUniqueSample("Autofill.Iban.CountryOfSaveAcceptedIban",
                                      Iban::IbanSupportedCountry::kFR, 1);
}

TEST_F(IbanMetricsTest, LogIbanSelectedCountry) {
  base::HistogramTester histogram_tester;
  autofill_metrics::LogIbanSelectedCountry("FR");
  histogram_tester.ExpectUniqueSample("Autofill.Iban.CountryOfSelectedIban",
                                      Iban::IbanSupportedCountry::kFR, 1);
}

}  // namespace autofill::autofill_metrics
