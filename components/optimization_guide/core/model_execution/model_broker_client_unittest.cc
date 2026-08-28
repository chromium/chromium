// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_client.h"

#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

// Verify that a ModelBrokerClient that is not connected fails callbacks.
TEST(ModelBrokerClientTest, DisconnectedClient) {
  base::test::TaskEnvironment task_environment_;
  OptimizationGuideLogger logger;

  mojo::PendingReceiver<mojom::ModelBroker> receiver;
  ModelBrokerClient client(receiver.InitWithNewPipeAndPassRemote(),
                           logger.GetWeakPtr());
  receiver.reset();

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());

  // A broker that is never connected should fail all CreateSession requests,
  // not leave the callbacks uncalled.
  ASSERT_FALSE(future.Get());
}

// Verify that when requesting a session while assets are still pending, the
// client will wait for the assets before resolving the callback.
TEST(ModelBrokerClientTest, PendingClient) {
  base::test::TaskEnvironment task_environment_;
  OptimizationGuideLogger logger;
  FakeManifestBroker broker;
  ScenarioBuilder::MinimalTestScenario(broker.component_state());
  broker.component_state().SetDownloadScenario(
      TestManifestAssetManagerComponentState::DownloadScenario::kOffline);
  broker.Startup();

  auto& client = broker.client();
  EXPECT_FALSE(client.HasSubscriber(mojom::OnDeviceFeature::kTest));

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  }));
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(client.HasSubscriber(mojom::OnDeviceFeature::kTest));

  broker.component_state().SetDownloadScenario(
      TestManifestAssetManagerComponentState::DownloadScenario::kHealthy);
  ASSERT_TRUE(future.Take());
}

// Verify that CreateSession works when all the assets are provided.
TEST(ModelBrokerClientTest, ReadyWithSetupClient) {
  base::test::TaskEnvironment task_environment_;
  OptimizationGuideLogger logger;
  FakeManifestBroker broker;
  ScenarioBuilder::MinimalTestScenario(broker.component_state());
  broker.Startup();
  broker.client().RequestAssetsFor("test");
  task_environment_.RunUntilIdle();

  auto& client = broker.client();
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;

  // Requesting the feature we've provided assets for should succeed.
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  ASSERT_TRUE(future.Take());
}

// Verify that CreateSession works when all the assets are provided using a
// custom use case string.
TEST(ModelBrokerClientTest, ReadyWithUseCaseClient) {
  base::test::TaskEnvironment task_environment_;
  OptimizationGuideLogger logger;
  FakeManifestBroker broker;
  ScenarioBuilder::MinimalTestScenario(broker.component_state());
  broker.Startup();
  broker.client().RequestAssetsFor("test");
  task_environment_.RunUntilIdle();

  auto& client = broker.client();
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;

  // Requesting the feature via its use case name "test" should succeed.
  client.CreateSession("test", SessionConfigParams{}, future.GetCallback());
  ASSERT_TRUE(future.Take());
}

TEST(ModelBrokerClientTest, GetConfigNulloptWhenNotSet) {
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeManifestBroker broker;
  ScenarioBuilder(broker.component_state()).Finish();
  broker.Startup();

  auto& client = broker.client();
  base::test::TestFuture<std::optional<mojo_base::ProtoWrapper>> future;
  client.GetConfig(mojom::OnDeviceFeature::kTest, future.GetCallback());

  auto result = future.Take();
  EXPECT_FALSE(result.has_value());
}

// Sometimes a feature is not supported for certain base models (e.g. EE model).
// Attempts to create a Session for such features should fully resolve as
// unavailable.
TEST(ModelBrokerClientTest, UnavailableAdaptationRejectsSession) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeManifestBroker broker;
  // Start up with no test solution in manifest.
  ScenarioBuilder(broker.component_state()).Finish();
  broker.Startup();

  auto& client = broker.client();
  base::test::TestFuture<std::unique_ptr<OnDeviceSession>> session_future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       session_future.GetCallback());

  task_environment.RunUntilIdle();
  // Session should resolve to unavailable.
  auto session = session_future.Take();
  ASSERT_FALSE(session);
}

}  // namespace optimization_guide
