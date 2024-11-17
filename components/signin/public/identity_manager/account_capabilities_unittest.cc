// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities.h"

#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif

class AccountCapabilitiesTest : public testing::Test {};

TEST_F(AccountCapabilitiesTest, CanFetchFamilyMemberInfo) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_fetch_family_member_info(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_fetch_family_member_info(true);
  EXPECT_EQ(capabilities.can_fetch_family_member_info(),
            signin::Tribool::kTrue);

  mutator.set_can_fetch_family_member_info(false);
  EXPECT_EQ(capabilities.can_fetch_family_member_info(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanHaveEmailAddressDisplayed) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_have_email_address_displayed(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_have_email_address_displayed(true);
  EXPECT_EQ(capabilities.can_have_email_address_displayed(),
            signin::Tribool::kTrue);

  mutator.set_can_have_email_address_displayed(false);
  EXPECT_EQ(capabilities.can_have_email_address_displayed(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest,
       CanShowHistorySyncOptInsWithoutMinorModeRestrictions) {
  AccountCapabilities capabilities;
  EXPECT_EQ(
      capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);
  EXPECT_EQ(
      capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kTrue);

  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      false);
  EXPECT_EQ(
      capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanRunChromePrivacySandboxTrials) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_run_chrome_privacy_sandbox_trials(true);
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kTrue);

  mutator.set_can_run_chrome_privacy_sandbox_trials(false);
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, IsOptedInToParentalSupervision) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_opted_in_to_parental_supervision(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_opted_in_to_parental_supervision(true);
  EXPECT_EQ(capabilities.is_opted_in_to_parental_supervision(),
            signin::Tribool::kTrue);

  mutator.set_is_opted_in_to_parental_supervision(false);
  EXPECT_EQ(capabilities.is_opted_in_to_parental_supervision(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanToggleAutoUpdates) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_toggle_auto_updates(true);
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kTrue);

  mutator.set_can_toggle_auto_updates(false);
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanUseChromeIpProtection) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_chrome_ip_protection(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_chrome_ip_protection(true);
  EXPECT_EQ(capabilities.can_use_chrome_ip_protection(),
            signin::Tribool::kTrue);

  mutator.set_can_use_chrome_ip_protection(false);
  EXPECT_EQ(capabilities.can_use_chrome_ip_protection(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanUseDevToolsGenerativeAiFeatures) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_devtools_generative_ai_features(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_devtools_generative_ai_features(true);
  EXPECT_EQ(capabilities.can_use_devtools_generative_ai_features(),
            signin::Tribool::kTrue);

  mutator.set_can_use_devtools_generative_ai_features(false);
  EXPECT_EQ(capabilities.can_use_devtools_generative_ai_features(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanUseEduFeatures) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_edu_features(), signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_edu_features(true);
  EXPECT_EQ(capabilities.can_use_edu_features(), signin::Tribool::kTrue);

  mutator.set_can_use_edu_features(false);
  EXPECT_EQ(capabilities.can_use_edu_features(), signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanUseMantaService) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_manta_service(), signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_manta_service(true);
  EXPECT_EQ(capabilities.can_use_manta_service(), signin::Tribool::kTrue);

  mutator.set_can_use_manta_service(false);
  EXPECT_EQ(capabilities.can_use_manta_service(), signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanUseCopyEditorFeature) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_copyeditor_feature(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_copyeditor_feature(true);
  EXPECT_EQ(capabilities.can_use_copyeditor_feature(), signin::Tribool::kTrue);

  mutator.set_can_use_copyeditor_feature(false);
  EXPECT_EQ(capabilities.can_use_copyeditor_feature(), signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanUseModelExecutionFeatures) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_model_execution_features(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_model_execution_features(true);
  EXPECT_EQ(capabilities.can_use_model_execution_features(),
            signin::Tribool::kTrue);

  mutator.set_can_use_model_execution_features(false);
  EXPECT_EQ(capabilities.can_use_model_execution_features(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, IsAllowedForMachineLearning) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_allowed_for_machine_learning(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_allowed_for_machine_learning(true);
  EXPECT_EQ(capabilities.is_allowed_for_machine_learning(),
            signin::Tribool::kTrue);

  mutator.set_is_allowed_for_machine_learning(false);
  EXPECT_EQ(capabilities.is_allowed_for_machine_learning(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, IsSubjectToEnterprisePolicies) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_subject_to_enterprise_policies(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_subject_to_enterprise_policies(true);
  EXPECT_EQ(capabilities.is_subject_to_enterprise_policies(),
            signin::Tribool::kTrue);

  mutator.set_is_subject_to_enterprise_policies(false);
  EXPECT_EQ(capabilities.is_subject_to_enterprise_policies(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, IsSubjectToParentalControls) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_subject_to_parental_controls(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  EXPECT_EQ(capabilities.is_subject_to_parental_controls(),
            signin::Tribool::kTrue);

  mutator.set_is_subject_to_parental_controls(false);
  EXPECT_EQ(capabilities.is_subject_to_parental_controls(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanUseSpeakerLabelInRecorderApp) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_speaker_label_in_recorder_app(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_speaker_label_in_recorder_app(true);
  EXPECT_EQ(capabilities.can_use_speaker_label_in_recorder_app(),
            signin::Tribool::kTrue);

  mutator.set_can_use_speaker_label_in_recorder_app(false);
  EXPECT_EQ(capabilities.can_use_speaker_label_in_recorder_app(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanUseGenerativeAiInRecorderApp) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_generative_ai_in_recorder_app(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_generative_ai_in_recorder_app(true);
  EXPECT_EQ(capabilities.can_use_generative_ai_in_recorder_app(),
            signin::Tribool::kTrue);

  mutator.set_can_use_generative_ai_in_recorder_app(false);
  EXPECT_EQ(capabilities.can_use_generative_ai_in_recorder_app(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest,
       IsSubjectToPrivacySandboxRestrictedMeasurementApiNotice) {
  AccountCapabilities capabilities;
  EXPECT_EQ(
      capabilities
          .is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
      signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator
      .set_is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(
          true);
  EXPECT_EQ(
      capabilities
          .is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
      signin::Tribool::kTrue);

  mutator
      .set_is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(
          false);
  EXPECT_EQ(
      capabilities
          .is_subject_to_chrome_privacy_sandbox_restricted_measurement_notice(),
      signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, AreAnyCapabilitiesKnown_Empty) {
  AccountCapabilities capabilities;
  EXPECT_FALSE(capabilities.AreAnyCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAnyCapabilitiesKnown_PartiallyFilled) {
  AccountCapabilities capabilities;

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);
  EXPECT_TRUE(capabilities.AreAnyCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Empty) {
  AccountCapabilities capabilities;
  EXPECT_FALSE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_PartiallyFilled) {
  AccountCapabilities capabilities;

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);
  EXPECT_FALSE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Filled) {
  AccountCapabilities capabilities;

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.SetAllSupportedCapabilities(true);
  EXPECT_TRUE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_UnknownToKnown) {
  AccountCapabilities capabilities;

  AccountCapabilities other;
  AccountCapabilitiesTestMutator mutator(&other);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(
      signin::Tribool::kTrue,
      capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_KnownToUnknown) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  AccountCapabilities other;

  EXPECT_FALSE(capabilities.UpdateWith(other));
  EXPECT_EQ(
      signin::Tribool::kTrue,
      capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_OverwriteKnown) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  AccountCapabilities other;
  AccountCapabilitiesTestMutator other_mutator(&other);
  other_mutator
      .set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(false);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(
      signin::Tribool::kFalse,
      capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions());
}

#if BUILDFLAG(IS_ANDROID)

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolTrue) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      true);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolFalse) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
      false);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolUnknown) {
  AccountCapabilities capabilities;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

#endif
