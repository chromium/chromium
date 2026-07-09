// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/common/autofill_features.h"
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
  explicit AutofillSettingsMetricsTest(bool enable_enterprise_policy = false) {
    if (enable_enterprise_policy) {
      feature_list_.InitAndEnableFeature(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kAutofillEnableAutofillSettingsEnterprisePolicy);
    }
  }
  ~AutofillSettingsMetricsTest() override = default;

  void SetUp() override { SetUpHelper(); }

  void TearDown() override { TearDownHelper(); }

  void CreateAddressDataManager() {
    AddressDataManager(/*webdata_service=*/nullptr,
                       /*pref_service=*/autofill_client().GetPrefs(),
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
                        /*pref_service=*/autofill_client().GetPrefs(),
                        /*sync_service=*/nullptr,
                        /*identity_manager=*/nullptr,
                        /*variations_country_code=*/GeoIpCountryCode("US"),
                        "en-US", /*autofill_optimization_guide=*/nullptr);
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(, AutofillSettingsMetricsTest, ::testing::Bool());

class AutofillSettingsMetricsEnterprisePolicyTest
    : public AutofillSettingsMetricsTest {
 public:
  AutofillSettingsMetricsEnterprisePolicyTest()
      : AutofillSettingsMetricsTest(/*enable_enterprise_policy=*/true) {}

 protected:
  void SetPolicyBlocklist(std::string_view url_pattern,
                          std::string_view category_value,
                          AutofillPreferenceSetter setter_type) {
    base::ListValue categories;
    categories.Append(category_value);

    base::DictValue entry;
    entry.Set(prefs::kAutofillBlockedTypesUrlPatternKey, url_pattern);
    entry.Set(prefs::kAutofillBlockedTypesBlockedTypesKey,
              std::move(categories));

    base::ListValue blocked_list;
    blocked_list.Append(std::move(entry));

    if (setter_type == AutofillPreferenceSetter::kAdminPolicy) {
      autofill_client().GetPrefs()->SetManagedPref(
          prefs::kAutofillTypesBlocked, base::Value(std::move(blocked_list)));
    } else if (setter_type == AutofillPreferenceSetter::kExtension) {
      autofill_client().GetPrefs()->SetExtensionPref(
          prefs::kAutofillTypesBlocked, base::Value(std::move(blocked_list)));
    } else if (setter_type == AutofillPreferenceSetter::kCustodian) {
      autofill_client().GetPrefs()->SetSupervisedUserPref(
          prefs::kAutofillTypesBlocked, base::Value(std::move(blocked_list)));
    }
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         AutofillSettingsMetricsEnterprisePolicyTest,
                         ::testing::Bool());

// Test that we log that Profile Autofill is enabled / disabled when filling a
// form.
TEST_P(AutofillSettingsMetricsTest, LogsAutofillProfileIsEnabledAtPageLoad) {
  autofill_client().SetAutofillProfileEnabled(GetParam());
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});
  histogram_tester_.ExpectUniqueSample("Autofill.Address.IsEnabled.PageLoad",
                                       GetParam(), 1);
}

// Test that we log that CreditCard Autofill is enabled / disabled when filling
// a form.
TEST_P(AutofillSettingsMetricsTest, AutofillCreditCardIsEnabledAtPageLoad) {
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(GetParam());
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
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled,
                                           GetParam());

  // The constructor of `AddressDataManager` emits
  // `Autofill.Address.IsEnabled.Startup`. Its instance is created at startup.
  CreateAddressDataManager();

  histogram_tester_.ExpectUniqueSample("Autofill.Address.IsEnabled.Startup",
                                       GetParam(), 1);
}

// Test that we log that CreditCard is enabled / disabled at startup.
TEST_P(AutofillSettingsMetricsTest, AutofillCreditCardIsEnabledAtStartup) {
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
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
  autofill_client().GetPrefs()->SetUserPref(prefs::kAutofillProfileEnabled,
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
  autofill_client().GetPrefs()->SetManagedPref(prefs::kAutofillProfileEnabled,
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
  autofill_client().GetPrefs()->SetExtensionPref(prefs::kAutofillProfileEnabled,
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
  autofill_client().GetPrefs()->SetSupervisedUserPref(
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
  autofill_client().SetAutofillProfileEnabled(GetParam());
  autofill_client().GetPrefs()->SetUserPref(prefs::kAutofillProfileEnabled,
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
  autofill_client().SetAutofillProfileEnabled(GetParam());
  autofill_client().GetPrefs()->SetManagedPref(prefs::kAutofillProfileEnabled,
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
  autofill_client().SetAutofillProfileEnabled(GetParam());
  autofill_client().GetPrefs()->SetExtensionPref(prefs::kAutofillProfileEnabled,
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
  autofill_client().SetAutofillProfileEnabled(GetParam());
  autofill_client().GetPrefs()->SetSupervisedUserPref(
      prefs::kAutofillProfileEnabled, base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kCustodian, !GetParam());
}

// Tests that payment method Autofill disabled by user setting is logged at
// startup.
TEST_P(AutofillSettingsMetricsTest,
       EmitsAutofillPaymentMethodsDisabledByUserAtStartup) {
  autofill_client().GetPrefs()->SetUserPref(prefs::kAutofillCreditCardEnabled,
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
  autofill_client().GetPrefs()->SetManagedPref(
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
  autofill_client().GetPrefs()->SetExtensionPref(
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
  autofill_client().GetPrefs()->SetSupervisedUserPref(
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
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(GetParam());
  autofill_client().GetPrefs()->SetUserPref(prefs::kAutofillCreditCardEnabled,
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
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(GetParam());
  autofill_client().GetPrefs()->SetManagedPref(
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
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(GetParam());
  autofill_client().GetPrefs()->SetExtensionPref(
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
  payments_autofill_client().SetAutofillPaymentMethodsEnabled(GetParam());
  autofill_client().GetPrefs()->SetSupervisedUserPref(
      prefs::kAutofillCreditCardEnabled, base::Value(GetParam()));

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kCustodian, !GetParam());
}

TEST_P(AutofillSettingsMetricsTest,
       EmitsActionAutofillProfileDisabledOnPrefChangeByUser) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client().GetPrefs()->SetUserPref(prefs::kAutofillProfileEnabled,
                                            base::Value(GetParam()));
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled),
            !GetParam());
}

TEST_P(AutofillSettingsMetricsTest,
       EmitsActionAutofillProfileDisabledOnPrefChangeByUserViaSetBoolean) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled,
                                           GetParam());
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled),
            !GetParam());
}

TEST_P(AutofillSettingsMetricsTest,
       EmitsActionAutofillProfileDisabledOnPrefChangeByExtension) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client().GetPrefs()->SetExtensionPref(prefs::kAutofillProfileEnabled,
                                                 base::Value(GetParam()));
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled),
            !GetParam());
}

TEST_P(AutofillSettingsMetricsTest,
       DoesNotEmitActionAutofillProfileDisabledOnPrefChangeByAdminPolicy) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client().GetPrefs()->SetManagedPref(prefs::kAutofillProfileEnabled,
                                               base::Value(GetParam()));
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
}

TEST_P(AutofillSettingsMetricsTest,
       DoesNotEmitActionAutofillProfileDisabledOnPrefChangeByCustodian) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
  autofill_client().GetPrefs()->SetSupervisedUserPref(
      prefs::kAutofillProfileEnabled, base::Value(GetParam()));
  EXPECT_EQ(user_action_tester.GetActionCount(kUserActionProfileDisabled), 0);
}

// Tests that Autofill Profile disabled by the GPO wildcard blocklist policy is
// logged as AdminPolicy at startup.
TEST_P(AutofillSettingsMetricsEnterprisePolicyTest,
       EmitsAutofillProfileDisabledByBlocklistWildcardAtStartup) {
  SetPolicyBlocklist("*", prefs::kAutofillBlockedTypesContactInfoValue,
                     AutofillPreferenceSetter::kAdminPolicy);

  LogAutofillProfileDisabledReasonAtStartup(*autofill_client().GetPrefs());

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.Startup",
      AutofillPreferenceSetter::kAdminPolicy, 1);
}

// Tests that Autofill Profile disabled by the GPO site-specific blocklist
// policy is logged as AdminPolicy at page load.
TEST_P(AutofillSettingsMetricsEnterprisePolicyTest,
       EmitsAutofillProfileDisabledByBlocklistSiteSpecificAtPageLoad) {
  // Override the fixture's GetParam() to ensure the legacy pref is true.
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled,
                                           true);

  SetPolicyBlocklist("example.com",
                     prefs::kAutofillBlockedTypesContactInfoValue,
                     AutofillPreferenceSetter::kAdminPolicy);
  autofill_client().SetAutofillTypeBlockedByPolicy(
      AutofillClient::AutofillPolicyDataCategory::kContactInfo,
      /*blocked=*/true);

  // Simulate page load on the blocked site.
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));

  // Simulate ChromeAutofillClient disabling profiles due to policy blocklist.
  autofill_client().SetAutofillProfileEnabled(false);
  // Restore the pref to true so it doesn't trigger the user pref check.
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillProfileEnabled,
                                           true);
  autofill_client().SetAutofillTypeBlockedByPolicy(
      AutofillClient::AutofillPolicyDataCategory::kContactInfo,
      /*blocked=*/true);

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kAdminPolicy, 1);
}

// Tests that Payment Methods disabled by the GPO wildcard blocklist policy is
// logged as AdminPolicy at startup.
TEST_P(AutofillSettingsMetricsEnterprisePolicyTest,
       EmitsAutofillPaymentMethodsDisabledByBlocklistWildcardAtStartup) {
  SetPolicyBlocklist("*", prefs::kAutofillBlockedTypesPaymentsValue,
                     AutofillPreferenceSetter::kAdminPolicy);

  LogAutofillPaymentMethodsDisabledReasonAtStartup(
      *autofill_client().GetPrefs());

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.Startup",
      AutofillPreferenceSetter::kAdminPolicy, 1);
}

// Tests that Payment Methods disabled by the GPO site-specific blocklist policy
// is logged as AdminPolicy at page load.
TEST_P(AutofillSettingsMetricsEnterprisePolicyTest,
       EmitsAutofillPaymentMethodsDisabledByBlocklistSiteSpecificAtPageLoad) {
  // Override the fixture\'s GetParam() to ensure the legacy pref is true.
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           true);

  SetPolicyBlocklist("example.com", prefs::kAutofillBlockedTypesPaymentsValue,
                     AutofillPreferenceSetter::kAdminPolicy);
  autofill_client().SetAutofillTypeBlockedByPolicy(
      AutofillClient::AutofillPolicyDataCategory::kPayments, /*blocked=*/true);

  // Simulate page load on the blocked site.
  autofill_client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));

  // Simulate ChromeAutofillClient disabling payment methods due to policy
  // blocklist.
  autofill_client()
      .GetPaymentsAutofillClient()
      ->SetAutofillPaymentMethodsEnabled(false);
  // Restore the pref to true so it doesn't trigger the user pref check.
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           true);
  autofill_client().SetAutofillTypeBlockedByPolicy(
      AutofillClient::AutofillPolicyDataCategory::kPayments, /*blocked=*/true);

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{});

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.PageLoad",
      AutofillPreferenceSetter::kAdminPolicy, 1);
}

// Tests that Autofill Profile disabled by the GPO wildcard blocklist policy via
// extension is logged as Extension at startup.
TEST_P(AutofillSettingsMetricsEnterprisePolicyTest,
       EmitsAutofillProfileDisabledByBlocklistWildcardExtensionAtStartup) {
  SetPolicyBlocklist("*", prefs::kAutofillBlockedTypesContactInfoValue,
                     AutofillPreferenceSetter::kExtension);

  LogAutofillProfileDisabledReasonAtStartup(*autofill_client().GetPrefs());

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.Startup",
      AutofillPreferenceSetter::kExtension, 1);
}

// Tests that Autofill Profile disabled by the GPO wildcard blocklist policy via
// custodian is logged as Custodian at startup.
TEST_P(AutofillSettingsMetricsEnterprisePolicyTest,
       EmitsAutofillProfileDisabledByBlocklistWildcardCustodianAtStartup) {
  SetPolicyBlocklist("*", prefs::kAutofillBlockedTypesContactInfoValue,
                     AutofillPreferenceSetter::kCustodian);

  LogAutofillProfileDisabledReasonAtStartup(*autofill_client().GetPrefs());

  histogram_tester_.ExpectUniqueSample(
      "Autofill.Address.DisabledReason.Startup",
      AutofillPreferenceSetter::kCustodian, 1);
}

// Tests that Payment Methods disabled by the GPO wildcard blocklist policy via
// extension is logged as Extension at startup.
TEST_P(
    AutofillSettingsMetricsEnterprisePolicyTest,
    EmitsAutofillPaymentMethodsDisabledByBlocklistWildcardExtensionAtStartup) {
  SetPolicyBlocklist("*", prefs::kAutofillBlockedTypesPaymentsValue,
                     AutofillPreferenceSetter::kExtension);

  LogAutofillPaymentMethodsDisabledReasonAtStartup(
      *autofill_client().GetPrefs());

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.Startup",
      AutofillPreferenceSetter::kExtension, 1);
}

// Tests that Payment Methods disabled by the GPO wildcard blocklist policy via
// custodian is logged as Custodian at startup.
TEST_P(
    AutofillSettingsMetricsEnterprisePolicyTest,
    EmitsAutofillPaymentMethodsDisabledByBlocklistWildcardCustodianAtStartup) {
  SetPolicyBlocklist("*", prefs::kAutofillBlockedTypesPaymentsValue,
                     AutofillPreferenceSetter::kCustodian);

  LogAutofillPaymentMethodsDisabledReasonAtStartup(
      *autofill_client().GetPrefs());

  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.DisabledReason.Startup",
      AutofillPreferenceSetter::kCustodian, 1);
}

}  // namespace

}  // namespace autofill::autofill_metrics
