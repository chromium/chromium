// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/manifest.pb.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

// These tests verify that ManifestBroker follows recipes correctly, e.g. the
// right configs are served, working solutions that load models correctly are
// constructed, etc.
class ManifestSolutionFactoryTest : public testing::Test {
 public:
  ManifestSolutionFactoryTest() {}

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeManifestBroker fake_;
};

// Tests that the feature config is propagated correctly from the manifest to
// the client.
TEST_F(ManifestSolutionFactoryTest, PropagatesFeatureConfig) {
  proto::Any config;
  config.set_type_url("type.googleapis.com/test.Config");
  config.set_value("test_value");

  ScenarioBuilder(fake_.component_state())
      .SetFeatureConfig(DeviceCategory::kGpuHighTier, "summarizer_api", config)
      .Finish();
  fake_.Startup();
  base::test::TestFuture<std::optional<mojo_base::ProtoWrapper>> future;
  fake_.client().GetConfig(mojom::OnDeviceFeature::kSummarize,
                           future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->As<proto::Any>()->value(), "test_value");
}

// Tests that the ManifestBrokerState can handle arbitrary use cases that are
// not part of the pre-defined `mojom::OnDeviceFeature` enum.
TEST_F(ManifestSolutionFactoryTest, SupportsArbitraryUseCases) {
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel("model_A")
      .AddUnsafeSolution("custom_use_case", "model_A")
      .Finish();
  fake_.Startup();

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession("custom_use_case", SessionConfigParams{},
                               session_future.GetCallback());

  auto session = session_future.Take();
  ASSERT_TRUE(session);
}

// Trying to use a feature that's missing from the manifest should fail,
// not wait for a download to complete.
TEST_F(ManifestSolutionFactoryTest, SessionFailsForMissingFeature) {
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel("model_A")
      .AddUnsafeSolution("custom_use_case", "model_A")
      .Finish();
  fake_.Startup();

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());
  EXPECT_FALSE(session_future.Take());
}

// Test that a simple feature can be executed successfully.
TEST_F(ManifestSolutionFactoryTest, ExecuteTestFeature) {
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel("model_A")
      .AddUnsafeSolution("test", "model_A")
      .Finish();
  fake_.Startup();
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());

  auto session = session_future.Take();
  ASSERT_TRUE(session);

  proto::ExampleForTestingRequest request;
  request.set_string_value("hello");

  ResponseHolder response;
  session->ExecuteModel(request, response.GetStreamingCallback());

  EXPECT_TRUE(response.GetFinalStatus());

  std::string expected_response =
      ("Encoder cache weight: 1016"
       "Adapter cache weight: 1017"
       "hello max:1024"
       "TopK: 3, Temp: 0.800000011920929");
  EXPECT_EQ(*response.value(), expected_response);
}

// Test that a simple feature can be executed successfully.
TEST_F(ManifestSolutionFactoryTest, ExecuteTestFeatureWithHints) {
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel(
          "model_A",
          BaseModelRecipeArgs(
              proto::BaseModelRecipe::BACKEND_TYPE_CPU,
              proto::BaseModelRecipe::PERFORMANCE_HINT_FASTEST_INFERENCE,
              /* supported_ranks= */ {4, 5},
              /* max_tokens= */ 100))
      .AddUnsafeSolution("test", "model_A")
      .Finish();
  fake_.Startup();
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());

  auto session = session_future.Take();
  ASSERT_TRUE(session);

  proto::ExampleForTestingRequest request;
  request.set_string_value("hello");

  ResponseHolder response;
  session->ExecuteModel(request, response.GetStreamingCallback());

  EXPECT_TRUE(response.GetFinalStatus());

  std::string expected_response =
      ("CPU backend"
       "Fastest inference"
       "Cache weight: 1015"
       "Encoder cache weight: 1016"
       "Adapter cache weight: 1017"
       "hello max:1024"
       "TopK: 3, Temp: 0.800000011920929");
  EXPECT_EQ(*response.value(), expected_response);
}

TEST_F(ManifestSolutionFactoryTest, ExecuteTestFeatureWithAdaptation) {
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel("model_A")
      .AddAdaptation("model_A1", "model_A")
      .AddUnsafeSolution("test", "model_A1")
      .Finish();
  fake_.Startup();
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());

  auto session = session_future.Take();
  ASSERT_TRUE(session);

  proto::ExampleForTestingRequest request;
  request.set_string_value("hello");

  ResponseHolder response;
  session->ExecuteModel(request, response.GetStreamingCallback());

  ASSERT_TRUE(response.GetFinalStatus());

  std::string expected_response =
      ("Adaptation model: 1"
       "Encoder cache weight: 1016"
       "Adapter cache weight: 1017"
       "hello max:1024"
       "TopK: 3, Temp: 0.800000011920929");
  EXPECT_EQ(*response.value(), expected_response);
}

TEST_F(ManifestSolutionFactoryTest, ExecuteTestFeatureWithSafety) {
  ScenarioBuilder(fake_.component_state())
      .AddBaseModel("model_A")
      .AddSafetyModel("safety")
      .AddSafeSolution(
          "test", "model_A", "safety",
          []() {
            proto::SolutionConfig solution_config;
            *solution_config.mutable_feature() = SimpleComposeConfig();
            *solution_config.mutable_safety() = ComposeSafetyConfig();
            return solution_config;
          }())
      .Finish();
  fake_.Startup();
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> session_future;
  fake_.client().CreateSession(mojom::OnDeviceFeature::kTest,
                               SessionConfigParams{},
                               session_future.GetCallback());

  auto session = session_future.Take();
  ASSERT_TRUE(session);

  ResponseHolder response;
  session->ExecuteModel(UserInputRequest("hello"),
                        response.GetStreamingCallback());

  EXPECT_TRUE(response.GetFinalStatus());
  EXPECT_EQ(response.error(), std::nullopt);

  std::string expected_response =
      ("Encoder cache weight: 1016"
       "Adapter cache weight: 1017"
       "execute:hello max:1024"
       "TopK: 3, Temp: 0.800000011920929");
  EXPECT_EQ(*response.value(), expected_response);
}

// TODO(crbug.com/504749700): Ensure equivalent scenarios from these
// OnDeviceModelServiceControllerTest tests are covered here:
// MultipleModelAdaptationExecutionSuccess
// ModelAdaptationAndBaseModelSuccess
// DisconnectsWhenIdle

}  // namespace optimization_guide
