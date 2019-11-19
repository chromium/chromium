// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_onboarding.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;

namespace password_manager {

using OnboardingState = metrics_util::OnboardingState;

class PasswordManagerOnboardingTest : public testing::Test {
 public:
  PasswordManagerOnboardingTest() = default;

  void SetUp() override {
    store_ = new TestPasswordStore;
    store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr);

    prefs_.reset(new TestingPrefServiceSimple());
    prefs_->registry()->RegisterIntegerPref(
        prefs::kPasswordManagerOnboardingState,
        static_cast<int>(OnboardingState::kDoNotShow));
    prefs_->registry()->RegisterBooleanPref(
        prefs::kWasOnboardingFeatureCheckedBefore, false);
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();

    // It's needed to cleanup the password store asynchronously.
    RunAllPendingTasks();
  }

  PrefService* GetPrefs() { return prefs_.get(); }

  void RunAllPendingTasks() { task_environment_.RunUntilIdle(); }

  PasswordForm MakeSimpleForm(int id) {
    PasswordForm form;
    form.origin = GURL("https://example.org/");
    form.signon_realm = "https://example.org/";
    form.username_value = ASCIIToUTF16("username") + base::NumberToString16(id);
    form.password_value = ASCIIToUTF16("p4ssword") + base::NumberToString16(id);
    return form;
  }

  PasswordForm MakeSimpleBlacklistedForm(int id) {
    PasswordForm form;
    std::string link = "https://example" + base::NumberToString(id) + ".org/";
    form.origin = GURL(link);
    form.signon_realm = link;
    form.blacklisted_by_user = true;
    return form;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestPasswordStore> store_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(PasswordManagerOnboardingTest, CredentialsCountUnderThreshold) {
  // Check that the count of credentials is handled correctly.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPasswordManagerOnboardingAndroid);
  for (int id = 0; id < kOnboardingCredentialsThreshold - 1; ++id) {
    store_->AddLogin(MakeSimpleForm(id));
  }
  constexpr int kNumberOfBlacklistedLogins = 5;
  for (int id = 0; id < kNumberOfBlacklistedLogins; ++id) {
    store_->AddLogin(MakeSimpleBlacklistedForm(id));
  }
  RunAllPendingTasks();
  UpdateOnboardingState(store_, GetPrefs(), base::TimeDelta::FromSeconds(0));
  RunAllPendingTasks();
  EXPECT_EQ(prefs_->GetInteger(prefs::kPasswordManagerOnboardingState),
            static_cast<int>(OnboardingState::kShouldShow));
}

TEST_F(PasswordManagerOnboardingTest,
       CredentialsCountThresholdHitAfterDoNotShow) {
  // Check that the threshold is handled correctly.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPasswordManagerOnboardingAndroid);
  for (int id = 0; id < kOnboardingCredentialsThreshold; ++id) {
    store_->AddLogin(MakeSimpleForm(id));
  }
  RunAllPendingTasks();
  UpdateOnboardingState(store_, GetPrefs(), base::TimeDelta::FromSeconds(0));
  RunAllPendingTasks();
  EXPECT_EQ(prefs_->GetInteger(prefs::kPasswordManagerOnboardingState),
            static_cast<int>(OnboardingState::kDoNotShow));
}

TEST_F(PasswordManagerOnboardingTest,
       CredentialsCountThresholdHitAfterShouldShow) {
  // Check that the threshold is handled correctly
  // in case the current state was |kShouldShow|.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPasswordManagerOnboardingAndroid);
  prefs_->SetInteger(prefs::kPasswordManagerOnboardingState,
                     static_cast<int>(OnboardingState::kShouldShow));
  for (int id = 0; id < kOnboardingCredentialsThreshold; ++id) {
    store_->AddLogin(MakeSimpleForm(id));
  }
  RunAllPendingTasks();
  UpdateOnboardingState(store_, GetPrefs(), base::TimeDelta::FromSeconds(0));
  RunAllPendingTasks();
  EXPECT_EQ(prefs_->GetInteger(prefs::kPasswordManagerOnboardingState),
            static_cast<int>(OnboardingState::kDoNotShow));
}

TEST_F(PasswordManagerOnboardingTest, CredentialsCountThresholdHitAfterShown) {
  // Check that the threshold is handled correctly
  // in case the current state is |kShown|.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPasswordManagerOnboardingAndroid);
  prefs_->SetInteger(prefs::kPasswordManagerOnboardingState,
                     static_cast<int>(OnboardingState::kShown));
  for (int id = 0; id < kOnboardingCredentialsThreshold; ++id) {
    store_->AddLogin(MakeSimpleForm(id));
  }
  RunAllPendingTasks();
  UpdateOnboardingState(store_, GetPrefs(), base::TimeDelta::FromSeconds(0));
  RunAllPendingTasks();
  EXPECT_EQ(prefs_->GetInteger(prefs::kPasswordManagerOnboardingState),
            static_cast<int>(OnboardingState::kShown));
}

TEST_F(PasswordManagerOnboardingTest, DoNotShowAfterShown) {
  // If the current state is |kShown| it should stay this way,
  // so that the onboarding won't be shown twice.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPasswordManagerOnboardingAndroid);
  prefs_->SetInteger(prefs::kPasswordManagerOnboardingState,
                     static_cast<int>(OnboardingState::kShown));
  UpdateOnboardingState(store_, GetPrefs(), base::TimeDelta::FromSeconds(0));
  RunAllPendingTasks();
  EXPECT_EQ(prefs_->GetInteger(prefs::kPasswordManagerOnboardingState),
            static_cast<int>(OnboardingState::kShown));
}

TEST_F(PasswordManagerOnboardingTest, FeatureDisabledAfterShowing) {
  prefs_->SetInteger(prefs::kPasswordManagerOnboardingState,
                     static_cast<int>(OnboardingState::kShown));
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPasswordManagerOnboardingAndroid);
  UpdateOnboardingState(store_, GetPrefs(), base::TimeDelta::FromSeconds(0));
  RunAllPendingTasks();
  EXPECT_EQ(prefs_->GetInteger(prefs::kPasswordManagerOnboardingState),
            static_cast<int>(OnboardingState::kShown));
}

TEST_F(PasswordManagerOnboardingTest, ShouldShowOnboardingState) {
  // Check that the |kPasswordManagerOnboardingState| pref is handled correctly.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordManagerOnboardingAndroid);
  EXPECT_FALSE(ShouldShowOnboarding(GetPrefs(), PasswordUpdateBool(false),
                                    BlacklistedBool(false),
                                    SyncState::SYNCING_NORMAL_ENCRYPTION));

  prefs_->SetInteger(password_manager::prefs::kPasswordManagerOnboardingState,
                     static_cast<int>(OnboardingState::kShouldShow));
  EXPECT_TRUE(ShouldShowOnboarding(GetPrefs(), PasswordUpdateBool(false),
                                   BlacklistedBool(false),
                                   SyncState::SYNCING_NORMAL_ENCRYPTION));

  prefs_->SetInteger(password_manager::prefs::kPasswordManagerOnboardingState,
                     static_cast<int>(OnboardingState::kDoNotShow));
  EXPECT_FALSE(ShouldShowOnboarding(GetPrefs(), PasswordUpdateBool(false),
                                    BlacklistedBool(false),
                                    SyncState::SYNCING_NORMAL_ENCRYPTION));

  prefs_->SetInteger(password_manager::prefs::kPasswordManagerOnboardingState,
                     static_cast<int>(OnboardingState::kShown));
  EXPECT_FALSE(ShouldShowOnboarding(GetPrefs(), PasswordUpdateBool(false),
                                    BlacklistedBool(false),
                                    SyncState::SYNCING_NORMAL_ENCRYPTION));
}

TEST_F(PasswordManagerOnboardingTest, ShouldShowOnboardingFeatureDisabled) {
  // Feature disabled ==> don't show.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPasswordManagerOnboardingAndroid);
  prefs_->SetInteger(password_manager::prefs::kPasswordManagerOnboardingState,
                     static_cast<int>(OnboardingState::kShouldShow));
  EXPECT_FALSE(ShouldShowOnboarding(GetPrefs(), PasswordUpdateBool(false),
                                    BlacklistedBool(false),
                                    SyncState::SYNCING_NORMAL_ENCRYPTION));
}

TEST_F(PasswordManagerOnboardingTest, ShouldShowOnboardingPasswordUpdate) {
  // Password update ==> don't show.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordManagerOnboardingAndroid);
  EXPECT_FALSE(ShouldShowOnboarding(GetPrefs(), PasswordUpdateBool(true),
                                    BlacklistedBool(false),
                                    SyncState::SYNCING_NORMAL_ENCRYPTION));
}

TEST_F(PasswordManagerOnboardingTest,
       ShouldShowOnboardingBlacklistedCredentials) {
  // Blacklisted credentials ==> don't show.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordManagerOnboardingAndroid);
  EXPECT_FALSE(ShouldShowOnboarding(GetPrefs(), PasswordUpdateBool(false),
                                    BlacklistedBool(true),
                                    SyncState::SYNCING_NORMAL_ENCRYPTION));
}

TEST_F(PasswordManagerOnboardingTest,
       ShouldShowOnboardingPasswordSyncDisabled) {
  // Password sync disabled ==> don't show.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordManagerOnboardingAndroid);
  EXPECT_FALSE(ShouldShowOnboarding(GetPrefs(), PasswordUpdateBool(false),
                                    BlacklistedBool(false),
                                    SyncState::NOT_SYNCING));
}

TEST_F(PasswordManagerOnboardingTest,
       SavingFlowMetricsRecorderInfobarNoDirectInteraction) {
  base::HistogramTester histogram_tester;
  {
    SavingFlowMetricsRecorder recorder;
    recorder.SetFlowResult(password_manager::metrics_util::UIDismissalReason::
                               NO_DIRECT_INTERACTION);
  }
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlow",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kInfobarNoDirectInteraction,
      1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding", 0);
}

TEST_F(PasswordManagerOnboardingTest,
       SavingFlowMetricsRecorderInfobarClickedSave) {
  base::HistogramTester histogram_tester;
  {
    SavingFlowMetricsRecorder recorder;
    recorder.SetFlowResult(
        password_manager::metrics_util::UIDismissalReason::CLICKED_SAVE);
  }
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlow",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kInfobarClickedSave,
      1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding", 0);
}

TEST_F(PasswordManagerOnboardingTest,
       SavingFlowMetricsRecorderInfobarClickedCancel) {
  base::HistogramTester histogram_tester;
  {
    SavingFlowMetricsRecorder recorder;
    recorder.SetFlowResult(
        password_manager::metrics_util::UIDismissalReason::CLICKED_CANCEL);
  }
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlow",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kInfobarClickedCancel,
      1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding", 0);
}

TEST_F(PasswordManagerOnboardingTest,
       SavingFlowMetricsRecorderInfobarClickedNever) {
  base::HistogramTester histogram_tester;
  {
    SavingFlowMetricsRecorder recorder;
    recorder.SetFlowResult(
        password_manager::metrics_util::UIDismissalReason::CLICKED_NEVER);
  }
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlow",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kInfobarClickedNever,
      1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding", 0);
}

TEST_F(PasswordManagerOnboardingTest,
       SavingFlowMetricsRecorderOnboardingRejected) {
  base::HistogramTester histogram_tester;
  {
    SavingFlowMetricsRecorder recorder;
    recorder.SetOnboardingShown();
    recorder.SetFlowResult(
        password_manager::metrics_util::OnboardingUIDismissalReason::kRejected);
  }
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlow",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kOnboardingRejected,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kOnboardingRejected,
      1);
}

TEST_F(PasswordManagerOnboardingTest,
       SavingFlowMetricsRecorderOnboardingDismissed) {
  base::HistogramTester histogram_tester;
  {
    SavingFlowMetricsRecorder recorder;
    recorder.SetOnboardingShown();
    recorder.SetFlowResult(password_manager::metrics_util::
                               OnboardingUIDismissalReason::kDismissed);
  }
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlow",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kOnboardingDismissed,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kOnboardingDismissed,
      1);
}

TEST_F(PasswordManagerOnboardingTest,
       SavingFlowMetricsRecorderAfterOnboarding) {
  base::HistogramTester histogram_tester;
  {
    SavingFlowMetricsRecorder recorder;
    recorder.SetOnboardingShown();
    recorder.SetFlowResult(
        password_manager::metrics_util::UIDismissalReason::CLICKED_SAVE);
  }
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlow",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kInfobarClickedSave,
      1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.Onboarding.ResultOfSavingFlowAfterOnboarding",
      password_manager::metrics_util::OnboardingResultOfSavingFlow::
          kInfobarClickedSave,
      1);
}

}  // namespace password_manager
