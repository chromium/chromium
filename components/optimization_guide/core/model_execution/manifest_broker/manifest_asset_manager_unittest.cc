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
  std::string public_key;
  std::string version = "1.0.0.0";

  static DummyAsset For(std::string use_case) {
    return {
        .use_case = use_case,
        .asset_id = "asset_" + use_case,
        .public_key = "dummy_key_" + use_case,
    };
  }

  DummyAsset WithVersion(std::string new_version) const {
    DummyAsset copy = *this;
    copy.version = std::move(new_version);
    return copy;
  }

  DummyAsset WithPublicKey(std::string new_public_key) const {
    DummyAsset copy = *this;
    copy.public_key = std::move(new_public_key);
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
      builder.Add(asset.asset_id,
                  OnDemandComponent(asset.public_key, asset.version));
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

  void MakeAssetInstallable(const DummyAsset& asset) {
    auto base_model_asset = std::make_unique<FakeBaseModelAsset>();
    base_model_asset->set_version(asset.version);
    component_state_.UpdateBaseModel(asset.public_key, *base_model_asset);
    base_model_assets_.push_back(std::move(base_model_asset));
  }

  void SetupReadyComponents(const DummyManifest& dummy_manifest) {
    // Ensure all manifest components are installable.
    for (const auto& asset : dummy_manifest.assets()) {
      MakeAssetInstallable(asset);
    }
    CreateManager(dummy_manifest.Build());
    for (const auto& asset : dummy_manifest.assets()) {
      EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  ModelBrokerPrefService local_state_;
  // Defer cleanup of these assets until the end of the test.
  std::vector<std::unique_ptr<FakeBaseModelAsset>> base_model_assets_;
  TestManifestAssetManagerComponentState component_state_;
  // ManifestBrokerState pieces:
  UsageTracker usage_tracker_{&local_state_.local_state()};
  std::unique_ptr<ManifestAssetManager::Delegate> delegate_;
  std::unique_ptr<ManifestAssetManager> manager_;
};

TEST_F(ManifestAssetManagerTest, RegistersComponentsForActiveUseCases) {
  DummyAsset compose_asset = DummyAsset::For("compose");
  DummyAsset test_asset = DummyAsset::For("test");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(compose_asset.use_case);
  CreateManager(DummyManifest().Add(compose_asset).Add(test_asset).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration(compose_asset.public_key));
  EXPECT_TRUE(
      component_state_.WasOnDemandUpdateRequested(compose_asset.public_key));
  EXPECT_FALSE(component_state_.IsRegistered(test_asset.public_key));
  EXPECT_FALSE(
      component_state_.WasOnDemandUpdateRequested(test_asset.public_key));
}

// Test that the manager registers components for feature usage keyed on
// mojom::OnDeviceFeature.
TEST_F(ManifestAssetManagerTest, RegistersComponentsForLegacyFeatureUsage) {
  DummyAsset asset = DummyAsset::For("test");
  usage_tracker_.OnDeviceEligibleFeatureUsed(mojom::OnDeviceFeature::kTest);

  CreateManager(DummyManifest().Add(asset).Build());

  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));
  EXPECT_TRUE(component_state_.WasOnDemandUpdateRequested(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DynamicEnterprisePolicyChange) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  CreateManager(DummyManifest().Add(asset).Build());

  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));

  // Disable enterprise policy.
  local_state_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::
                  kDisallowed));
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));

  // Enabling the policy should trigger registration.
  local_state_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed));
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DynamicOnDeviceAISettingsChange) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  CreateManager(DummyManifest().Add(asset).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));

  // Disable user setting.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));

  // Enable user setting.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled, true);
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, AlreadyInstalledFlow) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  MakeAssetInstallable(asset);
  CreateManager(DummyManifest().Add(asset).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));
  // Because it was already installed, it shouldn't request an on-demand update.
  EXPECT_FALSE(component_state_.WasOnDemandUpdateRequested(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, NotYetInstalledFlow) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  CreateManager(DummyManifest().Add(asset).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, SimulatesAssetReady) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  CreateManager(DummyManifest().Add(asset).Build());

  EXPECT_FALSE(manager_->GetInstallDirectory(asset.asset_id).has_value());
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));

  MakeAssetInstallable(asset);

  auto path = manager_->GetInstallDirectory(asset.asset_id);
  ASSERT_TRUE(path.has_value());
}

TEST_F(ManifestAssetManagerTest, ResumesInstallationOnStartup) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  CreateManager(DummyManifest().Add(asset).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));

  // Restart manager. It should immediately load from the pref-based ledger
  // and trigger registration again.
  ResetManager();
  CreateManager(DummyManifest().Add(asset).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, UninstallOutOfRetentionOnStartup) {
  DummyAsset compose_asset = DummyAsset::For("compose");
  DummyAsset test_asset = DummyAsset::For("test");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(compose_asset.use_case);
  SetupReadyComponents(DummyManifest().Add(compose_asset));

  task_environment_.FastForwardBy(features::GetOnDeviceModelRetentionTime() +
                                  base::Days(1));
  usage_tracker_.OnDeviceEligibleUseCaseUsed(test_asset.use_case);
  ResetManager();
  CreateManager(DummyManifest().Add(compose_asset).Add(test_asset).Build());

  EXPECT_TRUE(component_state_.WaitForUninstall(compose_asset.public_key));
}

TEST_F(ManifestAssetManagerTest, ObsoleteVersionOnStartup) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithVersion("1.0.0.0");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithVersion("2.0.0.0");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset_v1.use_case);
  SetupReadyComponents(DummyManifest().Add(asset_v1));
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v1.public_key));
  component_state_.SimulateRestart();

  // Restart manager.
  ResetManager();
  CreateManager(DummyManifest().Add(asset_v2).Build());
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.public_key));
}

TEST_F(ManifestAssetManagerTest, ChangedPublicKeyOnStartup) {
  DummyAsset test_v1 = DummyAsset::For("test").WithPublicKey("key1");
  DummyAsset test_v2 = DummyAsset::For("test").WithPublicKey("key2");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(test_v1.use_case);
  SetupReadyComponents(DummyManifest().Add(test_v1));
  EXPECT_TRUE(component_state_.WaitForRegistration(test_v1.public_key));
  component_state_.SimulateRestart();

  // Restart manager.
  ResetManager();
  CreateManager(DummyManifest().Add(test_v2).Build());
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(component_state_.WaitForUninstall(test_v1.public_key));
  EXPECT_TRUE(component_state_.WaitForRegistration(test_v2.public_key));
}

TEST_F(ManifestAssetManagerTest, ChangedAssetIdOnStartup) {
  DummyAsset asset_v1 = DummyAsset::For("prompt_api").WithAssetId("asset1");
  DummyAsset asset_v2 = DummyAsset::For("prompt_api").WithAssetId("asset2");
  // These assets have the same public key and version.
  ASSERT_EQ(asset_v1.public_key, asset_v2.public_key);
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset_v1.use_case);
  SetupReadyComponents(DummyManifest().Add(asset_v1));
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v1.public_key));
  component_state_.SimulateRestart();

  // Restart manager.
  ResetManager();
  CreateManager(DummyManifest().Add(asset_v2).Build());
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  // Since the public key and version are the same, it should not trigger an
  // uninstall, and we should just consider it already installed.
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.public_key));
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset_v1.public_key));
}

TEST_F(ManifestAssetManagerTest, ReRegistersWhenTargetVersionUpdated) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithVersion("1.0.0.0");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithVersion("2.0.0.0");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset_v1.use_case);
  SetupReadyComponents(DummyManifest().Add(asset_v1));
  component_state_.SimulateRestart();

  // Trigger update manifest with a new version.
  manager_->UpdateManifest(DummyManifest().Add(asset_v2).Build());
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.public_key));
}

TEST_F(ManifestAssetManagerTest,
       ReRegistersWhenVersionUpdatedWhileRegistering) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithVersion("1.0.0.0");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithVersion("2.0.0.0");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset_v1.use_case);
  component_state_.SetDeferRegistrationCallbacks(true);
  CreateManager(DummyManifest().Add(asset_v1).Build());
  component_state_.SimulateRestart();

  // Update manifest with new version while the first registration is pending.
  manager_->UpdateManifest(DummyManifest().Add(asset_v2).Build());

  // Complete the deferred callback for version 1.0.
  component_state_.RunPendingRegistrations();

  // Register again for the new version.
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.public_key));
}

TEST_F(ManifestAssetManagerTest, KeepInstalledWhenAssetRenamed) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithAssetId("asset_1");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithAssetId("asset_2");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset_v1.use_case);
  SetupReadyComponents(DummyManifest().Add(asset_v1));
  manager_->UpdateManifest(DummyManifest().Add(asset_v2).Build());
  // When only the asset id is changed, we should consider it already installed.
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.public_key));
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset_v1.public_key));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenPublicKeyChanged) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithPublicKey("key1");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithPublicKey("key2");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset_v1.use_case);
  SetupReadyComponents(DummyManifest().Add(asset_v1));
  manager_->UpdateManifest(DummyManifest().Add(asset_v2).Build());
  EXPECT_TRUE(component_state_.WaitForUninstall(asset_v1.public_key));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenOutOfRetention) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  SetupReadyComponents(DummyManifest().Add(asset));
  task_environment_.FastForwardBy(features::GetOnDeviceModelRetentionTime() +
                                  base::Days(1));
  manager_->UpdateManifest(DummyManifest().Add(asset).Build());
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenRunningOutOfDiskSpace) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  SetupReadyComponents(DummyManifest().Add(asset));
  // 5gb is the default in `IsFreeDiskSpaceTooLowForOnDeviceModelInstall`.
  component_state_.SetFreeDiskSpace(base::GiB(5) - base::ByteCount(1));
  task_environment_.FastForwardBy(base::Seconds(11));
  manager_->UpdateManifest(DummyManifest().Add(asset).Build());
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenFeatureNotEnabled) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kOptimizationGuideModelExecution);
  ResetManager();
  CreateManager(DummyManifest().Add(asset).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, UninstallWhileRegistrationPending) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  component_state_.SetDeferRegistrationCallbacks(true);
  CreateManager(DummyManifest().Add(asset).Build());

  // Verify that it is currently registering.
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));

  // Feature is disabled while registration is pending.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);

  // Uninstall is queued and triggered once registration is complete.
  component_state_.RunPendingRegistrations();
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, RegisterWhileUninstallPending) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  // 1. The component is already installed.
  SetupReadyComponents(DummyManifest().Add(asset));

  // 2. Trigger uninstall.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));

  // 3. Re-enable and verify it eventually installs again.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled, true);
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, RemainsInstalledWhenReferencedInManifest) {
  DummyAsset asset_compose = DummyAsset::For("compose").WithAssetId("asset_1");
  DummyAsset asset_test = DummyAsset::For("test").WithAssetId("asset_1");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset_compose.use_case);
  SetupReadyComponents(DummyManifest().Add(asset_compose));
  // compose no longer requires asset_1, but Test does (which isn't used).
  manager_->UpdateManifest(DummyManifest().Add(asset_test).Build());
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset_test.public_key));
}

TEST_F(ManifestAssetManagerTest, AssetRemainsInstalledWhileNotRequested) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  SetupReadyComponents(DummyManifest().Add(asset));
  // Clear usage prefs so that the model is no longer eligible for download.
  local_state_.local_state().ClearPref(
      model_execution::prefs::localstate::kLastUsageByFeature);

  // Trigger an update manifest to re-evaluate the registration.
  manager_->UpdateManifest(DummyManifest().Add(asset).Build());

  // Should not uninstall.
  EXPECT_TRUE(manager_->GetInstallDirectory(asset.asset_id).has_value());
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenDisabledByEnterprisePolicy) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  local_state_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::
                  kDisallowed));

  CreateManager(DummyManifest().Add(asset).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.public_key));
}

TEST_F(ManifestAssetManagerTest,
       DoesNotInstallWhenDisabledByOnDeviceAIUserSetting) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);

  CreateManager(DummyManifest().Add(asset).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenNotEnoughDiskSpace) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  // 20gb is the default in `IsFreeDiskSpaceSufficientForOnDeviceModelInstall`.
  component_state_.SetFreeDiskSpace(base::GiB(20) - base::ByteCount(1));

  CreateManager(DummyManifest().Add(asset).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenEligibleUseCaseUseTooOld) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  task_environment_.FastForwardBy(base::Days(31));

  CreateManager(DummyManifest().Add(asset).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenNoEligibleUseCaseUse) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.OnDeviceEligibleUseCaseUsed(asset.use_case);
  local_state_.local_state().ClearPref(
      model_execution::prefs::localstate::kLastUsageByFeature);

  CreateManager(DummyManifest().Add(asset).Build());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.public_key));
}

}  // namespace
}  // namespace optimization_guide
