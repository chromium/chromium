// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/functional/callback_helpers.h"
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
    pref_service_.registry()->RegisterBooleanPref(prefs::kAutofillHasSeenIban,
                                                  false);
    signin::IdentityManager::RegisterProfilePrefs(pref_service_.registry());
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
  TestingPrefServiceSimple pref_service_;
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
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledByFlag, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledByFlag, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_UnsupportedCountry) {
  // "ZZ" is NOT one of the countries in |kAutofillUpstreamLaunchedCountries|.
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "ZZ",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kUnsupportedCountry, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
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
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_AuthError) {
  sync_service_.SetPersistentAuthError();
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSyncPaused));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kSyncServicePaused, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SyncPaused",
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
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
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
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillSelectedType,
      1);
}

// Tests that for transport mode users, when CONTACT_INFO is available, credit
// card upload is offered only when kAutofill (address autofill + autocomplete)
// is among the UserSelectableTypes.
TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_TransportWithAddresses_AutofillSelected) {
  base::test::ScopedFeatureList feature{
      syncer::kSyncEnableContactInfoDataTypeInTransportMode};
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
  base::test::ScopedFeatureList features{
      syncer::kSyncEnableContactInfoDataTypeInTransportMode};
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
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillSelectedType,
      1);
}
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(
    AutofillExperimentsTest,
    IsCardUploadEnabled_TransportWithAddresses_AutofillDisabled_DiceMigration) {
  base::test::ScopedFeatureList feature{
      syncer::kSyncEnableContactInfoDataTypeInTransportMode};
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

// Tests that for transport mode users, when CONTACT_INFO is unavailable, credit
// card upload is offered independently of the kAutofill (address autofill +
// autocomplete) UserSelectableType.
TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_TransportWithoutAddresses_AutofillSelected) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(
      syncer::kSyncEnableContactInfoDataTypeInTransportMode);
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
       IsCardUploadEnabled_TransportWithoutAddresses_AutofillDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      syncer::kSyncEnableContactInfoDataTypeInTransportMode);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPayments});
  EXPECT_TRUE(
      IsCreditCardUploadEnabled(AutofillMetrics::PaymentsSigninState::
                                    kSignedInAndWalletSyncTransportEnabled));
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_SyncServiceUsingExplicitPassphrase) {
  sync_service_.SetIsUsingExplicitPassphrase(true);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kUsingExplicitSyncPassphrase, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kUsingExplicitSyncPassphrase, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_PaymentsTypeNotSelected) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
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
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
  histogram_tester_.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndWalletSyncTransportEnabled",
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

}  // namespace autofill
