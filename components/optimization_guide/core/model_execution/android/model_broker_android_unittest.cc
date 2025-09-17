// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"

#include <optional>

#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/fake_remote.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "services/on_device_model/android/on_device_model_bridge_native_unittest_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

proto::OnDeviceBaseModelMetadata MatchingMetadata(
    const OnDeviceBaseModelSpec& spec) {
  return CreateOnDeviceBaseModelMetadata(spec.model_name, spec.model_version,
                                         {spec.selected_performance_hint});
}

// TODO: crbug.com/442914748 - Support text safety.
proto::OnDeviceModelExecutionFeatureConfig UnsafeTestFeatureConfig() {
  proto::OnDeviceModelExecutionFeatureConfig cfg = SimpleTestFeatureConfig();
  cfg.set_can_skip_text_safety(true);
  return cfg;
}

class ModelBrokerAndroidFeatureList {
 public:
  ModelBrokerAndroidFeatureList() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kOptimizationGuideModelExecution, {}},
            {features::internal::kOnDeviceModelTestFeature, {}},
            {features::kOptimizationGuideOnDeviceModel, {}},
        },
        {});
  }
  ~ModelBrokerAndroidFeatureList() = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ModelBrokerAndroidTest : public testing::Test {
 public:
  ModelBrokerAndroidTest() {
    java_helper_.SetMockAiCoreFactory();
  }
  ~ModelBrokerAndroidTest() override = default;

  ModelBrokerAndroid& EnsureBroker() {
    if (!broker_) {
      broker_.emplace(local_state_.local_state(), provider_);
    }
    return *broker_;
  }

  mojo::PendingRemote<mojom::ModelBroker> BindAndPassRemote() {
    mojo::PendingRemote<mojom::ModelBroker> remote;
    EnsureBroker().BindBroker(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void InstallTestFeatureConfig() {
    provider_.UpdateModelImmediatelyForTesting(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
        std::make_unique<ModelInfo>(test_asset_.model_info()));
  }

 protected:
  ModelBrokerAndroidFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ModelBrokerPrefService local_state_;
  OptimizationGuideLogger logger_;
  ModelProviderRegistry provider_{&logger_};
  std::optional<ModelBrokerAndroid> broker_;
  OnDeviceBaseModelSpec spec_{
      "Test", "0.0.1", proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED};
  on_device_model::OnDeviceModelBridgeNativeUnitTestHelper java_helper_;
  FakeAdaptationAsset test_asset_{{
      .config = UnsafeTestFeatureConfig(),
      .metadata = MatchingMetadata(spec_),
  }};
};

// Verify that when requesting a session while assets are still pending, the
// client will wait for the assets before resolving the callback.
TEST_F(ModelBrokerAndroidTest, PendingClient) {
  ModelBrokerClient client(BindAndPassRemote(),
                           CreateSessionArgs(nullptr, FailOnRemoteFallback()));
  // Requesting test feature, but assets not available.
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::ModelBasedCapabilityKey::kTest, std::nullopt,
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::ModelBasedCapabilityKey::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(client.HasSubscriber(mojom::ModelBasedCapabilityKey::kTest));
}

// Verify that CreateSession works when the download succeeds.
TEST_F(ModelBrokerAndroidTest, DownloadSuccess) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(),
                           CreateSessionArgs(nullptr, FailOnRemoteFallback()));

  // Requesting the feature we've provided assets for should succeed.
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::ModelBasedCapabilityKey::kTest, std::nullopt,
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::ModelBasedCapabilityKey::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  java_helper_.TriggerDownloaderOnAvailable(spec_.model_name,
                                            spec_.model_version);
  EXPECT_NE(future.Get(), nullptr);
}

// Verify that when download fails, the client is notified.
TEST_F(ModelBrokerAndroidTest, DownloadFailure) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(),
                           CreateSessionArgs(nullptr, FailOnRemoteFallback()));

  // Requesting the feature we've provided assets for should fail.
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::ModelBasedCapabilityKey::kTest, std::nullopt,
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::ModelBasedCapabilityKey::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  java_helper_.TriggerDownloaderOnUnavailable(
      on_device_model::ModelDownloaderAndroid::DownloadFailureReason::
          kUnknownError);
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::ModelBasedCapabilityKey::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kNotSupported;
  });
  EXPECT_EQ(future.Get(), nullptr);
}

}  // namespace

}  // namespace optimization_guide
