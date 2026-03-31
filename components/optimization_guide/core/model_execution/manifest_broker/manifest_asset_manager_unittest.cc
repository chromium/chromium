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
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

// Helper to build a manifest for testing.
// Each asset added to this manifest will be associated with
// DeviceCategory::kCpu and will have its own unique recipe chain (BaseModel ->
// Solution) to avoid identifier conflicts and ensure each use case points to a
// valid solution.
struct DummyAsset {
  std::string use_case;
  std::string asset_id;
  std::string key_prefix = "dummy_key_";
  std::string version = "1.0.0.0";

  static DummyAsset For(std::string use_case) {
    return {
        .use_case = use_case,
        .asset_id = "asset_" + use_case,
    };
  }

  DummyAsset WithVersion(std::string new_version) const {
    DummyAsset copy = *this;
    copy.version = std::move(new_version);
    return copy;
  }

  DummyAsset WithPublicKey(std::string new_key_prefix) const {
    DummyAsset copy = *this;
    copy.key_prefix = std::move(new_key_prefix);
    return copy;
  }

  DummyAsset WithAssetId(std::string new_asset_id) const {
    DummyAsset copy = *this;
    copy.asset_id = std::move(new_asset_id);
    return copy;
  }
};

class DummyManifest {
 public:
  DummyManifest() = default;

  DummyManifest& Add(DummyAsset asset) {
    assets_.push_back(std::move(asset));
    return *this;
  }

  Manifest Build() const {
    ManifestBuilder builder;
    for (const auto& asset : assets_) {
      DeviceUseCase use_case{DeviceCategory::kCpu, asset.use_case};
      builder.Add(
          asset.asset_id,
          OnDemandComponent(asset.key_prefix + asset.asset_id, asset.version));
      builder.Add(asset.asset_id + "_base_model",
                  BaseModelRecipe(
                      FileReference(asset.asset_id, "weights.bin"),
                      BaseModelRecipeArgs(
                          proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                          proto::BaseModelRecipe::PERFORMANCE_HINT_UNSPECIFIED,
                          {}, 100)));
      builder.Add(asset.asset_id + "_solution",
                  SolutionRecipe(asset.asset_id + "_base_model", "",
                                 FileReference(asset.asset_id, "config.pb")));
      builder.Add(use_case, asset.asset_id + "_solution");
    }
    auto manifest_or = Manifest::Create(base::FilePath(), builder.Build(),
                                        DeviceCategory::kCpu);
    return std::move(*manifest_or);
  }

  const std::vector<DummyAsset>& assets() const { return assets_; }

 private:
  std::vector<DummyAsset> assets_;
};

class ManifestAssetManagerTest : public testing::Test {
 public:
  ManifestAssetManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationGuideModelExecution,
         features::kOptimizationGuideOnDeviceModel},
        {});
  }

  void CreateManager(Manifest manifest) {
    delegate_ = component_state_.CreateDelegate();
    manager_ = std::make_unique<ManifestAssetManager>(
        local_state_.local_state(), usage_tracker_, *delegate_,
        std::move(manifest));
  }

  void ResetManager() {
    manager_.reset();
    delegate_.reset();
  }

  void SetupReadyComponents(const DummyManifest& dummy_manifest) {
    CreateManager(dummy_manifest.Build());
    for (const auto& asset : dummy_manifest.assets()) {
      EXPECT_TRUE(component_state_.WaitForRegistration(asset.key_prefix +
                                                       asset.asset_id));
      component_state_.SimulateComponentReady(
          asset.key_prefix + asset.asset_id, base::Version(asset.version),
          base::FilePath(FILE_PATH_LITERAL("/fake/path")));
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  ModelBrokerPrefService local_state_;
  UsageTracker usage_tracker_{&local_state_.local_state()};
  TestManifestAssetManagerComponentState component_state_;
  std::unique_ptr<ManifestAssetManager::Delegate> delegate_;
  std::unique_ptr<ManifestAssetManager> manager_;
};

TEST_F(ManifestAssetManagerTest, RegistersComponentsForActiveUseCases) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  CreateManager(DummyManifest()
                    .Add(DummyAsset::For("compose"))
                    .Add(DummyAsset::For("test"))
                    .Build());
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
  EXPECT_TRUE(
      component_state_.WasOnDemandUpdateRequested("dummy_key_asset_compose"));
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_test"));
  EXPECT_FALSE(
      component_state_.WasOnDemandUpdateRequested("dummy_key_asset_test"));
}

// Test that the manager registers components for feature usage keyed on
// mojom::OnDeviceFeature.
TEST_F(ManifestAssetManagerTest, RegistersComponentsForLegacyFeatureUsage) {
  usage_tracker_.OnDeviceEligibleFeatureUsed(mojom::OnDeviceFeature::kTest);

  CreateManager(DummyManifest().Add(DummyAsset::For("test")).Build());

  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_test"));
  EXPECT_TRUE(
      component_state_.WasOnDemandUpdateRequested("dummy_key_asset_test"));
}

TEST_F(ManifestAssetManagerTest, DynamicEnterprisePolicyChange) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());

  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));

  // Disable enterprise policy.
  local_state_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::
                  kDisallowed));
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_compose"));

  // Enabling the policy should trigger registration.
  local_state_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed));
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, DynamicOnDeviceAISettingsChange) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));

  // Disable user setting.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_compose"));

  // Enable user setting.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled, true);
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, AlreadyInstalledFlow) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  component_state_.SetAlreadyInstalled("dummy_key_asset_compose");
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
  // Because it was already installed, it shouldn't request an on-demand update.
  EXPECT_FALSE(
      component_state_.WasOnDemandUpdateRequested("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, NotYetInstalledFlow) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, SimulatesAssetReady) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());

  EXPECT_FALSE(manager_->GetInstallDirectory("asset_compose").has_value());
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));

  base::FilePath fake_path(FILE_PATH_LITERAL("/fake/path"));
  component_state_.SimulateComponentReady("dummy_key_asset_compose",
                                          base::Version("1.0"), fake_path);

  auto path = manager_->GetInstallDirectory("asset_compose");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(path.value(), fake_path);
}

TEST_F(ManifestAssetManagerTest, ResumesInstallationOnStartup) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));

  // Restart manager. It should immediately load from the pref-based ledger
  // and trigger registration again.
  ResetManager();
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, UninstallOutOfRetentionOnStartup) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  SetupReadyComponents(DummyManifest().Add(DummyAsset::For("compose")));

  task_environment_.FastForwardBy(features::GetOnDeviceModelRetentionTime() +
                                  base::Days(1));
  usage_tracker_.OnDeviceEligibleUseCaseUsed("test");
  ResetManager();
  CreateManager(DummyManifest()
                    .Add(DummyAsset::For("compose"))
                    .Add(DummyAsset::For("test"))
                    .Build());

  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, ObsoleteVersionOnStartup) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  DummyAsset compose_v1 = DummyAsset::For("compose").WithVersion("1.0.0.0");
  DummyAsset compose_v2 = DummyAsset::For("compose").WithVersion("2.0.0.0");
  SetupReadyComponents(DummyManifest().Add(compose_v1));
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
  component_state_.ClearRegistered();

  // Restart manager.
  ResetManager();
  CreateManager(DummyManifest().Add(compose_v2).Build());
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, ChangedPublicKeyOnStartup) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("test");
  DummyAsset test_v1 = DummyAsset::For("test").WithPublicKey("key1");
  DummyAsset test_v2 = DummyAsset::For("test").WithPublicKey("key2");
  SetupReadyComponents(DummyManifest().Add(test_v1));
  EXPECT_TRUE(component_state_.WaitForRegistration("key1asset_test"));
  component_state_.ClearRegistered();

  // Restart manager.
  ResetManager();
  CreateManager(DummyManifest().Add(test_v2).Build());
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(component_state_.WaitForUninstall("key1asset_test"));
  EXPECT_TRUE(component_state_.WaitForRegistration("key2asset_test"));
}

TEST_F(ManifestAssetManagerTest, ChangedAssetIdOnStartup) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("prompt_api");
  DummyAsset prompt_v1 = DummyAsset::For("prompt_api").WithAssetId("asset1");
  DummyAsset prompt_v2 = DummyAsset::For("prompt_api").WithAssetId("asset2");
  SetupReadyComponents(DummyManifest().Add(prompt_v1));
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset1"));
  component_state_.ClearRegistered();

  // Restart manager.
  ResetManager();
  CreateManager(DummyManifest().Add(prompt_v2).Build());
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset1"));
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset2"));
}

TEST_F(ManifestAssetManagerTest, ReRegistersWhenTargetVersionUpdated) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  SetupReadyComponents(
      DummyManifest().Add(DummyAsset::For("compose").WithVersion("1.0.0.0")));
  component_state_.ClearRegistered();

  // Trigger update manifest with a new version.
  manager_->UpdateManifest(
      DummyManifest()
          .Add(DummyAsset::For("compose").WithVersion("2.0.0.0"))
          .Build());
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest,
       ReRegistersWhenVersionUpdatedWhileRegistering) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  component_state_.SetDeferRegistrationCallbacks(true);
  CreateManager(DummyManifest()
                    .Add(DummyAsset::For("compose").WithVersion("1.0.0.0"))
                    .Build());
  component_state_.ClearRegistered();

  // Update manifest with new version while the first registration is pending.
  manager_->UpdateManifest(
      DummyManifest()
          .Add(DummyAsset::For("compose").WithVersion("2.0.0.0"))
          .Build());

  // Complete the deferred callback for version 1.0.
  component_state_.RunPendingRegistrations();

  // Register again for the new version.
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenAssetObsoleted) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  SetupReadyComponents(
      DummyManifest().Add(DummyAsset::For("compose").WithAssetId("asset_1")));
  manager_->UpdateManifest(
      DummyManifest()
          .Add(DummyAsset::For("compose").WithAssetId("asset_2"))
          .Build());
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_1"));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenPublicKeyChanged) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  SetupReadyComponents(DummyManifest().Add(DummyAsset::For("compose")));
  manager_->UpdateManifest(
      DummyManifest()
          .Add(DummyAsset::For("compose").WithPublicKey("new_key_"))
          .Build());
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenOutOfRetention) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  SetupReadyComponents(DummyManifest().Add(DummyAsset::For("compose")));
  task_environment_.FastForwardBy(features::GetOnDeviceModelRetentionTime() +
                                  base::Days(1));
  manager_->UpdateManifest(
      DummyManifest().Add(DummyAsset::For("compose")).Build());
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenRunningOutOfDiskSpace) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  SetupReadyComponents(DummyManifest().Add(DummyAsset::For("compose")));
  // 5gb is the default in `IsFreeDiskSpaceTooLowForOnDeviceModelInstall`.
  component_state_.SetFreeDiskSpace(base::GiB(5) - base::ByteCount(1));
  task_environment_.FastForwardBy(base::Seconds(11));
  manager_->UpdateManifest(
      DummyManifest().Add(DummyAsset::For("compose")).Build());
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenFeatureNotEnabled) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kOptimizationGuideModelExecution);
  ResetManager();
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, UninstallWhileRegistrationPending) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  component_state_.SetDeferRegistrationCallbacks(true);
  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());

  // Verify that it is currently registering.
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));

  // Feature is disabled while registration is pending.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);

  // Uninstall is queued and triggered once registration is complete.
  component_state_.RunPendingRegistrations();
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, RegisterWhileUninstallPending) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  // 1. The component is already installed.
  SetupReadyComponents(DummyManifest().Add(DummyAsset::For("compose")));

  // 2. Trigger uninstall.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);
  EXPECT_TRUE(component_state_.WaitForUninstall("dummy_key_asset_compose"));

  // 3. Re-enable and verify it eventually installs again.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled, true);
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(component_state_.WaitForRegistration("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, RemainsInstalledWhenReferencedInManifest) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  SetupReadyComponents(
      DummyManifest().Add(DummyAsset::For("compose").WithAssetId("asset_1")));
  // compose no longer requires asset_1, but Test does (which isn't used).
  manager_->UpdateManifest(
      DummyManifest()
          .Add(DummyAsset::For("test").WithAssetId("asset_1"))
          .Build());
  EXPECT_FALSE(component_state_.WasUninstallRequested("dummy_key_asset_1"));
}

TEST_F(ManifestAssetManagerTest, AssetRemainsInstalledWhileNotRequested) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  SetupReadyComponents(DummyManifest().Add(DummyAsset::For("compose")));
  // Clear usage prefs so that the model is no longer eligible for download.
  local_state_.local_state().ClearPref(
      model_execution::prefs::localstate::kLastUsageByFeature);

  // Trigger an update manifest to re-evaluate the registration.
  manager_->UpdateManifest(
      DummyManifest().Add(DummyAsset::For("compose")).Build());

  // Should not uninstall.
  EXPECT_TRUE(manager_->GetInstallDirectory("asset_compose").has_value());
  EXPECT_FALSE(
      component_state_.WasUninstallRequested("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenDisabledByEnterprisePolicy) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  local_state_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::
                  kDisallowed));

  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest,
       DoesNotInstallWhenDisabledByOnDeviceAIUserSetting) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);

  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenNotEnoughDiskSpace) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  // 20gb is the default in `IsFreeDiskSpaceSufficientForOnDeviceModelInstall`.
  component_state_.SetFreeDiskSpace(base::GiB(20) - base::ByteCount(1));

  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenEligibleUseCaseUseTooOld) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  task_environment_.FastForwardBy(base::Days(31));

  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_compose"));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenNoEligibleUseCaseUse) {
  usage_tracker_.OnDeviceEligibleUseCaseUsed("compose");
  local_state_.local_state().ClearPref(
      model_execution::prefs::localstate::kLastUsageByFeature);

  CreateManager(DummyManifest().Add(DummyAsset::For("compose")).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered("dummy_key_asset_compose"));
}

}  // namespace
}  // namespace optimization_guide
