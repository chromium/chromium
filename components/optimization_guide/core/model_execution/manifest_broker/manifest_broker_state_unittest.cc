// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"

#include <memory>
#include <string>
#include <vector>

#include "base/byte_count.h"
#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class ManifestBrokerStateTest : public testing::Test {
 public:
  ManifestBrokerStateTest() {}

 protected:
  base::test::TaskEnvironment task_environment_;
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
  fake_.component_state().SetFreeDiskSpace(base::ByteCount(1));
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
              SolutionRecipe(name + "_recipe", "",
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

}  // namespace optimization_guide
