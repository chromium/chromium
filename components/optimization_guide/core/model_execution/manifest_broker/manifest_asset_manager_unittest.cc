// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

class ManifestAssetManagerTest : public testing::Test {
 public:
  ManifestAssetManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationGuideModelExecution,
         features::kOptimizationGuideOnDeviceModel},
        {});
    model_execution::prefs::RegisterLocalStatePrefs(local_state_.registry());
    usage_tracker_ = std::make_unique<UsageTracker>(&local_state_);
    usage_tracker_->OnDeviceEligibleUseCaseUsed("compose");
  }

  Manifest CreateDummyManifest(
      const std::vector<std::pair<std::string, std::string>>&
          use_case_to_asset_id,
      const std::string& version = "1.0.0.0",
      const std::string& public_key_prefix = "dummy_key_") {
    ManifestBuilder builder;

    // Map these dummy assets to valid DeviceUseCase so they don't get filtered
    // out. To avoid `ParseError::kDuplicateIdentifier` or missing references,
    // we give each dummy asset its own recipe chain.
    for (const auto& [use_case_name, asset_id] : use_case_to_asset_id) {
      DeviceUseCase use_case{DeviceCategory::kCpu, use_case_name};

      builder.Add(asset_id,
                  OnDemandComponent(public_key_prefix + asset_id, version));
      builder.Add(asset_id + "_base_model",
                  BaseModelRecipe(
                      FileReference(asset_id, "weights.bin"),
                      BaseModelRecipeArgs(
                          proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                          proto::BaseModelRecipe::PERFORMANCE_HINT_UNSPECIFIED,
                          {}, 100)));
      builder.Add(asset_id + "_solution",
                  SolutionRecipe(asset_id + "_base_model", "",
                                 FileReference(asset_id, "config.pb")));
      builder.Add(use_case, asset_id + "_solution");
    }

    auto manifest_or = Manifest::Create(builder.Build(), DeviceCategory::kCpu);
    EXPECT_TRUE(manifest_or.has_value());
    return std::move(*manifest_or);
  }

  void CreateManager(Manifest manifest) {
    manager_ = std::make_unique<ManifestAssetManager>(
        &local_state_, *usage_tracker_, component_state_.CreateDelegate(),
        std::move(manifest));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<UsageTracker> usage_tracker_;
  TestManifestAssetManagerComponentState component_state_;
  std::unique_ptr<ManifestAssetManager> manager_;
};

TEST_F(ManifestAssetManagerTest, RegistersComponentsForActiveUseCases) {
  CreateManager(
      CreateDummyManifest({{"compose", "asset_1"}, {"test", "asset_2"}}));

  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));
  EXPECT_TRUE(component_state_.WasOnDemandUpdateRequested("dummy_key_asset_1"));
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_2"));
  EXPECT_FALSE(
      component_state_.WasOnDemandUpdateRequested("dummy_key_asset_2"));
}

// Test that the manager registers components for feature usage keyed on
// mojom::OnDeviceFeature.
TEST_F(ManifestAssetManagerTest, RegistersComponentsForLegacyFeatureUsage) {
  usage_tracker_->OnDeviceEligibleFeatureUsed(mojom::OnDeviceFeature::kTest);
  CreateManager(CreateDummyManifest({{"test", "asset_1"}}));

  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));
  EXPECT_TRUE(component_state_.WasOnDemandUpdateRequested("dummy_key_asset_1"));
}

TEST_F(ManifestAssetManagerTest, DynamicEnterprisePolicyChange) {
  CreateManager(CreateDummyManifest({{"compose", "asset_1"}}));

  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));

  // Disable enterprise policy.
  local_state_.SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::
                  kDisallowed));
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_1"));

  // Enabling the policy should trigger registration.
  local_state_.SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed));
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));
}

TEST_F(ManifestAssetManagerTest, DynamicOnDeviceAISettingsChange) {
  CreateManager(CreateDummyManifest({{"compose", "asset_1"}}));
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));

  // Disable user setting.
  local_state_.SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_1"));

  // Enable user setting.
  local_state_.SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled, true);
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenRunningOutfOfDiskSpace) {
  CreateManager(CreateDummyManifest({{"compose", "asset_1"}}));
  // Installs normally when there is enough disk space.
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));

  // 5gb is the default in
  // `IsFreeDiskSpaceTooLowForOnDeviceModelInstall`.
  component_state_.SetFreeDiskSpace(base::GiB(5) - base::ByteCount(1));
  // Fast forward to trigger the disk space evaluation.
  task_environment_.FastForwardBy(base::Seconds(11));

  // Trigger an update registration to re-evaluate the disk space.
  manager_->UpdateManifest(CreateDummyManifest({{"compose", "asset_1"}}));

  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_1"));
}

TEST_F(ManifestAssetManagerTest, AlreadyInstalledFlow) {
  component_state_.SetAlreadyInstalled("dummy_key_asset_1");

  CreateManager(CreateDummyManifest({{"compose", "asset_1"}}));

  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));
  // Because it was already installed, it shouldn't request an on-demand update.
  EXPECT_FALSE(
      component_state_.WasOnDemandUpdateRequested("dummy_key_asset_1"));
}

TEST_F(ManifestAssetManagerTest, SimulatesAssetReady) {
  CreateManager(CreateDummyManifest({{"compose", "asset_1"}}));

  EXPECT_FALSE(manager_->GetInstallDirectory("asset_1").has_value());

  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_1"));

  base::FilePath fake_path(FILE_PATH_LITERAL("/fake/path"));
  component_state_.SimulateComponentReady("dummy_key_asset_1",
                                          base::Version("1.0"), fake_path);

  auto path = manager_->GetInstallDirectory("asset_1");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(path.value(), fake_path);
}

// Parameterized test for scenarios where no components are installed upon
// asset manager creation.
struct InitializationTestParams {
  std::string test_name;
  base::FunctionRef<void(TestingPrefServiceSimple&,
                         TestManifestAssetManagerComponentState&,
                         UsageTracker&,
                         base::test::TaskEnvironment&)>
      setup_func;
};

class ManifestAssetManagerInitializationTest
    : public ManifestAssetManagerTest,
      public testing::WithParamInterface<InitializationTestParams> {};

TEST_P(ManifestAssetManagerInitializationTest, DoesNotInstall) {
  const auto& params = GetParam();

  params.setup_func(local_state_, component_state_, *usage_tracker_,
                    task_environment_);

  CreateManager(CreateDummyManifest({{"compose", "asset_1"}}));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_1"));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManifestAssetManagerInitializationTest,
    testing::Values(
        InitializationTestParams{
            .test_name = "DisabledByEnterprisePolicy",
            .setup_func =
                [](TestingPrefServiceSimple& local_state_,
                   TestManifestAssetManagerComponentState& component_state_,
                   UsageTracker& usage_tracker_,
                   base::test::TaskEnvironment& task_environment_) {
                  local_state_.SetInteger(
                      model_execution::prefs::localstate::
                          kGenAILocalFoundationalModelEnterprisePolicySettings,
                      std::to_underlying(
                          model_execution::prefs::
                              GenAILocalFoundationalModelEnterprisePolicySettings::
                                  kDisallowed));
                }},
        InitializationTestParams{
            .test_name = "DisabledByOnDeviceAIUserSetting",
            .setup_func =
                [](TestingPrefServiceSimple& local_state_,
                   TestManifestAssetManagerComponentState& component_state_,
                   UsageTracker& usage_tracker_,
                   base::test::TaskEnvironment& task_environment_) {
                  local_state_.SetBoolean(model_execution::prefs::localstate::
                                              kOnDeviceAiUserSettingsEnabled,
                                          false);
                }},
        InitializationTestParams{
            .test_name = "NotEnoughDiskSpace",
            .setup_func =
                [](TestingPrefServiceSimple& local_state_,
                   TestManifestAssetManagerComponentState& component_state_,
                   UsageTracker& usage_tracker_,
                   base::test::TaskEnvironment& task_environment_) {
                  // 20gb is the default in
                  // `IsFreeDiskSpaceSufficientForOnDeviceModelInstall`.
                  component_state_.SetFreeDiskSpace(base::GiB(20) -
                                                    base::ByteCount(1));
                }},
        InitializationTestParams{
            .test_name = "EligibleUseCaseUseTooOld",
            .setup_func =
                [](TestingPrefServiceSimple& local_state_,
                   TestManifestAssetManagerComponentState& component_state_,
                   UsageTracker& usage_tracker_,
                   base::test::TaskEnvironment& task_environment_) {
                  task_environment_.FastForwardBy(base::Days(31));
                }},
        InitializationTestParams{
            .test_name = "NoEligibleUseCaseUse",
            .setup_func =
                [](TestingPrefServiceSimple& local_state_,
                   TestManifestAssetManagerComponentState& component_state_,
                   UsageTracker& usage_tracker_,
                   base::test::TaskEnvironment& task_environment_) {
                  local_state_.ClearPref(
                      model_execution::prefs::localstate::kLastUsageByFeature);
                }}),
    [](const testing::TestParamInfo<InitializationTestParams>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace optimization_guide
