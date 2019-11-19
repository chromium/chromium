// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_experiments.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AutofillExperimentsTest : public testing::Test {
 public:
  AutofillExperimentsTest() {}

 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAutofillWalletImportEnabled, true);
    log_manager_ = LogManager::Create(nullptr, base::Closure());
  }

  bool IsCreditCardUploadEnabled(const AutofillSyncSigninState sync_state) {
    return IsCreditCardUploadEnabled("john.smith@gmail.com", sync_state);
  }

  bool IsCreditCardUploadEnabled(const std::string& user_email,
                                 const AutofillSyncSigninState sync_state) {
    return autofill::IsCreditCardUploadEnabled(&pref_service_, &sync_service_,
                                               user_email, sync_state,
                                               log_manager_.get());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  syncer::TestSyncService sync_service_;
  base::HistogramTester histogram_tester;
  std::unique_ptr<LogManager> log_manager_;
};

// Testing each scenario, followed by logging the metrics for various
// success and failure scenario of IsCreditCardUploadEnabled(). Every scenario
// should also be associated with logging of a metric so it's easy to analyze
// the results.
TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_FeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kAutofillUpstream);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::AUTOFILL_UPSTREAM_DISABLED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::AUTOFILL_UPSTREAM_DISABLED, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_AuthError) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  sync_service_.SetAuthError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(IsCreditCardUploadEnabled(AutofillSyncSigninState::kSyncPaused));
  histogram_tester.ExpectUniqueSample("Autofill.CardUploadEnabled",
                                      AutofillMetrics::CardUploadEnabledMetric::
                                          SYNC_SERVICE_PERSISTENT_AUTH_ERROR,
                                      1);
  histogram_tester.ExpectUniqueSample("Autofill.CardUploadEnabled.SyncPaused",
                                      AutofillMetrics::CardUploadEnabledMetric::
                                          SYNC_SERVICE_PERSISTENT_AUTH_ERROR,
                                      1);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_SyncDoesNotHaveAutofillWalletDataActiveType) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  sync_service_.SetActiveDataTypes(syncer::ModelTypeSet());
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::
          SYNC_SERVICE_MISSING_AUTOFILL_WALLET_DATA_ACTIVE_TYPE,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::
          SYNC_SERVICE_MISSING_AUTOFILL_WALLET_DATA_ACTIVE_TYPE,
      1);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_SyncDoesNotHaveAutofillProfileActiveType) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  sync_service_.SetActiveDataTypes(
      syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::
          SYNC_SERVICE_MISSING_AUTOFILL_PROFILE_ACTIVE_TYPE,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::
          SYNC_SERVICE_MISSING_AUTOFILL_PROFILE_ACTIVE_TYPE,
      1);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_SyncServiceUsingSecondaryPassphrase) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  sync_service_.SetIsUsingSecondaryPassphrase(true);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::USING_SECONDARY_SYNC_PASSPHRASE,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::USING_SECONDARY_SYNC_PASSPHRASE,
      1);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_AutofillWalletImportEnabledPrefIsDisabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  prefs::SetPaymentsIntegrationEnabled(&pref_service_, false);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::PAYMENTS_INTEGRATION_DISABLED,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::PAYMENTS_INTEGRATION_DISABLED,
      1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_EmptyUserEmail) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "", AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::EMAIL_EMPTY, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::EMAIL_EMPTY, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_TransportModeOnly) {
  scoped_feature_list_.InitWithFeatures(
      /*enable_features=*/{features::kAutofillUpstream,
                           features::kAutofillEnableAccountWalletStorage},
      /*disable_features=*/{});
  // When we have no primary account, Sync will start in Transport-only mode
  // (if allowed).
  sync_service_.SetIsAuthenticatedAccountPrimary(false);

  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "john.smith@gmail.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 1);
}

TEST_F(
    AutofillExperimentsTest,
    IsCardUploadEnabled_TransportSyncDoesNotHaveAutofillProfileActiveDataType) {
  scoped_feature_list_.InitWithFeatures(
      /*enable_features=*/{features::kAutofillUpstream,
                           features::kAutofillEnableAccountWalletStorage},
      /*disable_features=*/{});
  // When we have no primary account, Sync will start in Transport-only mode
  // (if allowed).
  sync_service_.SetIsAuthenticatedAccountPrimary(false);

  // Update the active types to only include Wallet. This disables all other
  // types, including profiles.
  sync_service_.SetActiveDataTypes(
      syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));

  EXPECT_TRUE(IsCreditCardUploadEnabled(
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 1);
}

TEST_F(AutofillExperimentsTest, IsCardUploadEnabled_UserEmailWithGoogleDomain) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "john.smith@gmail.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "googler@google.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "old.school@googlemail.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "code.committer@chromium.org",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 4);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 4);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_UserEmailWithNonGoogleDomain) {
  scoped_feature_list_.InitAndEnableFeature(features::kAutofillUpstream);
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "cool.user@hotmail.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "john.smith@johnsmith.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "fake.googler@google.net",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_FALSE(IsCreditCardUploadEnabled(
      "fake.committer@chromium.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::EMAIL_DOMAIN_NOT_SUPPORTED, 4);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::EMAIL_DOMAIN_NOT_SUPPORTED, 4);
}

TEST_F(AutofillExperimentsTest,
       IsCardUploadEnabled_UserEmailWithNonGoogleDomainIfExperimentEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillUpstream,
       features::kAutofillUpstreamAllowAllEmailDomains},
      {});
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "cool.user@hotmail.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "john.smith@johnsmith.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "fake.googler@google.net",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  EXPECT_TRUE(IsCreditCardUploadEnabled(
      "fake.committer@chromium.com",
      AutofillSyncSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 4);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CardUploadEnabled.SignedInAndSyncFeatureEnabled",
      AutofillMetrics::CardUploadEnabledMetric::CARD_UPLOAD_ENABLED, 4);
}

}  // namespace autofill
