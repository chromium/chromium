// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_client.h"

#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
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
  FakeAdaptationAsset fake_asset({.config = SimpleComposeConfig()});
  FakeModelBroker fake_broker({});
  fake_broker.UpdateModelAdaptation(fake_asset);

  ModelBrokerClient client(fake_broker.BindAndPassRemote(),
                           logger.GetWeakPtr());
  EXPECT_FALSE(client.HasSubscriber(mojom::OnDeviceFeature::kTest));

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  // Requesting test feature, but only compose has assets.
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());

  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(client.HasSubscriber(mojom::OnDeviceFeature::kTest));
}

// Verify that CreateSession works when all the assets are provided.
TEST(ModelBrokerClientTest, ReadyWithSetupClient) {
  base::test::TaskEnvironment task_environment_;
  OptimizationGuideLogger logger;
  FakeAdaptationAsset fake_asset({
      .config =
          []() {
            auto config = SimpleComposeConfig();
            config.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
            config.set_can_skip_text_safety(true);
            return config;
          }(),
  });
  FakeModelBroker fake_broker({});
  fake_broker.UpdateModelAdaptation(fake_asset);

  ModelBrokerClient client(fake_broker.BindAndPassRemote(),
                           logger.GetWeakPtr());
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;

  // Requesting the feature we've provided assets for should succeed.
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  ASSERT_TRUE(future.Take());
}

// Sometimes a feature is not supported for certain base models (e.g. EE model).
// Attempts to create a Session for such features should fully resolve as
// unavailable.
TEST(ModelBrokerClientTest, UnavailableAdaptationRejectsSession) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  OptimizationGuideLogger logger;
  // Note: We pass a compose asset here, so the kTest feature will still be in
  // kPendingAsset status.
  FakeAdaptationAsset fake_asset{{
      .config = SimpleComposeConfig(),
  }};
  FakeModelBroker broker({});
  broker.UpdateModelAdaptation(fake_asset);

  mojo::PendingReceiver<mojom::ModelBroker> pending_broker;
  ModelBrokerClient broker_client(broker.BindAndPassRemote(),
                                  logger.GetWeakPtr());

  base::test::TestFuture<std::unique_ptr<OnDeviceSession>> session_future;
  broker_client.CreateSession(mojom::OnDeviceFeature::kTest,
                              SessionConfigParams{},
                              session_future.GetCallback());

  // Session should not resolve yet, because test adaptation asset has a
  // kUpdatePending status.
  task_environment.FastForwardBy(base::Hours(1));
  ASSERT_FALSE(session_future.IsReady());

  // Emulate receiving info that a adaptation is not available from server.
  // Provider removes the target when the server says no matching model is
  // available.
  broker.model_provider().RemoveModel(
      GetOptimizationTargetForFeature(mojom::OnDeviceFeature::kTest));

  // Session should resolve to unavailable.
  auto session = session_future.Take();
  ASSERT_FALSE(session);
}

}  // namespace optimization_guide
