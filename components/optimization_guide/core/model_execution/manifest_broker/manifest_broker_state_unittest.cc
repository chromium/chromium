// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeManifestBroker fake_;
};

// Test that a simple feature can be executed successfully.
TEST_F(ManifestBrokerStateTest, CreateSessionFailedOnDeviceIncapable) {
  // TODO(crbug.com/504749700): Implement
}

// TODO(crbug.com/504749700): Ensure equivalent scenarios from these
// OnDeviceModelServiceControllerTest tests are covered here:
// ShutsDownServiceAfterPerformanceCheck
// TestAvailabilityObserver
// GetCapabilities
// Broker
// BrokerCreateSessionRunsPerformanceClassCheck
// BrokerCreateSessionFailedOnDeviceIncapable
// BrokerCreateSessionFailedOnNotEnoughDiskSpace

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
