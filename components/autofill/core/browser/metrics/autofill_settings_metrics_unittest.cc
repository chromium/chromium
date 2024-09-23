// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

namespace {

constexpr std::string_view kUserActionProfileDisabled =
    "Autofill_ProfileDisabled";

class AutofillSettingsMetricsTest : public AutofillMetricsBaseTest,
                                    public testing::TestWithParam<bool> {
 public:
  ~AutofillSettingsMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

  void CreateAddressDataManager() {
    AddressDataManager(/*webdata_service=*/nullptr,
                       /*pref_service=*/autofill_client_->GetPrefs(),
                       /*local_state=*/nullptr,
                       /*sync_service=*/nullptr,
                       /*identity_manager=*/nullptr,
                       /*strike_database=*/nullptr,
                       /*variation_country_code=*/GeoIpCountryCode("US"),
                       "en-US");
  }

  void CreatePaymentsDataManager() {
    PaymentsDataManager(/*profile_database=*/nullptr,
                        /*account_database=*/nullptr,
                        /*image_fetcher=*/nullptr,
                        /*shared_storage_handler=*/nullptr,
                        /*pref_service=*/autofill_client_->GetPrefs(),
                        /*sync_service=*/nullptr,
                        /*identity_manager=*/nullptr,
                        /*variations_country_code=*/GeoIpCountryCode("US"),
                        "en-US");
  }

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
  CreateAddressDataManager();

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
  CreatePaymentsDataManager();

  histogram_tester_.ExpectUniqueSample("Autofill.CreditCard.IsEnabled.Startup",
                                       GetParam(), 1);
}

// Tests that Autofill Profile disabled by user setting is logged at startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByUserAtStartup) {
  autofill_client_->GetPrefs()->SetUserPref(prefs::kAutofillProfileEnabled,
                                            base::Value(GetParam()));

  // The constructor of `AddressDataManager` emits
  // `Autofill.Address.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreateAddressDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.Startup",
      AutofillPreferenceSetter::kUserSetting, !GetParam());
}

// Tests that Autofill Profile disabled by admin policy is logged at startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByAdminPolicyAtStartup) {
  autofill_client_->GetPrefs()->SetManagedPref(prefs::kAutofillProfileEnabled,
                                               base::Value(GetParam()));

  // The constructor of `AddressDataManager` emits
  // `Autofill.Address.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreateAddressDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.Startup",
      AutofillPreferenceSetter::kAdminPolicy, !GetParam());
}

// Tests that Autofill Profile disabled by extension is logged at startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByExtensionAtStartup) {
  autofill_client_->GetPrefs()->SetExtensionPref(prefs::kAutofillProfileEnabled,
                                                 base::Value(GetParam()));

  // The constructor of `AddressDataManager` emits
  // `Autofill.Address.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreateAddressDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.Startup",
      AutofillPreferenceSetter::kExtension, !GetParam());
}

// Tests that Autofill Profile disabled by custodian is logged at startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByCustodianAtStartup) {
  autofill_client_->GetPrefs()->SetSupervisedUserPref(
      prefs::kAutofillProfileEnabled, base::Value(GetParam()));

  // The constructor of `AddressDataManager` emits
  // `Autofill.Address.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreateAddressDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.Startup",
      AutofillPreferenceSetter::kCustodian, !GetParam());
}

// Tests that Autofill Profile disabled by user setting is logged at page load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByUserAtPageLoad) {
  autofill_manager().SetAutofillProfileEnabled(*autofill_client_, GetParam());
  autofill_client_->GetPrefs()->SetUserPref(prefs::kAutofillProfileEnabled,
                                            base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kUserSetting, !GetParam());
}

// Tests that Autofill Profile disabled by admin policy is logged at page load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByAdminPolicyAtPageLoad) {
  autofill_manager().SetAutofillProfileEnabled(*autofill_client_, GetParam());
  autofill_client_->GetPrefs()->SetManagedPref(prefs::kAutofillProfileEnabled,
                                               base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kAdminPolicy, !GetParam());
}

// Tests that Autofill Profile disabled by extension is logged at page load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByExtensionAtPageLoad) {
  autofill_manager().SetAutofillProfileEnabled(*autofill_client_, GetParam());
  autofill_client_->GetPrefs()->SetExtensionPref(prefs::kAutofillProfileEnabled,
                                                 base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kExtension, !GetParam());
}

// Tests that Autofill Profile disabled by custodian is logged at page load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByCustodianAtPageLoad) {
  autofill_manager().SetAutofillProfileEnabled(*autofill_client_, GetParam());
  autofill_client_->GetPrefs()->SetSupervisedUserPref(
      prefs::kAutofillProfileEnabled, base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kCustodian, !GetParam());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// [ChromeOS-only] Tests that Autofill Profile disabled by standalone browser is
// logged at startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByStandaloneBrowserAtStartup) {
  autofill_client_->GetPrefs()->SetStandaloneBrowserPref(
      prefs::kAutofillProfileEnabled, base::Value(GetParam()));

  // The constructor of `AddressDataManager` emits
  // `Autofill.Address.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreateAddressDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.Startup",
      AutofillPreferenceSetter::kStandaloneBrowser, !GetParam());
}

// [ChromeOS-only] Tests that Autofill Profile disabled by standalone browser is
// logged at page load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillProfileDisabledByStandaloneBrowserAtPageLoad) {
  autofill_manager().SetAutofillProfileEnabled(*autofill_client_, GetParam());
  autofill_client_->GetPrefs()->SetStandaloneBrowserPref(
      prefs::kAutofillProfileEnabled, base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kStandaloneBrowser, !GetParam());
}

#endif

// Tests that payment method Autofill disabled by user setting is logged at
// startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByUserAtStartup) {
  autofill_client_->GetPrefs()->SetUserPref(prefs::kAutofillCreditCardEnabled,
                                            base::Value(GetParam()));

  // The constructor of `PaymentsDataManager` emits
  // `Autofill.CreditCard.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreatePaymentsDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.Startup",
      AutofillPreferenceSetter::kUserSetting, !GetParam());
}

// Tests that payment method Autofill disabled by admin policy is logged at
// startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByAdminPolicyAtStartup) {
  autofill_client_->GetPrefs()->SetManagedPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  // The constructor of `PaymentsDataManager` emits
  // `Autofill.CreditCard.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreatePaymentsDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.Startup",
      AutofillPreferenceSetter::kAdminPolicy, !GetParam());
}

// Tests that payment method Autofill disabled by extension is logged at
// startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByExtensionAtStartup) {
  autofill_client_->GetPrefs()->SetExtensionPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  // The constructor of `PaymentsDataManager` emits
  // `Autofill.CreditCard.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreatePaymentsDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.Startup",
      AutofillPreferenceSetter::kExtension, !GetParam());
}

// Tests that payment method Autofill disabled by custodian is logged at
// startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByCustodianAtStartup) {
  autofill_client_->GetPrefs()->SetSupervisedUserPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  // The constructor of `PaymentsDataManager` emits
  // `Autofill.CreditCard.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreatePaymentsDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.Startup",
      AutofillPreferenceSetter::kCustodian, !GetParam());
}

// Tests that payment method Autofill disabled by user setting is logged at page
// load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByUserAtPageLoad) {
  autofill_manager().SetAutofillPaymentMethodsEnabled(*autofill_client_,
                                                      GetParam());
  autofill_client_->GetPrefs()->SetUserPref(prefs::kAutofillCreditCardEnabled,
                                            base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kUserSetting, !GetParam());
}

// Tests that payment method Autofill disabled by admin policy is logged at page
// load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByAdminPolicyAtPageLoad) {
  autofill_manager().SetAutofillPaymentMethodsEnabled(*autofill_client_,
                                                      GetParam());
  autofill_client_->GetPrefs()->SetManagedPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kAdminPolicy, !GetParam());
}

// Tests that payment method Autofill disabled by extension is logged at page
// load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByExtensionAtPageLoad) {
  autofill_manager().SetAutofillPaymentMethodsEnabled(*autofill_client_,
                                                      GetParam());
  autofill_client_->GetPrefs()->SetExtensionPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kExtension, !GetParam());
}

// Tests that payment method Autofill disabled by custodian is logged at page
// load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByCustodianAtPageLoad) {
  autofill_manager().SetAutofillPaymentMethodsEnabled(*autofill_client_,
                                                      GetParam());
  autofill_client_->GetPrefs()->SetSupervisedUserPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kCustodian, !GetParam());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// [ChromeOS-only] Tests that payment method Autofill disabled by standalone
// browser is logged at startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByStandaloneBrowserAtStartup) {
  autofill_client_->GetPrefs()->SetStandaloneBrowserPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  // The constructor of `PaymentsDataManager` emits
  // `Autofill.CreditCard.DisabledReason.Startup`. Its instance is created at
  // startup.
  CreatePaymentsDataManager();

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.Startup",
      AutofillPreferenceSetter::kStandaloneBrowser, !GetParam());
}

// [ChromeOS-only] Tests that payment method Autofill disabled by standalone
// browser is logged at page load.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByStandaloneBrowserAtPageLoad) {
  autofill_manager().SetAutofillPaymentMethodsEnabled(*autofill_client_,
                                                      GetParam());
  autofill_client_->GetPrefs()->SetStandaloneBrowserPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kStandaloneBrowser, !GetParam());
}

#endif

TEST_P(AutofillSettingsMetricsTest,
       EmitsActionAutofillProfileDisabledOnPrefChangeByUser) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client_->GetPrefs()->SetUserPref(prefs::kAutofillProfileEnabled,
                                            base::Value(GetParam()));
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled),
            !GetParam());
}

TEST_P(AutofillSettingsMetricsTest,
       EmitsActionAutofillProfileDisabledOnPrefChangeByUserViaSetBoolean) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client_->GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled,
                                           GetParam());
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled),
            !GetParam());
}

TEST_P(AutofillSettingsMetricsTest,
       EmitsActionAutofillProfileDisabledOnPrefChangeByExtension) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client_->GetPrefs()->SetExtensionPref(prefs::kAutofillProfileEnabled,
                                                 base::Value(GetParam()));
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled),
            !GetParam());
}

TEST_P(AutofillSettingsMetricsTest,
       DoesNotEmitActionAutofillProfileDisabledOnPrefChangeByAdminPolicy) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client_->GetPrefs()->SetManagedPref(prefs::kAutofillProfileEnabled,
                                               base::Value(GetParam()));
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
}

TEST_P(AutofillSettingsMetricsTest,
       DoesNotEmitActionAutofillProfileDisabledOnPrefChangeByCustodian) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client_->GetPrefs()->SetSupervisedUserPref(
      prefs::kAutofillProfileEnabled, base::Value(GetParam()));
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
}

}  // namespace

}  // namespace autofill::autofill_metrics
