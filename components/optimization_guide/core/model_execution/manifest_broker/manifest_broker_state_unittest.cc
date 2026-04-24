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
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/manifest.pb.h"
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
TEST_F(ManifestBrokerStateTest, ExecuteTestFeature) {
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
      ("CPU backendFastest inference"
       "hello max:1024"
       "TopK: 3, Temp: 0.800000011920929");
  EXPECT_EQ(*response.value(), expected_response);
}

// Tests that the feature config is propagated correctly from the manifest to
// the client.
TEST_F(ManifestBrokerStateTest, PropagatesFeatureConfig) {
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
TEST_F(ManifestBrokerStateTest, SupportsArbitraryUseCases) {
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

}  // namespace optimization_guide
