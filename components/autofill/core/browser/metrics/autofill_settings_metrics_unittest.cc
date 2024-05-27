// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

class AutofillSettingsMetricsTest : public AutofillMetricsBaseTest,
                                    public testing::TestWithParam<bool> {
 public:
  ~AutofillSettingsMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

 protected:
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(, AutofillSettingsMetricsTest, ::testing::Bool());

// Test that we log that Profile Autofill is enabled / disabled when filling a
// form.
TEST_P(AutofillSettingsMetricsTest, LogsAutofillProfileIsEnabledAtPageLoad) {
  autofill_manager().SetAutofillProfileEnabled(*autofill_client_, GetParam());
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});
  histogram_tester_.ExpectUniqueSample("Autofill.Address.IsEnabled.PageLoad",
                                       GetParam(), 1);
}

// Test that we log that CreditCard Autofill is enabled / disabled when filling
// a form.
TEST_P(AutofillSettingsMetricsTest, AutofillCreditCardIsEnabledAtPageLoad) {
  autofill_manager().SetAutofillPaymentMethodsEnabled(*autofill_client_,
                                                      GetParam());
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});
  histogram_tester_.ExpectUniqueSample("Autofill.CreditCard.IsEnabled.PageLoad",
                                       GetParam(), 1);
}

// Verify that we correctly log the IsEnabled metrics with signed in sync state.
TEST_P(AutofillSettingsMetricsTest,
       LogIsAutofillEnabledAtPageLoadSignedInSyncState) {
  LogIsAutofillEnabledAtPageLoad(
      /*enabled=*/GetParam(), AutofillMetrics::PaymentsSigninState::kSignedIn);
  histogram_tester_.ExpectBucketCount("Autofill.IsEnabled.PageLoad.SignedIn",
                                      GetParam(), 1);
  // Make sure the metric without the sync state is still recorded.
  histogram_tester_.ExpectBucketCount("Autofill.IsEnabled.PageLoad", GetParam(),
                                      1);
}

// Verify that we correctly log the IsEnabled metrics with signed out sync
// state.
TEST_P(AutofillSettingsMetricsTest,
       LogIsAutofillEnabledAtPageLoadSignedOutSyncState) {
  LogIsAutofillEnabledAtPageLoad(
      /*enabled=*/GetParam(), AutofillMetrics::PaymentsSigninState::kSignedOut);
  histogram_tester_.ExpectBucketCount("Autofill.IsEnabled.PageLoad.SignedOut",
                                      GetParam(), 1);
  // Make sure the metric without the sync state is still recorded.
  histogram_tester_.ExpectBucketCount("Autofill.IsEnabled.PageLoad", GetParam(),
                                      1);
}

// Test that we log that Profile Autofill is enabled / disabled at startup.
TEST_P(AutofillSettingsMetricsTest, AutofillProfileIsEnabledAtStartup) {
  autofill_client_->GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled,
                                           GetParam());
  // The constructor of `AddressDataManager` emits
  // `Autofill.Address.IsEnabled.Startup`. Its instance is created at startup.
  AddressDataManager(/*webdata_service=*/nullptr,
                     /*pref_service=*/autofill_client_->GetPrefs(),
                     /*local_state=*/nullptr,
                     /*sync_service=*/nullptr,
                     /*identity_manager=*/nullptr,
                     /*strike_database=*/nullptr,
                     /*variation_country_code=*/GeoIpCountryCode("US"),
                     "en-US");
  histogram_tester_.ExpectUniqueSample("Autofill.Address.IsEnabled.Startup",
                                       GetParam(), 1);
}

// Test that we log that CreditCard is enabled / disabled at startup.
TEST_P(AutofillSettingsMetricsTest, AutofillCreditCardIsEnabledAtStartup) {
  autofill_client_->GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           GetParam());
  // The constructor of `PaymentsDataManager` emits
  // `Autofill.CreditCard.IsEnabled.Startup`. Its instance is created at
  // startup.
  PaymentsDataManager(/*profile_database=*/nullptr,
                      /*account_database=*/nullptr,
                      /*image_fetcher=*/nullptr,
                      /*shared_storage_handler=*/nullptr,
                      /*pref_service=*/autofill_client_->GetPrefs(),
                      /*sync_service=*/nullptr,
                      /*identity_manager=*/nullptr,
                      /*variations_country_code=*/GeoIpCountryCode("US"),
                      "en-US");
  histogram_tester_.ExpectUniqueSample("Autofill.CreditCard.IsEnabled.Startup",
                                       GetParam(), 1);
}

}  // namespace

}  // namespace autofill::autofill_metrics
