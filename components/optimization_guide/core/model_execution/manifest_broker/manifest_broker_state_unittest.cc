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

// GPU blocked is a non-recoverable error, so the service should not be
// restarted after the disconnect, and clients should be notified that the
// service is unavailable.
TEST_F(ManifestBrokerStateTest, RespectsGpuBlockedError) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  ASSERT_TRUE(fake_.WarmupPrefsAndAssets());
  fake_.settings().service_disconnect_reason =
      on_device_model::ServiceDisconnectReason::kGpuBlocked;
  // First session creation should succeed, because we don't know that
  // the GPU is blocked until we try to start the service.
  auto session = fake_.CreateSession();
  ASSERT_TRUE(session);

  // Clients should eventually be notified that the service is unavailable.
  task_environment_.FastForwardBy(base::Seconds(1));
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            mojom::ModelUnavailableReason::kNotSupported);

  fake_.launcher().clear_did_launch_service();
  // Using the original session should not trigger launching the service again.
  session->AddContext(UserInputRequest("baz"));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(fake_.launcher().did_launch_service());
}

TEST_F(ManifestBrokerStateTest, StopsConnectingAfterMultipleDrops) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  ASSERT_TRUE(fake_.WarmupPrefsAndAssets());
  fake_.settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);

  // Start enough sessions to trigger the crash count limit.
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(fake_.CreateSession()) << i;
    // Wait for the disconnect to be counted as a crash.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
  // Clients should be notified that the service is unavailable.
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            mojom::ModelUnavailableReason::kNotSupported);

  // Fast forward by backoff time and it should be possible to connect again.
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            std::nullopt);

  // Starting a new session should succeed because the backoff period is over.
  EXPECT_TRUE(fake_.CreateSession());
  // Wait for the disconnect to be counted as a crash.
  task_environment_.FastForwardBy(base::Seconds(1));
  // Clients should be notified that the service is unavailable again.
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            mojom::ModelUnavailableReason::kNotSupported);

  // The backoff period should be longer now, so fast forwarding by base time
  // should not be enough.
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            mojom::ModelUnavailableReason::kNotSupported);

  // Fast forward again should allow retrying (now 2 * base time).
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            std::nullopt);
}

TEST_F(ManifestBrokerStateTest, IdleTimeoutNotCountedAsCrash) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  ASSERT_TRUE(fake_.WarmupPrefsAndAssets());
  fake_.settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kIdleShutdown);

  // Start enough sessions to trigger the crash count limit.
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(fake_.CreateSession()) << i;
    // Wait for the disconnect to be counted as a crash.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
  // Service should not be disabled because the disconnects are not crashes.
  EXPECT_TRUE(fake_.CreateSession());
}

TEST_F(ManifestBrokerStateTest, ClearsCrashDataOnSuccessAfterBackoff) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  ASSERT_TRUE(fake_.WarmupPrefsAndAssets());
  fake_.settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);

  // Start enough sessions to trigger the crash count limit.
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(fake_.CreateSession()) << i;
    // Wait for the disconnect to be counted as a crash.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
  // Clients should be notified that the service is unavailable.
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            mojom::ModelUnavailableReason::kNotSupported);

  // Fast forward by backoff time and it should be possible to connect again.
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            std::nullopt);

  // This test diverges from StopsConnectingAfterMultipleDrops here.
  fake_.settings().set_drop_connection_request(std::nullopt);
  // Starting a new session should succeed because the backoff period is over.
  EXPECT_TRUE(fake_.CreateSession());
  // Give time for any potential crash to be counted, but it shouldn't happen.
  task_environment_.FastForwardBy(base::Seconds(1));
  // We should still be able to create sessions.
  EXPECT_TRUE(fake_.CreateSession());

  // Crash again.
  fake_.settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);
  EXPECT_TRUE(fake_.CreateSession());
  // Wait for the disconnect to be counted as a crash.
  task_environment_.FastForwardBy(base::Seconds(1));
  // We should still be able to create sessions, because the crash count was
  // reset by the previous success.
  EXPECT_TRUE(fake_.CreateSession());
}

TEST_F(ManifestBrokerStateTest, AlternatingDisconnectSucceeds) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  ASSERT_TRUE(fake_.WarmupPrefsAndAssets());
  for (int i = 0; i < 10; ++i) {
    // Crash on odd iterations, succeed on even iterations.
    // This should reset the crash count on even iterations, preventing the
    // crash limit from being reached.
    fake_.settings().set_drop_connection_request(
        i % 2 == 1 ? std::make_optional(
                         on_device_model::ModelDisconnectReason::kUnspecified)
                   : std::nullopt);
    EXPECT_TRUE(fake_.CreateSession()) << i;
    // Wait for the disconnect / success to be counted.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_F(ManifestBrokerStateTest, MultipleDisconnectsThenVersionChangeRetries) {
  ScenarioBuilder::MinimalTestScenario(fake_.component_state());
  ASSERT_TRUE(fake_.WarmupPrefsAndAssets());
  fake_.settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);

  // Start enough sessions to trigger the crash count limit.
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(fake_.CreateSession()) << i;
    // Wait for the disconnect to be counted as a crash.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
  // Clients should be notified that the service is unavailable.
  ASSERT_EQ(fake_.client().GetSubscriber("test").unavailable_reason(),
            mojom::ModelUnavailableReason::kNotSupported);

  fake_.SimulateShutdown();
  fake_.local_state().SetString(
      model_execution::prefs::localstate::kOnDeviceModelChromeVersion,
      "BOGUS VERSION");
  fake_.Startup();
  // Should succeed because the version change resets the crash count.
  EXPECT_TRUE(fake_.CreateSession());
}

}  // namespace optimization_guide
