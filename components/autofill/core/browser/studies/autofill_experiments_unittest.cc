// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/studies/autofill_experiments.h"

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

using device_reauth::MockDeviceAuthenticator;
using testing::Return;

namespace autofill {

class AutofillExperimentsTest : public testing::Test {
 public:
  AutofillExperimentsTest() = default;

 protected:
  void SetUp() override {
    signin::IdentityManager::RegisterProfilePrefs(pref_service_.registry());
    prefs::RegisterProfilePrefs(pref_service_.registry());
    log_manager_ = LogManager::Create(nullptr, base::NullCallback());
    mock_device_authenticator_ = std::make_unique<MockDeviceAuthenticator>();
  }

  bool IsCreditCardUploadEnabled(
      const AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
    return IsCreditCardUploadEnabled("US", signin_state_for_metrics);
  }

  bool IsCreditCardUploadEnabled(
      const std::string& user_country,
      const AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
    return autofill::IsCreditCardUploadEnabled(
        &sync_service_, pref_service_, user_country, signin_state_for_metrics,
        log_manager_.get());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  syncer::TestSyncService sync_service_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<LogManager> log_manager_;
  std::unique_ptr<device_reauth::MockDeviceAuthenticator>
      mock_device_authenticator_;
};

// Testing each scenario, followed by logging the metrics for various
// success and failure scenario of IsCreditCardUploadEnabled(). Every scenario
// should also be associated with logging of a metric so it's easy to analyze
// the results.
TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_FeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  // Use an unsupported country to show that feature flag enablement overrides
  // the client-side country check.
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "ZZ",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::kEnabledByFlag, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledByFlag, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_UnsupportedCountry) {
  // "ZZ" is NOT one of the countries in |kAutofillUpstreamLaunchedCountries|.
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "ZZ",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::kUnsupportedCountry, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kUnsupportedCountry, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_SupportedCountry) {
  // Show that a disabled or missing feature flag no longer impedes feature
  // availability. The flag is used only to launch to new countries.
  scoped_feature_list_.InitAndDisableFeature(features::kAutofillUpstream);
  // "US" is one of the countries in |kAutofillUpstreamLaunchedCountries|.
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "US",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_AuthError) {
  sync_service_.SetPersistentAuthError();
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSyncPaused));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::kSyncServicePaused, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2.SyncPaused",
      autofill_metrics::CardUploadEnabled::kSyncServicePaused, 1);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_SyncDoesNotHaveAutofillWalletDataActiveType) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
}

TEST_F(AutofillExperimentsTest,
       CreditCardSyncDisabled_LogsReasonForBeingDisabled_UserNotSignedIn) {
  sync_service_.SetSignedOut();
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.SyncDisabledReason",
      autofill_metrics::SyncDisabledReason::kNotSignedIn, 1);
}

TEST_F(AutofillExperimentsTest,
       CreditCardSyncDisabled_LogsReasonForBeingDisabled_SyncDisabledByPolicy) {
  sync_service_.SetAllowedByEnterprisePolicy(false);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.SyncDisabledReason",
      autofill_metrics::SyncDisabledReason::kSyncDisabledByPolicy, 1);
}

TEST_F(AutofillExperimentsTest,
       CreditCardSyncDisabled_LogsReasonForBeingDisabled_TypeDisabledByPolicy) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  sync_service_.GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kPayments, true);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.SyncDisabledReason",
      autofill_metrics::SyncDisabledReason::kTypeDisabledByPolicy, 1);
}

TEST_F(
    AutofillExperimentsTest,
    CreditCardSyncDisabled_LogsReasonForBeingDisabled_TypeDisabledByCustodian) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  sync_service_.GetUserSettings()->SetTypeIsManagedByCustodian(
      syncer::UserSelectableType::kPayments, true);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.SyncDisabledReason",
      autofill_metrics::SyncDisabledReason::kTypeDisabledByCustodian, 1);
}

TEST_F(AutofillExperimentsTest,
       CreditCardSyncDisabled_LogsReasonForBeingDisabled_TypeDisabledByUser) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CreditCard.SyncDisabledReason",
      autofill_metrics::SyncDisabledReason::kTypeProbablyDisabledByUser, 1);
}

// Tests that for syncing users, credit card upload is offered only when
// kAutofill (address autofill + autocomplete) is among the UserSelectableTypes.
TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_Syncing_AutofillSelected) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
}
TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_Syncing_AutofillDisabled) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPayments});
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillSelectedType,
      1);
}

// Tests that for transport mode users, when CONTACT_INFO is available, credit
// card upload is offered only when kAutofill (address autofill + autocomplete)
// is among the UserSelectableTypes.
TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_TransportWithAddresses_AutofillSelected) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Migrate Dice users.
  pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin, true);
#endif
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});
  EXPECT_TRUE(
      IsCreditCardUploadEnabled(AutofillMetrics::PaymentsSigninState::
                                    kSignedInAndWalletSyncTransportEnabled));
}
TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_TransportWithAddresses_AutofillDisabled) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Migrate Dice users.
  pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin, true);
#endif
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPayments});
  EXPECT_FALSE(
      IsCreditCardUploadEnabled(AutofillMetrics::PaymentsSigninState::
                                    kSignedInAndWalletSyncTransportEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillSelectedType,
      1);
}
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(
    AutofillExperimentsTest,
    IsCardUploadEnabled_TransportWithAddresses_AutofillDisabled_DiceMigration) {
  // Dice user not migrated to explicit signin.
  ASSERT_FALSE(pref_service_.GetBoolean(::prefs::kExplicitBrowserSignin));
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPayments});
  EXPECT_TRUE(
      IsCreditCardUploadEnabled(AutofillMetrics::PaymentsSigninState::
                                    kSignedInAndWalletSyncTransportEnabled));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_SyncServiceUsingExplicitPassphrase) {
  sync_service_.SetIsUsingExplicitPassphrase(true);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::kUsingExplicitSyncPassphrase, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kUsingExplicitSyncPassphrase, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_PaymentsTypeNotSelected) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_TransportModeOnly) {
  // When we don't have Sync consent, Sync will start in Transport-only mode
  // (if allowed).
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_TRUE(
      IsCreditCardUploadEnabled(AutofillMetrics::PaymentsSigninState::
                                    kSignedInAndWalletSyncTransportEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled2.SignedInAndWalletSyncTransportEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
}

TEST_F(AutofillExperimentsTest, ShouldShowIbanOnSettingsPage_FeatureEnabled) {
  // Use a supported country to verify the feature is enabled.
  EXPECT_TRUE(ShouldShowIbanOnSettingsPage("AE", &pref_service_));

  // Use an unsupported country to verify the feature is disabled.
  EXPECT_FALSE(ShouldShowIbanOnSettingsPage("US", &pref_service_));

  // Use an unsupported country to verify the feature is enabled if
  // kAutofillHasSeenIban in pref is set to true.
  prefs::SetAutofillHasSeenIban(&pref_service_);
  EXPECT_TRUE(ShouldShowIbanOnSettingsPage("US", &pref_service_));
}

TEST_F(
    AutofillExperimentsTest,
    IsDeviceAuthAvailable_FeatureEnabledAndAuthenticationAvailableForMacAndWin) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillByDefault(Return(true));

  EXPECT_TRUE(IsDeviceAuthAvailable(mock_device_authenticator_.get()));
#else
  EXPECT_FALSE(IsDeviceAuthAvailable(mock_device_authenticator_.get()));
#endif
}

// Tests that setting and getting the AutofillSyncTransportOptIn works as
// expected.
// On mobile, no dedicated opt-in is required for WalletSyncTransport - the
// user is always considered opted-in and thus this test doesn't make sense.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillExperimentsTest, WalletSyncTransportPref_GetAndSet) {
  ASSERT_FALSE(pref_service_.GetBoolean(::prefs::kExplicitBrowserSignin));
  const CoreAccountId account1 = CoreAccountId::FromGaiaId(GaiaId("account1"));
  const CoreAccountId account2 = CoreAccountId::FromGaiaId(GaiaId("account2"));

  // There should be no opt-in recorded at first.
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(&pref_service_, account1));
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(&pref_service_, account2));
  // There should be no entry for the accounts in the dictionary.
  EXPECT_TRUE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(&pref_service_, account1, true);
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(&pref_service_, account1));
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(&pref_service_, account2));
  // There should only be one entry in the dictionary.
  EXPECT_EQ(1U,
            pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).size());

  // Unset the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(&pref_service_, account1, false);
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(&pref_service_, account1));
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(&pref_service_, account2));
  // There should be no entry for the accounts in the dictionary.
  EXPECT_TRUE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the second account.
  SetUserOptedInWalletSyncTransport(&pref_service_, account2, true);
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(&pref_service_, account1));
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(&pref_service_, account2));
  // There should only be one entry in the dictionary.
  EXPECT_EQ(1U,
            pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).size());

  // Set the opt-in for the first account too.
  SetUserOptedInWalletSyncTransport(&pref_service_, account1, true);
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(&pref_service_, account1));
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(&pref_service_, account1));
  // There should be tow entries in the dictionary.
  EXPECT_EQ(2U,
            pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).size());
}

TEST_F(AutofillExperimentsTest, WalletSyncTransportPrefExplicitSignin) {
  ASSERT_FALSE(pref_service_.GetBoolean(::prefs::kExplicitBrowserSignin));

  const CoreAccountId account1 = CoreAccountId::FromGaiaId(GaiaId("account1"));
  // There should be no opt-in recorded at first.
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(&pref_service_, account1));

  // Explicit browser signin opts the user in.
  pref_service_.SetBoolean(::prefs::kExplicitBrowserSignin, true);
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(&pref_service_, account1));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests that AutofillSyncTransportOptIn is not stored using the plain text
// account id.
TEST_F(AutofillExperimentsTest, WalletSyncTransportPref_UsesHashAccountId) {
  ASSERT_FALSE(pref_service_.GetBoolean(::prefs::kExplicitBrowserSignin));

  const CoreAccountId account1 = CoreAccountId::FromGaiaId(GaiaId("account1"));

  // There should be no opt-in recorded at first.
  EXPECT_TRUE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(&pref_service_, account1, true);
  EXPECT_FALSE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Make sure that the dictionary keys don't contain the account id.
  const auto& dictionary =
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn);
  EXPECT_EQ(std::nullopt, dictionary.FindInt(account1.ToString()));
}

// Tests that clearing the AutofillSyncTransportOptIn works as expected.
TEST_F(AutofillExperimentsTest, WalletSyncTransportPref_Clear) {
  ASSERT_FALSE(pref_service_.GetBoolean(::prefs::kExplicitBrowserSignin));

  const CoreAccountId account1 = CoreAccountId::FromGaiaId(GaiaId("account1"));
  const CoreAccountId account2 = CoreAccountId::FromGaiaId(GaiaId("account2"));

  // There should be no opt-in recorded at first.
  EXPECT_TRUE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(&pref_service_, account1, true);
  EXPECT_FALSE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the second account.
  SetUserOptedInWalletSyncTransport(&pref_service_, account2, true);
  EXPECT_FALSE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Clear all opt-ins. The dictionary should be empty.
  prefs::ClearSyncTransportOptIns(&pref_service_);
  EXPECT_TRUE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());
}

// Tests that the account id hash that we generate can be written and read from
// JSON properly.
TEST_F(AutofillExperimentsTest,
       WalletSyncTransportPref_CanBeSetAndReadFromJSON) {
  ASSERT_FALSE(pref_service_.GetBoolean(::prefs::kExplicitBrowserSignin));

  const CoreAccountId account1 = CoreAccountId::FromGaiaId(GaiaId("account1"));

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(&pref_service_, account1, true);
  EXPECT_FALSE(
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  const base::Value::Dict& dictionary =
      pref_service_.GetDict(prefs::kAutofillSyncTransportOptIn);

  std::string output_js;
  ASSERT_TRUE(base::JSONWriter::Write(dictionary, &output_js));
  EXPECT_EQ(dictionary, *base::JSONReader::Read(
                            output_js, base::JSON_PARSE_CHROMIUM_EXTENSIONS));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AutofillExperimentsTest,
       FacilitatedPaymentsPixPref_DefaultValueSetToTrue) {
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kFacilitatedPaymentsPix));
}

TEST_F(AutofillExperimentsTest,
       FacilitatedPaymentsPixAccountLinkingPref_DefaultValueSetToTrue) {
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kFacilitatedPaymentsPixAccountLinking));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill
