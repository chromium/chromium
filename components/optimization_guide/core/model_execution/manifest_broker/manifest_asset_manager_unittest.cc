// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "components/crx_file/id_util.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_asset_manager.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"
#include "components/optimization_guide/core/model_execution/test/mock_download_progress_observer.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using UnavailableReason = std::optional<mojom::ModelUnavailableReason>;
using CanCreateSessionFuture = base::test::TestFuture<
    std::optional<mojom::ModelUnavailableReason>,
    std::optional<mojom::ModelNotSupportedDetailedReason>>;

// Helper to build a manifest for testing.
// Each asset added to this manifest will be associated with
// DeviceCategory::kGpuHighTier and will have its own unique recipe chain
// (BaseModel -> Solution) to avoid identifier conflicts and ensure each use
// case points to a valid solution.
struct DummyAsset {
  std::string use_case;
  std::string asset_id;
  std::string public_key;
  std::string version = "1.0.0.0";
  bool background_download = false;

  TestManifestAssetManagerComponentState::InstallTarget ToInstallTarget()
      const {
    return TestManifestAssetManagerComponentState::InstallTarget(
        public_key, base::Version(version));
  }

  static DummyAsset For(std::string use_case) {
    std::string key = "dummy_key_" + use_case;
    key.resize(32, '0');
    return {
        .use_case = use_case,
        .asset_id = "asset_" + use_case,
        .public_key = base::HexEncode(base::as_byte_span(key)),
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

  DummyAsset WithBackgroundDownload(bool bg) const {
    DummyAsset copy = *this;
    copy.background_download = bg;
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

  std::unique_ptr<ManifestComponentDirectory> BuildAsset() const {
    ManifestBuilder builder;
    for (const auto& asset : assets_) {
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
                                 FileReference("manifest", "config.pb")));
      builder.Add(DeviceUseCase{DeviceCategory::kGpuHighTier, asset.use_case},
                  asset.asset_id + "_solution");
    }

    proto::Manifest manifest = builder.Build();
    for (const auto& asset : assets_) {
      if (asset.background_download) {
        auto& category_config =
            (*manifest.mutable_category_configs())["gpu_high_tier"];
        auto& use_case_config =
            (*category_config.mutable_use_cases())[asset.use_case];
        use_case_config.set_background_download(true);
      }
    }

    auto component =
        std::make_unique<ManifestComponentDirectory>(std::move(manifest));
    component->Add("config.pb", proto::SolutionConfig());
    return component;
  }

  const std::vector<DummyAsset>& assets() const { return assets_; }

 private:
  std::vector<DummyAsset> assets_;
};

class ManifestAssetManagerTest : public testing::Test {
 public:
  ManifestAssetManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationGuideModelExecution},
        {features::kAIModelUnloadableProgress});
  }

  void UpdateManifest(const DummyManifest& manifest) {
    component_state_.UpdateManifest(manifest.BuildAsset());
  }

  void MakeAssetInstallable(const DummyAsset& asset) {
    auto base_model_asset = std::make_unique<FakeBaseModelAsset>();
    base_model_asset->set_version(asset.version);
    component_state_.UpdateBaseModel(asset.public_key,
                                     std::move(base_model_asset));
  }

  void MakeAssetsInstallable(const DummyManifest& dummy_manifest) {
    // Ensure all manifest components are installable.
    for (const auto& asset : dummy_manifest.assets()) {
      MakeAssetInstallable(asset);
    }
    UpdateManifest(dummy_manifest);
  }

  void SetUp() override {
    testing::Test::SetUp();
    EXPECT_CALL(component_update_service_,
                GetComponentDetails(testing::_, testing::_))
        .WillRepeatedly([&](const std::string& id,
                            update_client::CrxUpdateItem* item) {
          auto iter = fake_components_.find(id);
          if (iter == fake_components_.end()) {
            return false;
          }

          if (iter->second.downloaded_bytes() == iter->second.total_bytes()) {
            *item = iter->second.CreateUpdateItem(
                update_client::ComponentState::kUpdated,
                iter->second.total_bytes());
          } else {
            *item = iter->second.CreateUpdateItem(
                update_client::ComponentState::kNew, 0);
          }

          return true;
        });
  }

  void Startup() {
    manifest_broker_state_ = std::make_unique<ManifestBrokerState>(
        local_state_.local_state(), component_state_.CreateDelegate(),
        fake_launcher_.LaunchFn(), &component_update_service_);
    model_broker_client_ = std::make_unique<ModelBrokerClient>(
        manifest_broker_state_->BindAndPassRemoteBroker(), nullptr);
    // Bind a subscriber to trigger initialization.
    model_broker_client_->GetSubscriber(mojom::OnDeviceFeature::kTest);
  }

  std::string GetCrxId(const DummyAsset& asset) {
    std::vector<uint8_t> public_key_hash;
    CHECK(base::HexStringToBytes(asset.public_key, &public_key_hash));
    return crx_file::id_util::GenerateIdFromHash(public_key_hash);
  }

  void RegisterFakeComponent(const std::string& id, uint64_t total_bytes) {
    fake_components_.insert({id, FakeComponent(id, total_bytes)});
  }

  void SendUpdate(const std::string& id, uint64_t downloaded_bytes) {
    auto iter = fake_components_.find(id);
    ASSERT_NE(iter, fake_components_.end());
    component_update_service_.SendUpdate(iter->second.CreateUpdateItem(
        update_client::ComponentState::kDownloading, downloaded_bytes));
  }

  void SimulateShutdown() {
    model_broker_client_.reset();
    manifest_broker_state_.reset();
    component_state_.SimulateRestart();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<FakeComponentUpdateService> component_update_service_;
  std::map<std::string, FakeComponent> fake_components_;
  ModelBrokerPrefService local_state_;
  TestManifestAssetManagerComponentState component_state_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  on_device_model::FakeServiceLauncher fake_launcher_{&fake_settings_};
  // ManifestBrokerState pieces:
  UsageTracker usage_tracker_{&local_state_.local_state()};
  std::unique_ptr<ManifestBrokerState> manifest_broker_state_;
  std::unique_ptr<ModelBrokerClient> model_broker_client_;
};

TEST_F(ManifestAssetManagerTest, DownloadProgressObserverReceivesUpdates) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  MockDownloadProgressObserver observer;
  model_broker_client_->AddModelDownloadProgressObserver(
      asset.use_case, observer.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();

  RegisterFakeComponent(GetCrxId(asset), 100);

  // Send the zero update.
  SendUpdate(GetCrxId(asset), 0);
  observer.ExpectReceivedNormalizedUpdate(0, 100);

  // Send an update for 50 downloaded bytes.
  task_environment_.FastForwardBy(base::Milliseconds(51));
  SendUpdate(GetCrxId(asset), 50);
  observer.ExpectReceivedNormalizedUpdate(50, 100);
}

TEST_F(ManifestAssetManagerTest,
       DownloadProgressObserverAppliesUnloadableProgressPercent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAIModelUnloadableProgress,
      {{"ai_model_unloadable_progress_percent", "10"}});

  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  MockDownloadProgressObserver observer;
  model_broker_client_->AddModelDownloadProgressObserver(
      asset.use_case, observer.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();  // nocheck

  // 900 actual component bytes with 10% holdback = 1000 total virtual bytes.
  RegisterFakeComponent(GetCrxId(asset), 900);

  // Send the zero update.
  SendUpdate(GetCrxId(asset), 0);
  observer.ExpectReceivedNormalizedUpdate(0, 1000);

  // Send an update for 450 downloaded bytes (45% of virtual total).
  task_environment_.FastForwardBy(base::Milliseconds(51));
  SendUpdate(GetCrxId(asset), 450);
  observer.ExpectReceivedNormalizedUpdate(450, 1000);

  // Send an update for 900 downloaded bytes (all actual bytes downloaded =
  // 90%).
  task_environment_.FastForwardBy(base::Milliseconds(51));
  SendUpdate(GetCrxId(asset), 900);
  observer.ExpectReceivedNormalizedUpdate(900, 1000);
}

TEST_F(ManifestAssetManagerTest, DownloadProgressObserverIsUseCaseSpecific) {
  DummyAsset compose_asset = DummyAsset::For("compose");
  DummyAsset test_asset = DummyAsset::For("test");
  usage_tracker_.RaisePriority(compose_asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  usage_tracker_.RaisePriority(test_asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  UpdateManifest(DummyManifest().Add(compose_asset).Add(test_asset));
  Startup();
  EXPECT_TRUE(
      component_state_.WaitForRegistration(compose_asset.ToInstallTarget()));
  EXPECT_TRUE(
      component_state_.WaitForRegistration(test_asset.ToInstallTarget()));

  MockDownloadProgressObserver compose_observer;
  model_broker_client_->AddModelDownloadProgressObserver(
      compose_asset.use_case, compose_observer.BindNewPipeAndPassRemote());

  MockDownloadProgressObserver test_observer;
  model_broker_client_->AddModelDownloadProgressObserver(
      test_asset.use_case, test_observer.BindNewPipeAndPassRemote());
  task_environment_.RunUntilIdle();

  RegisterFakeComponent(GetCrxId(compose_asset), 100);
  RegisterFakeComponent(GetCrxId(test_asset), 200);

  // Send the zero update for compose component.
  test_observer.ExpectNoUpdate();
  SendUpdate(GetCrxId(compose_asset), 0);
  compose_observer.ExpectReceivedNormalizedUpdate(0, 100);

  // Send an update for compose component.
  task_environment_.FastForwardBy(base::Milliseconds(51));
  test_observer.ExpectNoUpdate();
  SendUpdate(GetCrxId(compose_asset), 50);
  compose_observer.ExpectReceivedNormalizedUpdate(50, 100);
}

TEST_F(ManifestAssetManagerTest, RegistersComponentsForActiveUseCases) {
  DummyAsset compose_asset = DummyAsset::For("compose");
  DummyAsset test_asset = DummyAsset::For("test");
  UpdateManifest(DummyManifest().Add(compose_asset).Add(test_asset));
  Startup();
  manifest_broker_state_->SetUseCaseRequested(compose_asset.use_case, true);
  EXPECT_TRUE(
      component_state_.WaitForRegistration(compose_asset.ToInstallTarget()));
  EXPECT_TRUE(
      component_state_.WasOnDemandUpdateRequested(compose_asset.public_key));
  EXPECT_FALSE(component_state_.IsRegistered(test_asset.public_key));
  EXPECT_FALSE(
      component_state_.WasOnDemandUpdateRequested(test_asset.public_key));
}

// Test that the manager registers components for feature usage keyed on
// mojom::OnDeviceFeature without triggering on-demand downloads.
TEST_F(ManifestAssetManagerTest, RegistersComponentsForLegacyFeatureUsage) {
  DummyAsset asset = DummyAsset::For("test");
  model_execution::prefs::RecordFeatureUsage(&local_state_.local_state(),
                                             mojom::OnDeviceFeature::kTest);

  UpdateManifest(DummyManifest().Add(asset));
  Startup();

  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  EXPECT_FALSE(component_state_.WasOnDemandUpdateRequested(asset.public_key));
}

TEST_F(ManifestAssetManagerTest,
       BackgroundPriorityDoesNotRequestOnDemandUpdate) {
  DummyAsset asset = DummyAsset::For("compose");
  model_execution::prefs::RecordUseCaseUsage(&local_state_.local_state(),
                                             asset.use_case);

  UpdateManifest(DummyManifest().Add(asset));
  Startup();

  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  EXPECT_FALSE(component_state_.WasOnDemandUpdateRequested(asset.public_key));

  // Upgrading to foreground requests on-demand update.
  manifest_broker_state_->SetUseCaseRequested(asset.use_case, true);
  EXPECT_TRUE(component_state_.WasOnDemandUpdateRequested(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DynamicEnterprisePolicyChange) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();

  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

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
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, DynamicOnDeviceAISettingsChange) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  // Disable user setting.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));

  // Enable user setting.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled, true);
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, AlreadyInstalledFlow) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));

  // First startup to install the asset.
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  SimulateShutdown();

  // Second startup. Now it is already installed.
  base::HistogramTester histogram_tester;
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  // Because it was already installed, it shouldn't request an on-demand update.
  EXPECT_FALSE(component_state_.WasOnDemandUpdateRequested(asset.public_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime.Unknown",
      true, 1);
}

TEST_F(ManifestAssetManagerTest, NotYetInstalledFlow) {
  base::HistogramTester histogram_tester;
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelInstalledAtRegistrationTime.Unknown",
      false, 1);
}

TEST_F(ManifestAssetManagerTest, SimulatesAssetReady) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  auto& subscriber =
      model_broker_client_->GetSubscriber(mojom::OnDeviceFeature::kCompose);

  CanCreateSessionFuture future;
  subscriber.CanCreateSession({}, future.GetCallback());
  EXPECT_EQ(future.Get<UnavailableReason>(),
            mojom::ModelUnavailableReason::kPendingAssets);

  base::HistogramTester histogram_tester;
  MakeAssetInstallable(asset);

  base::test::TestFuture<base::WeakPtr<ModelClient>> client_future;
  subscriber.WaitForClient(client_future.GetCallback());
  EXPECT_TRUE(client_future.Get());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OnDeviceModel.InstalledModel",
      static_cast<int>(
          OnDeviceBaseModel::kUnknown) /*OnDeviceBaseModel::kUnknown*/,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OnDeviceModel.NewModelInstalled",
      static_cast<int>(
          OnDeviceBaseModel::kUnknown) /*OnDeviceBaseModel::kUnknown*/,
      1);
}

TEST_F(ManifestAssetManagerTest, DoesNotLogNewInstallExistingComponent) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));

  {
    base::HistogramTester histogram_tester;
    Startup();
    EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.OnDeviceModel.NewModelInstalled",
        static_cast<int>(
            OnDeviceBaseModel::kUnknown) /*OnDeviceBaseModel::kUnknown*/,
        1);
    SimulateShutdown();
  }

  {
    base::HistogramTester histogram_tester;
    UpdateManifest(DummyManifest().Add(asset));
    Startup();
    EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.OnDeviceModel.NewModelInstalled",
        static_cast<int>(
            OnDeviceBaseModel::kUnknown) /*OnDeviceBaseModel::kUnknown*/,
        0);
  }
}

TEST_F(ManifestAssetManagerTest, ResumesInstallationOnStartup) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  SimulateShutdown();

  // Restart manager. It should immediately load from the pref-based ledger
  // and trigger registration again.
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, ObsoleteVersionOnStartup) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithVersion("1.0.0.0");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithVersion("2.0.0.0");
  usage_tracker_.RaisePriority(asset_v1.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset_v1));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v1.ToInstallTarget()));
  SimulateShutdown();

  UpdateManifest(DummyManifest().Add(asset_v2));
  Startup();
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, ChangedPublicKeyOnStartup) {
  DummyAsset test_v1 = DummyAsset::For("test").WithPublicKey("key1");
  DummyAsset test_v2 = DummyAsset::For("test").WithPublicKey("key2");
  usage_tracker_.RaisePriority(test_v1.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(test_v1));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(test_v1.ToInstallTarget()));
  SimulateShutdown();

  UpdateManifest(DummyManifest().Add(test_v2));
  Startup();
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_TRUE(component_state_.WaitForUninstall(test_v1.public_key));
  EXPECT_TRUE(component_state_.WaitForRegistration(test_v2.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, ChangedAssetIdOnStartup) {
  DummyAsset asset_v1 = DummyAsset::For("prompt_api").WithAssetId("asset1");
  DummyAsset asset_v2 = DummyAsset::For("prompt_api").WithAssetId("asset2");
  // These assets have the same public key and version.
  ASSERT_EQ(asset_v1.public_key, asset_v2.public_key);
  usage_tracker_.RaisePriority(asset_v1.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset_v1));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v1.ToInstallTarget()));
  SimulateShutdown();

  UpdateManifest(DummyManifest().Add(asset_v2));
  Startup();
  // Wait for the delayed uninstall task.
  task_environment_.FastForwardBy(base::Seconds(2));

  // Since the public key and version are the same, it should not trigger an
  // uninstall, and we should just consider it already installed.
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.ToInstallTarget()));
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset_v1.public_key));
}

TEST_F(ManifestAssetManagerTest, ReRegistersWhenTargetVersionUpdated) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithVersion("1.0.0.0");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithVersion("2.0.0.0");
  usage_tracker_.RaisePriority(asset_v1.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset_v1));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v1.ToInstallTarget()));
  UpdateManifest(DummyManifest().Add(asset_v2));
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest,
       ReRegistersWhenVersionUpdatedWhileRegistering) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithVersion("1.0.0.0");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithVersion("2.0.0.0");
  usage_tracker_.RaisePriority(asset_v1.use_case,
                               UsageTracker::Priority::kUserBlocking);
  component_state_.SetDeferRegistrationCallbacks(true);
  UpdateManifest(DummyManifest().Add(asset_v1));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v1.ToInstallTarget()));
  // Update manifest with new version while the first registration is pending.
  UpdateManifest(DummyManifest().Add(asset_v2));

  // Complete the deferred callback for version 1.0.
  component_state_.RunPendingRegistrations();

  // Register again for the new version.
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, KeepInstalledWhenAssetRenamed) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithAssetId("asset_1");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithAssetId("asset_2");
  usage_tracker_.RaisePriority(asset_v1.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset_v1));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v1.ToInstallTarget()));
  UpdateManifest(DummyManifest().Add(asset_v2));
  // When only the asset id is changed, we should consider it already installed.
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v2.ToInstallTarget()));
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset_v1.public_key));
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenPublicKeyChanged) {
  DummyAsset asset_v1 = DummyAsset::For("compose").WithPublicKey("key1");
  DummyAsset asset_v2 = DummyAsset::For("compose").WithPublicKey("key2");
  usage_tracker_.RaisePriority(asset_v1.use_case,
                               UsageTracker::Priority::kUserBlocking);
  base::HistogramTester histogram_tester;
  MakeAssetsInstallable(DummyManifest().Add(asset_v1));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset_v1.ToInstallTarget()));
  UpdateManifest(DummyManifest().Add(asset_v2));
  EXPECT_TRUE(component_state_.WaitForUninstall(asset_v1.public_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelUninstallReason.Unknown",
      Manifest::UninstallReason::kObsolete, 1);
}

TEST_F(ManifestAssetManagerTest, UninstallsWhenRunningOutOfDiskSpace) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  SimulateShutdown();
  base::HistogramTester histogram_tester;
  // 5gb is the default in `IsFreeDiskSpaceTooLowForOnDeviceModelInstall`.
  component_state_.SetFreeDiskSpace(base::GiB(5) - base::ByteSizeDelta(1));
  task_environment_.FastForwardBy(base::Seconds(11));
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelUninstallReason.Unknown",
      Manifest::UninstallReason::kInsufficientDisk, 1);
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenFeatureNotEnabled) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kOptimizationGuideModelExecution);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, UninstallWhileRegistrationPending) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  component_state_.SetDeferRegistrationCallbacks(true);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();

  base::HistogramTester histogram_tester;
  // Verify that it is currently registering.
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  // Feature is disabled while registration is pending.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);

  // Uninstall is queued and triggered once registration is complete.
  component_state_.RunPendingRegistrations();
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelUninstallReason.Unknown",
      Manifest::UninstallReason::kDisallowedByUser, 1);
}

TEST_F(ManifestAssetManagerTest, RegisterWhileUninstallPending) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  // 1. The component is already installed.
  MakeAssetsInstallable(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  // 2. Trigger uninstall.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);
  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));

  // 3. Re-enable and verify it eventually installs again.
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled, true);
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, ExternalUninstallOfReadyAssetReregisters) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  EXPECT_EQ(1, component_state_.GetRegistrationCount(asset.public_key));

  // The component is uninstalled by something other than the manager, e.g.
  // the component updater unregistering it. The manager did not request an
  // uninstall, so it must tolerate the notification and, because the manifest
  // still needs the asset, register it again.
  component_state_.SimulateExternalUninstall(asset.public_key);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(component_state_.WasUninstallRequested(asset.public_key));
  EXPECT_EQ(2, component_state_.GetRegistrationCount(asset.public_key));
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, ExternalUninstallWhileRegisteringIsIgnored) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  component_state_.SetDeferRegistrationCallbacks(true);
  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  EXPECT_EQ(1, component_state_.GetRegistrationCount(asset.public_key));

  // An uninstall notification while the registration is in flight is ignored;
  // the registration callback determines the state.
  component_state_.SimulateExternalUninstall(asset.public_key);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, component_state_.GetRegistrationCount(asset.public_key));

  component_state_.RunPendingRegistrations();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, component_state_.GetRegistrationCount(asset.public_key));
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, UninstallNotificationForUnknownKeyIsIgnored) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  // A notification for a key the manager never tracked must not crash.
  DummyAsset unknown = DummyAsset::For("test");
  component_state_.SimulateExternalUninstall(unknown.public_key);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(component_state_.IsRegistered(asset.ToInstallTarget()));
  EXPECT_TRUE(component_state_.IsInstalled(asset.ToInstallTarget()));
  EXPECT_FALSE(component_state_.WasUninstallRequested(unknown.public_key));
  EXPECT_EQ(component_state_.GetRegistrationCount(unknown.public_key), 0);
}

TEST_F(ManifestAssetManagerTest, RemainsInstalledWhenReferencedInManifest) {
  DummyAsset asset_compose = DummyAsset::For("compose").WithAssetId("asset_1");
  DummyAsset asset_test = DummyAsset::For("test").WithAssetId("asset_1");
  usage_tracker_.RaisePriority(asset_compose.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset_compose));
  Startup();
  EXPECT_TRUE(
      component_state_.WaitForRegistration(asset_compose.ToInstallTarget()));
  // compose no longer requires asset_1, but Test does (which isn't used).
  UpdateManifest(DummyManifest().Add(asset_test));
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset_test.public_key));
}

TEST_F(ManifestAssetManagerTest, AssetRemainsInstalledWhileNotRequested) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));
  // Clear usage prefs so that the model is no longer eligible for download.
  local_state_.local_state().ClearPref(
      model_execution::prefs::localstate::kLastUsageByFeature);

  // Trigger an update manifest to re-evaluate the registration.
  UpdateManifest(DummyManifest().Add(asset));

  // Should not uninstall.
  EXPECT_FALSE(component_state_.WasUninstallRequested(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenDisabledByEnterprisePolicy) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  local_state_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      std::to_underlying(
          model_execution::prefs::
              GenAILocalFoundationalModelEnterprisePolicySettings::
                  kDisallowed));

  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest,
       DoesNotInstallWhenDisabledByOnDeviceAIUserSetting) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  local_state_.local_state().SetBoolean(
      model_execution::prefs::localstate::kOnDeviceAiUserSettingsEnabled,
      false);

  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenNotEnoughDiskSpace) {
  base::HistogramTester histogram_tester;
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  // 20gb is the default in `IsFreeDiskSpaceSufficientForOnDeviceModelInstall`.
  component_state_.SetFreeDiskSpace(base::GiB(20) - base::ByteSizeDelta(1));

  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.ToInstallTarget()));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelInstallCriteria."
      "AtRegistration.DiskSpaceWhenNotEnoughAvailable",
      19, 1);
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenEligibleUseCaseUseTooOld) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  task_environment_.FastForwardBy(base::Days(31));

  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, DoesNotInstallWhenNoEligibleUseCaseUse) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  local_state_.local_state().ClearPref(
      model_execution::prefs::localstate::kLastUsageByFeature);

  UpdateManifest(DummyManifest().Add(asset));
  Startup();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(component_state_.IsRegistered(asset.ToInstallTarget()));
}

TEST_F(ManifestAssetManagerTest, BackgroundDownloadForManifestEnabledUseCase) {
  base::test::ScopedPowerMonitorTestSource power_monitor_source;
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {features::kOptimizationGuideModelExecution,
       features::kOnDeviceModelBackgroundDownload},
      {});

  component_state_.SetFreeDiskSpace(base::GiB(100));

  DummyAsset compose_asset =
      DummyAsset::For("compose").WithBackgroundDownload(true);
  DummyAsset test_asset = DummyAsset::For("test").WithBackgroundDownload(false);

  UpdateManifest(DummyManifest().Add(compose_asset).Add(test_asset));
  Startup();

  EXPECT_TRUE(
      component_state_.WaitForRegistration(compose_asset.ToInstallTarget()));
  EXPECT_FALSE(component_state_.IsRegistered(test_asset.public_key));
}

TEST_F(ManifestAssetManagerTest, UninstallModels) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));
  Startup();

  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  manifest_broker_state_->UninstallModels();

  EXPECT_TRUE(component_state_.WaitForUninstall(asset.public_key));
}

TEST_F(ManifestAssetManagerTest, AssetAvailableAfterManifestUpdate) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));
  Startup();
  EXPECT_TRUE(component_state_.WaitForRegistration(asset.ToInstallTarget()));

  auto& subscriber =
      model_broker_client_->GetSubscriber(mojom::OnDeviceFeature::kCompose);
  base::test::TestFuture<base::WeakPtr<ModelClient>> client_future;
  subscriber.WaitForClient(client_future.GetCallback());
  EXPECT_TRUE(client_future.Get());

  // Update manifest to a new version containing the same asset version.
  UpdateManifest(DummyManifest().Add(asset));

  // The client should remain available and able to create sessions immediately.
  CanCreateSessionFuture can_create_future;
  subscriber.CanCreateSession({}, can_create_future.GetCallback());
  EXPECT_EQ(can_create_future.Get<UnavailableReason>(), std::nullopt);
}

TEST_F(ManifestAssetManagerTest,
       InitiallyAvailableOnStartupWhenPreviouslyDownloaded) {
  DummyAsset asset = DummyAsset::For("compose");
  usage_tracker_.RaisePriority(asset.use_case,
                               UsageTracker::Priority::kUserBlocking);
  MakeAssetsInstallable(DummyManifest().Add(asset));

  // First run: install the asset so it is saved in the ledger.
  Startup();
  {
    auto& subscriber =
        model_broker_client_->GetSubscriber(mojom::OnDeviceFeature::kCompose);
    base::test::TestFuture<base::WeakPtr<ModelClient>> client_future;
    subscriber.WaitForClient(client_future.GetCallback());
    EXPECT_TRUE(client_future.Get());
  }
  SimulateShutdown();

  // Second run: on startup, the asset was already downloaded.
  UpdateManifest(DummyManifest().Add(asset));
  Startup();

  auto& subscriber =
      model_broker_client_->GetSubscriber(mojom::OnDeviceFeature::kCompose);
  base::test::TestFuture<base::WeakPtr<ModelClient>> client_future;
  subscriber.WaitForClient(client_future.GetCallback());
  EXPECT_TRUE(client_future.Get());

  CanCreateSessionFuture future;
  subscriber.CanCreateSession({}, future.GetCallback());
  EXPECT_EQ(future.Get<UnavailableReason>(), std::nullopt);
}

// TODO(crbug.com/504749700): Verify these scenarios from these
// OnDeviceModelServiceControllerTest tests are covered by
// ManifestAssetManagerTests BaseModelToBeInstalled BaseModelAvailableAfterInit
// MidSessionModelUpdate
// SessionBeforeAndAfterModelUpdate
// UpdatingSafetyModelEnablesModels
// SessionRequiresSafetyModel

TEST(AssetPrioritiesTest, RaiseAndIsAtLeast) {
  AssetPriorities priorities;

  priorities.Raise(AssetPriority::kSpeculative, {"speculative_asset"});
  priorities.Raise(AssetPriority::kBestEffort, {"best_effort_asset"});
  priorities.Raise(AssetPriority::kUserBlocking, {"user_blocking_asset"});

  EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kSpeculative,
                                   "speculative_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kBestEffort, "speculative_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "speculative_asset"));

  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "best_effort_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kBestEffort, "best_effort_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "best_effort_asset"));

  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "user_blocking_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kBestEffort, "user_blocking_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "user_blocking_asset"));

  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "unknown_asset"));

  // Raising to a lower priority does not decrease existing priority.
  priorities.Raise(AssetPriority::kSpeculative, {"user_blocking_asset"});
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "user_blocking_asset"));

  // Raising to a higher priority increases priority.
  priorities.Raise(AssetPriority::kUserBlocking, {"best_effort_asset"});
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "best_effort_asset"));

  priorities.Clear();
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "speculative_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "best_effort_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "user_blocking_asset"));
}

}  // namespace
}  // namespace optimization_guide
