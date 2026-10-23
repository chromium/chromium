// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"

#include <memory>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/task/execution_fence.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/configs/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_component_directory.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class ManifestBrokerStateTest : public testing::Test {
 public:
  ManifestBrokerStateTest() {}

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeManifestBroker fake_;
};

// When the device is incapable of running models, session creations attempts
// should fail, rather than hang, and the service should shut down after the
// performance class check.
TEST_F(ManifestBrokerStateTest, CreateSessionFailedOnDeviceIncapable) {
  base::HistogramTester histogram_tester;
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());

  // Ensure the device is considered incapable of using on-device models.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      on_device_model::features::kOnDeviceModelCpuBackend);
  fake_.settings().performance_class =
      on_device_model::mojom::PerformanceClass::kVeryLow;
  fake_.Startup();

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());
  // Broker should have a Manifest that supports no features, so session
  // creations should fail (kNotSupported).
  ASSERT_FALSE(session_future.Take());

  // Performance class should have been evaluated.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass",
      OnDeviceModelPerformanceClass::kVeryLow, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OnDeviceModel.ManifestBrokerInstantiated", true, 1);
  EXPECT_TRUE(fake_.launcher().did_launch_service());
  // Service should idle-out again after the performance class check.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !fake_.launcher().is_service_running(); }));
}

// When the device is incapable of downloading models due to low disk space,
// session creations attempts should fail, rather than hang.
TEST_F(ManifestBrokerStateTest, CreateSessionFailedOnNotEnoughDiskSpace) {
  base::HistogramTester histogram_tester;
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());

  // Ensure the device is considered incapable of using on-device models.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      on_device_model::features::kOnDeviceModelCpuBackend);
  fake_.component_state().SetFreeDiskSpace(base::ByteSize(1));
  fake_.Startup();

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());
  // Broker should have a Manifest that supports no features, so session
  // creations should fail (kNotSupported).
  ASSERT_FALSE(session_future.Take());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OnDeviceModel.ManifestBrokerInstantiated", true, 1);
}

class TestOnDeviceModelAvailabilityObserver
    : public OnDeviceModelAvailabilityObserver {
 public:
  explicit TestOnDeviceModelAvailabilityObserver(
      mojom::OnDeviceFeature expected_feature) {
    expected_feature_ = expected_feature;
  }

  void OnDeviceModelAvailabilityChanged(
      mojom::OnDeviceFeature feature,
      OnDeviceModelEligibilityReason reason) override {
    EXPECT_EQ(expected_feature_, feature);
    reason_ = reason;
  }
  mojom::OnDeviceFeature expected_feature_;
  std::optional<OnDeviceModelEligibilityReason> reason_;
};

// Verify that availability observers are hooked up to the ModelBrokerImpl.
TEST_F(ManifestBrokerStateTest, TestAvailabilityObserver) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  fake_.Startup();
  TestOnDeviceModelAvailabilityObserver obs(mojom::OnDeviceFeature::kTest);
  fake_.state().AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature::kTest, &obs);
  fake_.client().RequestAssetsFor("test");
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return obs.reason_ == OnDeviceModelEligibilityReason::kSuccess;
  }));
}

// Tests fallback when `max_tokens` manifest proto field is 0 or unspecified.
TEST_F(ManifestBrokerStateTest, FallbackToDefaultMaxTokens) {
  std::string name = "model_A";
  fake_.component_state().UpdateBaseModel(name + "_key", []() {
    auto base_model_asset = std::make_unique<FakeBaseModelAsset>();
    base_model_asset->set_version("1.0.0.0");
    return base_model_asset;
  }());

  ManifestBuilder builder;
  builder.Add(name + "_asset", OnDemandComponent(name + "_key", "1.0.0.0"));
  builder.Add(name + "_recipe",
              BaseModelRecipe(
                  FileReference(name + "_asset", "weights.bin"),
                  BaseModelRecipeArgs(
                      proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                      proto::BaseModelRecipe::PERFORMANCE_HINT_UNSPECIFIED, {},
                      /*max_tokens=*/0)));
  builder.Add("test_solution",
              SolutionRecipe(name + "_recipe",
                             ManifestFileReference("testconfig.pb")));
  builder.Add(DeviceUseCase{DeviceCategory::kGpuHighTier, "test"},
              "test_solution");

  auto manifest_directory =
      std::make_unique<ManifestComponentDirectory>(builder.Build());
  proto::SolutionConfig solution_config;
  *solution_config.mutable_feature() = SimpleTestFeatureConfig();
  manifest_directory->Add("testconfig.pb", solution_config);
  fake_.component_state().UpdateManifest(std::move(manifest_directory));
  fake_.Startup();

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());
  auto session = session_future.Take();
  ASSERT_TRUE(session);
  EXPECT_EQ(session->GetTokenLimits().max_tokens, kOnDeviceModelMaxTokens);
}

class TestDebugObserver : public mojom::ModelBrokerDebugObserver {
 public:
  TestDebugObserver() = default;
  ~TestDebugObserver() override = default;

  mojo::PendingRemote<mojom::ModelBrokerDebugObserver> Bind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnBrokerStateChanged() override {
    call_count_++;
    run_loop_.Quit();
  }

  void WaitForNotification() { run_loop_.Run(); }

  int call_count() const { return call_count_; }

 private:
  mojo::Receiver<mojom::ModelBrokerDebugObserver> receiver_{this};
  int call_count_ = 0;
  base::RunLoop run_loop_;
};

TEST_F(ManifestBrokerStateTest, ModelBrokerDebugObserverNotified) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  fake_.Startup();

  TestDebugObserver observer;
  fake_.state().AddObserver(observer.Bind());

  // Force initialization to start performance classification and load manifest.
  base::test::TestFuture<OnDeviceModelEligibilityReason> eligibility_future;
  fake_.state().GetOnDeviceModelEligibilityAsync(
      mojom::OnDeviceFeature::kTest, {}, eligibility_future.GetCallback());
  ASSERT_TRUE(eligibility_future.Wait());

  observer.WaitForNotification();
  EXPECT_GE(observer.call_count(), 1);
}

TEST_F(ManifestBrokerStateTest, UninstallModels) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  fake_.Startup();
  fake_.client().RequestAssetsFor("test");

  EXPECT_TRUE(fake_.component_state().WaitForRegistration(
      {"model_A_key", base::Version("1.0.0.0")}));

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());
  EXPECT_TRUE(session_future.Take());

  fake_.state().UninstallModels();

  EXPECT_TRUE(fake_.component_state().WaitForUninstall("model_A_key"));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return fake_.client().GetSubscriber("test").unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingUsage;
  }));

  // Requesting assets again should re-install and restore availability.
  fake_.client().RequestAssetsFor("test");
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult>
      session_future2;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future2.GetCallback());
  EXPECT_TRUE(session_future2.Take());
}

TEST_F(ManifestBrokerStateTest, SetUseCaseRequested) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  fake_.Startup();
  fake_.state().SetUseCaseRequested("test", true);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());
  EXPECT_TRUE(session_future.Take());

  fake_.state().SetUseCaseRequested("test", false);

  EXPECT_TRUE(fake_.component_state().WaitForUninstall("model_A_key"));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return fake_.client().GetSubscriber("test").unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingUsage;
  }));

  // Requesting the use case again should re-install and restore availability.
  fake_.state().SetUseCaseRequested("test", true);
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult>
      session_future2;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future2.GetCallback());
  EXPECT_TRUE(session_future2.Take());
}

TEST_F(ManifestBrokerStateTest, SetUseCaseRequestedFalseBeforeManifestLoaded) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  ASSERT_TRUE(fake_.WarmupPrefsAndAssets());

  // Call SetUseCaseRequested(false) immediately on startup before the manifest
  // has finished loading and creating ManifestAssetManager.
  fake_.state().SetUseCaseRequested("test", false);

  // Trigger initialization and verify the model is evicted once the asset
  // manager initializes.
  fake_.client().GetSubscriber("test");
  EXPECT_TRUE(fake_.component_state().WaitForUninstall("model_A_key"));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return fake_.client().GetSubscriber("test").unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingUsage;
  }));
}

TEST_F(ManifestBrokerStateTest, UninstallCapabilityBeforeManifestLoaded) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  ASSERT_TRUE(fake_.WarmupPrefsAndAssets());

  // Call UninstallCapability immediately on startup before the manifest has
  // finished loading and creating ManifestAssetManager.
  fake_.state().UninstallCapability("model_A_asset");

  EXPECT_TRUE(fake_.component_state().WaitForUninstall("model_A_key"));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return fake_.client().GetSubscriber("test").unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingUsage;
  }));
}

TEST_F(ManifestBrokerStateTest,
       UninstallCapabilityWithAdaptationsAndSharedPostprocessorModel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {kOnDeviceModelEviction, kOnDeviceWebSpeechPolisherModel}, {});
  fake_.local_state().SetBoolean(model_execution::prefs::localstate::
                                     kOnDeviceAiAutoRemoveUnusedModelsEnabled,
                                 false);

  proto::SolutionConfig compose_config;
  *compose_config.mutable_feature() = SimpleComposeConfig();

  // Include `model_C` ("prompt_api") which also references `polisher` but is
  // never used, to verify that an unused use case in the manifest does not
  // retain the shared auxiliary model when all used base models are
  // uninstalled.
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel("model_A")
      .AddBaseModel("model_B")
      .AddBaseModel("model_C")
      .AddBaseModel("polisher")
      .AddAdaptation("adapt_A", "model_A")
      .AddPostprocessedModel("post_A", "model_A", "polisher")
      .AddPostprocessedModel("post_B", "model_B", "polisher")
      .AddPostprocessedModel("post_C", "model_C", "polisher")
      .AddUnsafeSolution("test", "adapt_A")
      .AddUnsafeSolution("summarize", "post_A")
      .AddUnsafeSolution("compose", "post_B", std::move(compose_config))
      .AddUnsafeSolution("prompt_api", "post_C")
      .Finish();
  fake_.Startup();

  fake_.client().RequestAssetsFor("test");
  fake_.client().RequestAssetsFor("summarize");
  fake_.client().RequestAssetsFor("compose");

  EXPECT_TRUE(fake_.component_state().WaitForRegistration(
      {"model_A_key", base::Version("1.0.0.0")}));
  EXPECT_TRUE(fake_.component_state().WaitForRegistration(
      {"adapt_A_key", base::Version("12345")}));
  EXPECT_TRUE(fake_.component_state().WaitForRegistration(
      {"model_B_key", base::Version("1.0.0.0")}));
  EXPECT_TRUE(fake_.component_state().WaitForRegistration(
      {"polisher_key", base::Version("1.0.0.0")}));

  {
    base::test::TestFuture<ModelBrokerClient::CreateSessionResult> test_future;
    fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                                 SessionConfigParams{},
                                 test_future.GetCallback());
    EXPECT_TRUE(test_future.Take());

    base::test::TestFuture<ModelBrokerClient::CreateSessionResult>
        compose_future;
    fake_.client().CreateSession(mojom::OnDeviceFeature::kCompose,
                                 SessionConfigParams{},
                                 compose_future.GetCallback());
    EXPECT_TRUE(compose_future.Take());
  }

  // Advance past the 30-day retention window so that "test"/"summarize"
  // (model_A) and "compose" (model_B) become kEvictable. With Auto-Removal
  // disabled, uninstalling model_A should still retain the expired model_B and
  // the shared polisher model.
  task_environment_.FastForwardBy(base::Days(31));

  OnDeviceModelControl* model_control = fake_.state().GetModelControl();
  ASSERT_NE(model_control, nullptr);

  // Uninstalling model_A should uninstall model_A and its adaptation adapt_A,
  // while keeping model_B and the shared polisher model.
  model_control->UninstallCapability("model_A_asset");

  EXPECT_TRUE(fake_.component_state().WaitForUninstall("model_A_key"));
  EXPECT_TRUE(fake_.component_state().WaitForUninstall("adapt_A_key"));
  EXPECT_FALSE(fake_.component_state().WasUninstallRequested("model_B_key"));
  EXPECT_FALSE(fake_.component_state().WasUninstallRequested("polisher_key"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return fake_.client().GetSubscriber("test").unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingUsage;
  }));
  EXPECT_EQ(fake_.client().GetSubscriber("compose").unavailable_reason(),
            std::nullopt);

  // Uninstalling the remaining used model_B should now also uninstall the
  // shared polisher model even though unused model_C also references polisher
  // in the manifest.
  model_control->UninstallCapability("model_B_asset");

  EXPECT_TRUE(fake_.component_state().WaitForUninstall("model_B_key"));
  EXPECT_TRUE(fake_.component_state().WaitForUninstall("polisher_key"));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return fake_.client().GetSubscriber("compose").unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingUsage;
  }));
}

TEST_F(ManifestBrokerStateTest,
       CreateSessionFailedOnInsufficientDiskSpaceForCaches) {
  // Use a base model without cache files so that has_caches is false,
  // which allows the kInsufficientDiskSpaceForCaches check to trigger.
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel(
          "model_A",
          BaseModelRecipeArgs(
              proto::BaseModelRecipe::BACKEND_TYPE_GPU,
              proto::BaseModelRecipe::PERFORMANCE_HINT_HIGHEST_QUALITY, {},
              100),
          FakeBaseModelAsset::Content{.shader_cache_data = nullptr}, "1.0.0.0")
      .AddUnsafeSolution("test", "model_A")
      .AddUnsafeSolution("compose", "model_A")
      .Finish();
  fake_.Startup();
  TestOnDeviceModelAvailabilityObserver obs(mojom::OnDeviceFeature::kTest);
  fake_.state().AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature::kTest, &obs);
  fake_.client().RequestAssetsFor("test");
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return obs.reason_ == OnDeviceModelEligibilityReason::kSuccess;
  }));

  // Drop free space below 10 GiB required for building caches after install.
  fake_.component_state().SetFreeDiskSpace(base::GiB(8));
  task_environment_.FastForwardBy(base::Seconds(11));
  // Request a different use case to trigger a fresh disk space evaluation.
  fake_.client().RequestAssetsFor("compose");
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return obs.reason_ ==
           OnDeviceModelEligibilityReason::kInsufficientDiskSpaceForCaches;
  }));
}

TEST_F(ManifestBrokerStateTest,
       CreateSessionSucceedsWithLowDiskSpaceWhenCachesExist) {
  // ScenarioBuilder::AddBaseModel creates cache files by default,
  // so has_caches is true.
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel("model_A")
      .AddUnsafeSolution("test", "model_A")
      .AddUnsafeSolution("compose", "model_A")
      .Finish();
  fake_.Startup();
  TestOnDeviceModelAvailabilityObserver obs(mojom::OnDeviceFeature::kTest);
  fake_.state().AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature::kTest, &obs);
  fake_.client().RequestAssetsFor("test");
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return obs.reason_ == OnDeviceModelEligibilityReason::kSuccess;
  }));

  // Drop free space below 10 GiB required for building caches.
  fake_.component_state().SetFreeDiskSpace(base::GiB(8));
  task_environment_.FastForwardBy(base::Seconds(11));
  // Request a different use case to trigger a fresh disk space evaluation.
  fake_.client().RequestAssetsFor("compose");

  // Since caches already exist on disk, availability remains kSuccess and
  // session creation succeeds.
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());
  EXPECT_TRUE(session_future.Take());
  EXPECT_EQ(obs.reason_, OnDeviceModelEligibilityReason::kSuccess);
}

TEST_F(ManifestBrokerStateTest, EnsureInitializationBypassesBestEffortFence) {
  base::ScopedBestEffortExecutionFence best_effort_fence;
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  // Pre-seed the performance class so that ManifestMonitor::OnInputsChanged
  // posts Manifest::Load during startup while the best-effort execution fence
  // is active.
  UpdatePerformanceClassPref(&fake_.local_state(),
                             OnDeviceModelPerformanceClass::kHigh);
  fake_.Startup();
  base::RunLoop().RunUntilIdle();

  // Before EnsureInitialization is called, the manifest load is queued at
  // BEST_EFFORT priority and blocked by the execution fence.
  EXPECT_EQ(
      fake_.state().GetOnDeviceModelEligibility(mojom::OnDeviceFeature::kTest),
      OnDeviceModelEligibilityReason::kUnknown);

  // Triggering EnsureInitialization should raise the manifest load priority to
  // USER_VISIBLE and complete even while the best-effort execution fence is
  // active.
  fake_.client().RequestAssetsFor("test");
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());
  EXPECT_TRUE(session_future.Take());
}

}  // namespace optimization_guide
