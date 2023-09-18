// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

using device_reauth::MockDeviceAuthenticator;
using testing::Return;

namespace autofill {

class AutofillExperimentsTest : public testing::Test {
 public:
  AutofillExperimentsTest() {}

 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(prefs::kAutofillHasSeenIban,
                                                  false);
    log_manager_ = LogManager::Create(nullptr, base::NullCallback());
    mock_device_authenticator_ = std::make_unique<MockDeviceAuthenticator>();
  }

  bool IsCreditCardUploadEnabled(
      const AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
    return IsCreditCardUploadEnabled("john.smith@gmail.com",
                                     signin_state_for_metrics);
  }

  bool IsCreditCardUploadEnabled(
      const std::string& user_email,
      const AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
    return IsCreditCardUploadEnabled(user_email, "US",
                                     signin_state_for_metrics);
  }

  bool IsCreditCardUploadEnabled(
      const std::string& user_email,
      const std::string& user_country,
      const AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
    return autofill::IsCreditCardUploadEnabled(
        &sync_service_, user_email, user_country, signin_state_for_metrics,
        log_manager_.get());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
  base::HistogramTester histogram_tester;
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
      "john.smith@gmail.com", "ZZ",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledByFlag, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledByFlag, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_UnsupportedCountry) {
  // "ZZ" is NOT one of the countries in |kAutofillUpstreamLaunchedCountries|.
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "john.smith@gmail.com", "ZZ",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kUnsupportedCountry, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kUnsupportedCountry, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_SupportedCountry) {
  // Show that a disabled or missing feature flag no longer impedes feature
  // availability. The flag is used only to launch to new countries.
  scoped_feature_list_.InitAndDisableFeature(features::kAutofillUpstream);
  // "US" is one of the countries in |kAutofillUpstreamLaunchedCountries|.
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "john.smith@gmail.com", "US",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_AuthError) {
  sync_service_.SetPersistentAuthError();
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSyncPaused));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kSyncServicePaused, 1);
  histogram_tester.ExpectUniqueSample(
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
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_SyncDoesNotHaveAutofillProfileActiveType) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});
  sync_service_.SetFailedDataTypes({syncer::AUTOFILL_PROFILE});
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillProfileActiveType,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillProfileActiveType,
      1);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_SyncServiceUsingExplicitPassphrase) {
  sync_service_.SetIsUsingExplicitPassphrase(true);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kUsingExplicitSyncPassphrase, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kUsingExplicitSyncPassphrase, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_PaymentsTypeNotSelected) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_EmptyUserEmail) {
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEmailEmpty, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEmailEmpty, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_TransportModeOnly) {
  // When we don't have Sync consent, Sync will start in Transport-only mode
  // (if allowed).
  sync_service_.SetHasSyncConsent(false);

  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "john.smith@gmail.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
}

TEST_F(
    AutofillExperimentsTest,
    IsCardUploadEnabled_TransportSyncDoesNotHaveAutofillProfileActiveDataType) {
  // When we don't have Sync consent, Sync will start in Transport-only mode
  // (if allowed).
  sync_service_.SetHasSyncConsent(false);

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});
  sync_service_.SetFailedDataTypes({syncer::AUTOFILL_PROFILE});

  EXPECT_TRUE(IsCreditCardUploadEnabled(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 1);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_UserEmailWithGoogleDomain_IsAllowed) {
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "john.smith@gmail.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "googler@google.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "old.school@googlemail.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "code.committer@chromium.org",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 4);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 4);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_UserEmailWithSupportedAdditionalDomain_IsAllowed) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamAllowAdditionalEmailDomains);
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "cool.user@hotmail.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "cool.british.user@hotmail.co.uk",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "telecom.user@verizon.net",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "youve.got.mail@aol.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 4);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 4);
}

TEST_F(
    AutofillExperimentsTest,
    IsCardUploadEnabled_UserEmailWithSupportedAdditionalDomain_NotAllowedIfFlagOff) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          features::kAutofillUpstreamAllowAdditionalEmailDomains,
          features::kAutofillUpstreamAllowAllEmailDomains});
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "cool.user@hotmail.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "cool.british.user@hotmail.co.uk",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "telecom.user@verizon.net",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "youve.got.mail@aol.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEmailDomainNotSupported, 4);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEmailDomainNotSupported, 4);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_UserEmailWithOtherDomain_IsAllowed) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamAllowAllEmailDomains);
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "grad.student@university.edu",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "some.ceo@bigcorporation.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "fake.googler@google.net",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "fake.committer@chromium.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 4);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEnabledForCountry, 4);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_UserEmailWithOtherDomain_NotAllowedIfFlagOff) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::
                                kAutofillUpstreamAllowAdditionalEmailDomains},
      /*disabled_features=*/{features::kAutofillUpstreamAllowAllEmailDomains});
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "grad.student@university.edu",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "some.ceo@bigcorporation.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "fake.googler@google.net",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "fake.committer@chromium.com",
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      autofill_metrics::CardUploadEnabled::kEmailDomainNotSupported, 4);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::CardUploadEnabled::kEmailDomainNotSupported, 4);
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
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillByDefault(Return(true));

  EXPECT_TRUE(IsDeviceAuthAvailable(mock_device_authenticator_.get()));
#else
  EXPECT_FALSE(IsDeviceAuthAvailable(mock_device_authenticator_.get()));
#endif
}

TEST_F(
    AutofillExperimentsTest,
    IsDeviceAuthAvailable_FeatureDisabledAndAuthenticationAvailableForMacAndWin) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillByDefault(Return(true));
#endif
  EXPECT_FALSE(IsDeviceAuthAvailable(mock_device_authenticator_.get()));
}

}  // namespace autofill
