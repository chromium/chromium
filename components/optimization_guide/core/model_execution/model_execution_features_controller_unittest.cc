// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/component_updater/pref_names.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/feature_registry/settings_ui_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/tflite/buildflags.h"

namespace optimization_guide {

using model_execution::prefs::ModelExecutionEnterprisePolicyValue;
using policy::ScopedManagementServiceOverrideForTesting;

class TestManagementService : public policy::ManagementService {
 public:
  TestManagementService() : policy::ManagementService({}) {}
  void SetManagementStatusProviderForTesting(
      std::vector<std::unique_ptr<policy::ManagementStatusProvider>>
          providers) {
    SetManagementStatusProvider(std::move(providers));
  }
};

class ModelExecutionFeaturesControllerTest : public testing::Test {
 public:
  ModelExecutionFeaturesControllerTest() = default;
  ~ModelExecutionFeaturesControllerTest() override = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());
    model_execution::prefs::RegisterProfilePrefs(pref_service_->registry());
    model_execution::prefs::RegisterLocalStatePrefs(local_state_->registry());
    local_state_->registry()->RegisterBooleanPref(
        ::prefs::kComponentUpdatesEnabled, true, PrefRegistry::LOSSY_PREF);
  }

  void CreateController(
      ModelExecutionFeaturesController::DogfoodStatus dogfood_status =
          ModelExecutionFeaturesController::DogfoodStatus::NON_DOGFOOD,
      bool is_official_build = true) {
    controller_ = std::make_unique<ModelExecutionFeaturesController>(
        pref_service_.get(), identity_test_env_.identity_manager(),
        local_state_.get(), management_service(), dogfood_status,
        is_official_build);
  }

  void EnableSignIn() {
    auto account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
    RunUntilIdle();
  }

  void EnableSignInWithoutCapability() {
    auto account_info = identity_test_env()->MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(false);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
    RunUntilIdle();
  }

  prefs::FeatureOptInState GetFeaturePrefValue(UserVisibleFeatureKey feature) {
    return static_cast<prefs::FeatureOptInState>(
        pref_service_->GetInteger(prefs::GetSettingEnabledPrefName(feature)));
  }

  void SetEnterprisePolicy(UserVisibleFeatureKey feature,
                           ModelExecutionEnterprisePolicyValue value) {
    const char* key = SettingsUiRegistry::GetInstance()
                          .GetFeature(feature)
                          ->enterprise_policy()
                          .name();
    ASSERT_TRUE(key);
    return pref_service_->SetInteger(key, static_cast<int>(value));
  }

  ModelExecutionFeaturesController* controller() { return controller_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  PrefService* pref_service() { return pref_service_.get(); }
  PrefService* local_state() { return local_state_.get(); }
  policy::ManagementService* management_service() {
    return &management_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  TestManagementService management_service_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<ModelExecutionFeaturesController> controller_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ModelExecutionFeaturesControllerTest, OneFeatureSettingVisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility},
      {features::internal::kWallpaperSearchGraduated,
       features::internal::kTabOrganizationGraduated});
  CreateController();

  EnableSignIn();
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFeatureAlreadyEnabled,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleFieldTrialDisabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleFieldTrialDisabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kWallpaperSearch));
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup.Compose", false,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup."
      "TabOrganization",
      false, 1);
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup."
      "WallpaperSearch",
      false, 1);
}

TEST_F(ModelExecutionFeaturesControllerTest,
       DefaultFeatureSettingForUnsignedUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kComposeSettingsVisibility, {}},
       {features::internal::kTabOrganizationSettingsVisibility, {}}},
      {features::internal::kWallpaperSearchGraduated});
  CreateController();
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kNotVisibleUnsignedUser,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleUnsignedUser,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));

  EnableSignIn();
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFeatureAlreadyEnabled,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFeatureAlreadyEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleFieldTrialDisabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kWallpaperSearch));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       FeatureSettingChangedForUnsignedUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kComposeSettingsVisibility,
        {{"allow_unsigned_user", "true"}}},
       {features::internal::kTabOrganizationSettingsVisibility,
        {{"allow_unsigned_user", "true"}}}},
      {features::internal::kWallpaperSearchGraduated});
  CreateController();
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFeatureAlreadyEnabled,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFeatureAlreadyEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));

  EnableSignIn();
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFeatureAlreadyEnabled,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFeatureAlreadyEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleFieldTrialDisabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kWallpaperSearch));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       FeatureAllowedForSignedUserWithoutCapabilityWhenUnsignedUserAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kComposeSettingsVisibility,
        {{"allow_unsigned_user", "true"}}},
       {features::internal::kTabOrganizationSettingsVisibility,
        {{"allow_unsigned_user", "true"}}}},
      {features::internal::kWallpaperSearchGraduated});
  CreateController();
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFeatureAlreadyEnabled,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFeatureAlreadyEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));

  EnableSignInWithoutCapability();
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFeatureAlreadyEnabled,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFeatureAlreadyEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleModelExecutionCapability,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kWallpaperSearch));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       FeatureSettingDisabledWhenCapabilityDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility},
      {features::internal::kWallpaperSearchGraduated,
       features::internal::kTabOrganizationGraduated});
  CreateController();
  EnableSignInWithoutCapability();
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kNotVisibleModelExecutionCapability,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleModelExecutionCapability,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleModelExecutionCapability,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kWallpaperSearch));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       FeatureSettingAllowedWhenCapabilityCheckDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility,
       features::internal::kModelExecutionCapabilityDisable},
      {features::internal::kTabOrganizationGraduated});
  CreateController();
  EnableSignInWithoutCapability();

  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFeatureAlreadyEnabled,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleFieldTrialDisabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));
}

TEST_F(ModelExecutionFeaturesControllerTest, GraduatedFeatureIsVisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {features::internal::kComposeGraduated,
       features::internal::kTabOrganizationGraduated,
       features::internal::kWallpaperSearchGraduated},
      /*disabled_features=*/
      {features::internal::kComposeSettingsVisibility,
       features::internal::kTabOrganizationSettingsVisibility,
       features::internal::kWallpaperSearchSettingsVisibility});
  CreateController();

  EnableSignIn();
  // GetSettingsVisibility
  EXPECT_EQ(
      ModelExecutionFeaturesController::SettingsVisibilityResult::
          kVisibleFeatureAlreadyEnabled,
      controller()->GetSettingsVisibility(UserVisibleFeatureKey::kCompose));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFeatureAlreadyEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kTabOrganization));
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFeatureAlreadyEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kWallpaperSearch));
  // ShouldFeatureBeCurrentlyEnabledForUser
  EXPECT_TRUE(controller()->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));
  EXPECT_TRUE(controller()->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));
  EXPECT_TRUE(controller()->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup.Compose", false,
      1);
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup."
      "TabOrganization",
      false, 1);
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup."
      "WallpaperSearch",
      false, 1);
}

TEST_F(ModelExecutionFeaturesControllerTest,
       EnterprisePolicyUnsetLoggingDisabledForEnterpriseUser) {
  ScopedManagementServiceOverrideForTesting override(
      management_service(),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  CreateController();
  EnableSignIn();
  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kActorLogin);
  EXPECT_FALSE(
      controller()->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       EnterprisePolicyUnsetLoggingEnabledForNonEnterpriseUser) {
  CreateController();
  EnableSignIn();

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kActorLogin);
  EXPECT_TRUE(
      controller()->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       Logging_DisabledByEnterprisePolicy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility},
      {features::internal::kComposeGraduated});
  CreateController();

  auto feature = UserVisibleFeatureKey::kCompose;
  EnableSignIn();
  SetEnterprisePolicy(
      feature, ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(
      controller()->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       Logging_DisabledByEnterprisePolicy_NotOverriddenByDogfoodStatusAlone) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility},
      {features::internal::kComposeGraduated});
  CreateController(ModelExecutionFeaturesController::DogfoodStatus::DOGFOOD);

  auto feature = UserVisibleFeatureKey::kCompose;
  EnableSignIn();
  SetEnterprisePolicy(
      feature, ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(
      controller()->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       Logging_DisabledByEnterprisePolicy_NotOverriddenByDeveloperBuildAlone) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility},
      {features::internal::kComposeGraduated});
  CreateController(ModelExecutionFeaturesController::DogfoodStatus::NON_DOGFOOD,
                   /*is_official_build=*/false);

  auto feature = UserVisibleFeatureKey::kCompose;
  EnableSignIn();
  SetEnterprisePolicy(
      feature, ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(
      controller()->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       Logging_DisabledByEnterprisePolicy_NotOverriddenBySwitchAlone) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility},
      {features::internal::kComposeGraduated});
  CreateController();

  auto feature = UserVisibleFeatureKey::kCompose;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableModelQualityDogfoodLogging);
  EnableSignIn();
  SetEnterprisePolicy(
      feature, ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(
      controller()->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       Logging_DisabledByEnterprisePolicy_OverriddenBySwitchWhenDogfood) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility},
      {features::internal::kComposeGraduated});
  CreateController(ModelExecutionFeaturesController::DogfoodStatus::DOGFOOD);

  auto feature = UserVisibleFeatureKey::kCompose;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableModelQualityDogfoodLogging);
  EnableSignIn();
  SetEnterprisePolicy(
      feature, ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_TRUE(
      controller()->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

TEST_F(
    ModelExecutionFeaturesControllerTest,
    Logging_DisabledByEnterprisePolicy_OverriddenBySwitchWhenDeveloperBuild) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kComposeSettingsVisibility},
      {features::internal::kComposeGraduated});
  CreateController(ModelExecutionFeaturesController::DogfoodStatus::NON_DOGFOOD,
                   /*is_official_build=*/false);

  auto feature = UserVisibleFeatureKey::kCompose;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableModelQualityDogfoodLogging);
  EnableSignIn();
  SetEnterprisePolicy(
      feature, ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_TRUE(
      controller()->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       HistorySearchVisibilityWithXNNPACK) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::internal::kHistorySearchSettingsVisibility}, {});
  CreateController();

  EnableSignIn();

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFieldTrialEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kHistorySearch));
#else
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleHardwareUnsupported,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kHistorySearch));
#endif
}

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
TEST_F(ModelExecutionFeaturesControllerTest,
       HistorySearchVisibilityWithPerformanceClass) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kHistorySearchSettingsVisibility,
        {{"PerformanceClassListForHistorySearch", "3,4,5"}}}},
      {});

  CreateController();

  EnableSignIn();

  // Not visible - performance class not in the list
  UpdatePerformanceClassPref(local_state(),
                             OnDeviceModelPerformanceClass::kVeryLow);
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleHardwareUnsupported,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kHistorySearch));

  // Visible - performance class in the list.
  UpdatePerformanceClassPref(local_state(),
                             OnDeviceModelPerformanceClass::kMedium);
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFieldTrialEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kHistorySearch));
}

TEST_F(ModelExecutionFeaturesControllerTest,
       HistorySearchSettingsIsHiddenWithComponentUpdatesDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitWithFeatures(
      {features::internal::kHistorySearchSettingsVisibility}, {});

  CreateController();

  EnableSignIn();

  // Visible by default since the feature is on.
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kVisibleFieldTrialEnabled,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kHistorySearch));

  // Not visible if component updates are disabled.
  local_state()->SetBoolean(::prefs::kComponentUpdatesEnabled, false);
  EXPECT_EQ(ModelExecutionFeaturesController::SettingsVisibilityResult::
                kNotVisibleEnterprisePolicy,
            controller()->GetSettingsVisibility(
                UserVisibleFeatureKey::kHistorySearch));
}
#endif

}  // namespace optimization_guide
