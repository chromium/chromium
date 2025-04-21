// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_client.h"

#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/fake_remote.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(ModelBrokerClientTest, DisconnectedClient) {
  base::test::TaskEnvironment task_environment_;

  mojo::PendingReceiver<mojom::ModelBroker> receiver;
  ModelBrokerClient client(receiver.InitWithNewPipeAndPassRemote(),
                           CreateSessionArgs(nullptr, FailOnRemoteFallback()));
  receiver.reset();

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::ModelBasedCapabilityKey::kTest, std::nullopt,
                       future.GetCallback());

  // A broker that is never connected should fail all CreateSession requests,
  // not leave the callbacks uncalled.
  ASSERT_FALSE(future.Get());
}

TEST(ModelBrokerClientTest, PendingClient) {
  base::test::TaskEnvironment task_environment_;
  FakeAdaptationAsset fake_asset({.config = SimpleComposeConfig()});
  FakeModelBroker fake_broker(fake_asset);
  ModelBrokerClient client(fake_broker.BindAndPassRemote(),
                           CreateSessionArgs(nullptr, FailOnRemoteFallback()));

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  // Requesting test feature, but only compose has assets.
  client.CreateSession(mojom::ModelBasedCapabilityKey::kTest, std::nullopt,
                       future.GetCallback());

  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::ModelBasedCapabilityKey::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  EXPECT_FALSE(future.IsReady());
}

TEST(ModelBrokerClientTest, ReadyWithSetupClient) {
  base::test::TaskEnvironment task_environment_;
  FakeAdaptationAsset test_asset({
      .config =
          []() {
            auto config = SimpleComposeConfig();
            config.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
            config.set_can_skip_text_safety(true);
            return config;
          }(),
  });
  FakeModelBroker fake_broker(test_asset);
  ModelBrokerClient client(fake_broker.BindAndPassRemote(),
                           CreateSessionArgs(nullptr, FailOnRemoteFallback()));
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;

  // Requesting the feature we've provided assets for should succeed.
  client.CreateSession(mojom::ModelBasedCapabilityKey::kTest, std::nullopt,
                       future.GetCallback());
  ASSERT_TRUE(future.Take());
}

}  // namespace optimization_guide
