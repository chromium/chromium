// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

class BnplMetricsTest : public AutofillMetricsBaseTest, public testing::Test {
 public:
  BnplMetricsTest() = default;
  ~BnplMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

// BNPL is currently only available for desktop platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Test that we log when the user flips the BNPL enabled toggle.
TEST_F(BnplMetricsTest, LogBnplPrefToggled) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(autofill_client_->GetPrefs()->GetBoolean(
      autofill::prefs::kAutofillBnplEnabled));
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", true,
                                     0);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", false,
                                     0);

  autofill_client_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillBnplEnabled, false);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", true,
                                     0);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", false,
                                     1);

  autofill_client_->GetPrefs()->SetBoolean(
      autofill::prefs::kAutofillBnplEnabled, true);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", true,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.SettingsPage.BnplToggled", false,
                                     1);
}

TEST_F(BnplMetricsTest, LogBnplIssuersSyncedCountAtStartup) {
  base::HistogramTester histogram_tester;

  int count = 5;
  LogBnplIssuersSyncedCountAtStartup(count);
  histogram_tester.ExpectBucketCount("Autofill.Bnpl.IssuersSyncedCount.Startup",
                                     count, 1);

  count = 25;
  LogBnplIssuersSyncedCountAtStartup(count);
  histogram_tester.ExpectBucketCount("Autofill.Bnpl.IssuersSyncedCount.Startup",
                                     count, 1);

  histogram_tester.ExpectTotalCount("Autofill.Bnpl.IssuersSyncedCount.Startup",
                                    2);
}

TEST_F(BnplMetricsTest, LogBnplTosDialogShownZip) {
  base::HistogramTester histogram_tester;
  LogBnplTosDialogShown(kBnplZipIssuerId);
  histogram_tester.ExpectUniqueSample("Autofill.Bnpl.TosDialogShown.Zip",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

TEST_F(BnplMetricsTest, LogBnplTosDialogShownAffirm) {
  base::HistogramTester histogram_tester;
  LogBnplTosDialogShown(kBnplAffirmIssuerId);
  histogram_tester.ExpectUniqueSample("Autofill.Bnpl.TosDialogShown.Affirm",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

TEST_F(BnplMetricsTest, LogBnplTosDialogShownAfterpay) {
  base::HistogramTester histogram_tester;
  LogBnplTosDialogShown(kBnplAfterpayIssuerId);
  histogram_tester.ExpectUniqueSample("Autofill.Bnpl.TosDialogShown.Afterpay",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace autofill::autofill_metrics
