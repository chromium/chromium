// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#if !defined(NDEBUG)
#include "components/signin/public/android/jni_headers/AccountCapabilities_jni.h"
#endif
#endif  // BUILDFLAG(IS_ANDROID)

namespace {
using testing::Contains;
using testing::Not;
}  // namespace

class AccountCapabilitiesTest : public testing::Test {};

TEST_F(AccountCapabilitiesTest, GetSupportedAccountCapabilityNames) {
  auto names = AccountCapabilities::GetSupportedAccountCapabilityNames();

  // Check one of the existing expected account capabilities.
  EXPECT_THAT(names, Contains(kCanUseModelExecutionFeaturesName));
}

#if !defined(NDEBUG)
TEST_F(AccountCapabilitiesTest,
       GetSupportedAccountCapabilityNames_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(switches::kEnableFakeCapabilityForTesting);

  auto names =
      AccountCapabilities::GetSupportedAccountCapabilityNamesInternal();

  // Check one of the existing expected account capabilities.
  EXPECT_THAT(names, Not(Contains(kFakeCapabilityForTestingName)));
}

TEST_F(AccountCapabilitiesTest,
       GetSupportedAccountCapabilityNames_FlagEnabled) {
  base::test::ScopedFeatureList feature_list{
      switches::kEnableFakeCapabilityForTesting};

  auto names =
      AccountCapabilities::GetSupportedAccountCapabilityNamesInternal();

  // Check one of the existing expected account capabilities.
  EXPECT_THAT(names, Contains(kFakeCapabilityForTestingName));
}
#endif  // !defined(NDEBUG)

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

#if !BUILDFLAG(IS_IOS)
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
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID)
TEST_F(AccountCapabilitiesTest, CanMakeChromeSearchEngineChoiceScreenChoice) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_make_chrome_search_engine_choice_screen_choice(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_make_chrome_search_engine_choice_screen_choice(true);
  EXPECT_EQ(capabilities.can_make_chrome_search_engine_choice_screen_choice(),
            signin::Tribool::kTrue);

  mutator.set_can_make_chrome_search_engine_choice_screen_choice(false);
  EXPECT_EQ(capabilities.can_make_chrome_search_engine_choice_screen_choice(),
            signin::Tribool::kFalse);
}
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_IOS)
TEST_F(AccountCapabilitiesTest, CanSignInToChrome) {
  base::test::ScopedFeatureList feature_list{
      switches::kEnforceCanSignInToChromeCapability};
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_sign_in_to_chrome(), signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_sign_in_to_chrome(true);
  EXPECT_EQ(capabilities.can_sign_in_to_chrome(), signin::Tribool::kTrue);

  mutator.set_can_sign_in_to_chrome(false);
  EXPECT_EQ(capabilities.can_sign_in_to_chrome(), signin::Tribool::kFalse);
}
#endif  // BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_IOS)
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
#endif  // !BUILDFLAG(IS_IOS)

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

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(AccountCapabilitiesTest, CanToggleAutoUpdates) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_toggle_auto_updates(true);
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kTrue);

  mutator.set_can_toggle_auto_updates(false);
  EXPECT_EQ(capabilities.can_toggle_auto_updates(), signin::Tribool::kFalse);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_IOS)
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
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_IOS)
TEST_F(AccountCapabilitiesTest, CanUseEduFeatures) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_edu_features(), signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_edu_features(true);
  EXPECT_EQ(capabilities.can_use_edu_features(), signin::Tribool::kTrue);

  mutator.set_can_use_edu_features(false);
  EXPECT_EQ(capabilities.can_use_edu_features(), signin::Tribool::kFalse);
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(AccountCapabilitiesTest, CanUseMantaService) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_manta_service(), signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_manta_service(true);
  EXPECT_EQ(capabilities.can_use_manta_service(), signin::Tribool::kTrue);

  mutator.set_can_use_manta_service(false);
  EXPECT_EQ(capabilities.can_use_manta_service(), signin::Tribool::kFalse);
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

TEST_F(AccountCapabilitiesTest, IsSubjectToAccountLevelEnterprisePolicies) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_subject_to_account_level_enterprise_policies(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_subject_to_account_level_enterprise_policies(true);
  EXPECT_EQ(capabilities.is_subject_to_account_level_enterprise_policies(),
            signin::Tribool::kTrue);

  mutator.set_is_subject_to_account_level_enterprise_policies(false);
  EXPECT_EQ(capabilities.is_subject_to_account_level_enterprise_policies(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, IsSubjectToEnterpriseFeatures) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.is_subject_to_enterprise_features(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_is_subject_to_enterprise_features(true);
  EXPECT_EQ(capabilities.is_subject_to_enterprise_features(),
            signin::Tribool::kTrue);

  mutator.set_is_subject_to_enterprise_features(false);
  EXPECT_EQ(capabilities.is_subject_to_enterprise_features(),
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

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(AccountCapabilitiesTest, CanUseGenerativeAiPhotoEditing) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_generative_ai_photo_editing(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_generative_ai_photo_editing(true);
  EXPECT_EQ(capabilities.can_use_generative_ai_photo_editing(),
            signin::Tribool::kTrue);

  mutator.set_can_use_generative_ai_photo_editing(false);
  EXPECT_EQ(capabilities.can_use_generative_ai_photo_editing(),
            signin::Tribool::kFalse);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(AccountCapabilitiesTest, CanUseGenerativeAi) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_use_chromeos_generative_ai(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_use_chromeos_generative_ai(true);
  EXPECT_EQ(capabilities.can_use_chromeos_generative_ai(),
            signin::Tribool::kTrue);

  mutator.set_can_use_chromeos_generative_ai(false);
  EXPECT_EQ(capabilities.can_use_chromeos_generative_ai(),
            signin::Tribool::kFalse);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if !defined(NDEBUG)
TEST_F(AccountCapabilitiesTest, ConversionWithJNI_FlagGuardDisabled_JavaToCpp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(switches::kEnableFakeCapabilityForTesting);

  // C++ shouldn't support the fake capability.
  EXPECT_THAT(AccountCapabilities::GetSupportedAccountCapabilityNamesInternal(),
              Not(Contains(kFakeCapabilityForTestingName)));

  // Create a Java AccountCapabilities object with a capability that is not
  // supported in C++.
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> java_capability_names;
  java_capability_names.push_back(kFakeCapabilityForTestingName);
  java_capability_names.push_back(kCanFetchFamilyMemberInfoCapabilityName);
  std::vector<bool> java_capability_values;
  java_capability_values.push_back(true);
  java_capability_values.push_back(false);

  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      signin::Java_AccountCapabilities_Constructor(
          env, base::android::ToJavaArrayOfStrings(env, java_capability_names),
          base::android::ToJavaBooleanArray(env, java_capability_values));

  AccountCapabilities cpp_capabilities =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  // The fake capability should not be present in the C++ object.
  EXPECT_EQ(cpp_capabilities.GetCapabilityByName(kFakeCapabilityForTestingName),
            signin::Tribool::kUnknown);
  // The known capability should be present.
  EXPECT_EQ(cpp_capabilities.can_fetch_family_member_info(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_FlagGuardDisabled_CppToJava) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(switches::kEnableFakeCapabilityForTesting);

  // C++ shouldn't support the fake capability.
  EXPECT_THAT(AccountCapabilities::GetSupportedAccountCapabilityNamesInternal(),
              Not(Contains(kFakeCapabilityForTestingName)));

  AccountCapabilities cpp_capabilities;
  AccountCapabilitiesTestMutator mutator(&cpp_capabilities);
  mutator.set_can_fetch_family_member_info(true);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      cpp_capabilities.ConvertToJavaAccountCapabilities(env);

  // The fake capability is not supported in C++, so it shouldn't be in the
  // converted Java object.
  signin::Tribool fake_capability_in_java = static_cast<signin::Tribool>(
      signin::Java_AccountCapabilities_getCapabilityByName(
          env, java_capabilities,
          base::android::ConvertUTF8ToJavaString(
              env, kFakeCapabilityForTestingName)));
  EXPECT_EQ(fake_capability_in_java, signin::Tribool::kUnknown);

  // The known capability should be present.
  signin::Tribool known_capability_in_java = static_cast<signin::Tribool>(
      signin::Java_AccountCapabilities_getCapabilityByName(
          env, java_capabilities,
          base::android::ConvertUTF8ToJavaString(
              env, kCanFetchFamilyMemberInfoCapabilityName)));
  EXPECT_EQ(known_capability_in_java, signin::Tribool::kTrue);
}
#endif  // !defined(NDEBUG)

#endif
