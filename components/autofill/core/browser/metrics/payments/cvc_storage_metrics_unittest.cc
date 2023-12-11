// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/cvc_storage_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"

namespace autofill::autofill_metrics {

class CvcStorageMetricsTest : public AutofillMetricsBaseTest,
                              public testing::Test {
 public:
  CvcStorageMetricsTest() = default;
  ~CvcStorageMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }
};

TEST_F(CvcStorageMetricsTest, LogCvcStorageIsEnabledAtStartup) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  personal_data().SetIsPaymentCvcStorageEnabled(true);
  personal_data().SetSyncServiceForTest(nullptr);  // Undo work in base suite.
  personal_data().Init(scoped_refptr<AutofillWebDataService>(nullptr),
                       /*account_database=*/nullptr,
                       /*pref_service=*/autofill_client_->GetPrefs(),
                       /*local_state=*/autofill_client_->GetPrefs(),
                       /*identity_manager=*/nullptr,
                       /*history_service=*/nullptr,
                       /*sync_service=*/nullptr,
                       /*strike_database=*/nullptr,
                       /*image_fetcher=*/nullptr);

  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.CvcStorageIsEnabled.Startup", true, 1);
}

TEST_F(CvcStorageMetricsTest, LogCvcStorageIsDisabledAtStartup) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);

  personal_data().SetIsPaymentCvcStorageEnabled(false);
  personal_data().SetSyncServiceForTest(nullptr);  // Undo work in base suite.
  personal_data().Init(scoped_refptr<AutofillWebDataService>(nullptr),
                       /*account_database=*/nullptr,
                       /*pref_service=*/autofill_client_->GetPrefs(),
                       /*local_state=*/autofill_client_->GetPrefs(),
                       /*identity_manager=*/nullptr,
                       /*history_service=*/nullptr,
                       /*sync_service=*/nullptr,
                       /*strike_database=*/nullptr,
                       /*image_fetcher=*/nullptr);

  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.CvcStorageIsEnabled.Startup", false, 1);
}

}  // namespace autofill::autofill_metrics
