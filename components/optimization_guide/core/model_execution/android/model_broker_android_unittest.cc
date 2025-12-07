// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"

#include <optional>

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
proto::OnDeviceModelExecutionFeatureConfig UnsafeFeatureConfig(
    mojom::OnDeviceFeature feature) {
  proto::OnDeviceModelExecutionFeatureConfig cfg = SimpleTestFeatureConfig();
  cfg.set_feature(ToModelExecutionFeatureProto(feature));
  cfg.set_can_skip_text_safety(true);
  return cfg;
}

class ModelBrokerAndroidFeatureList {
 public:
  ModelBrokerAndroidFeatureList() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kOptimizationGuideModelExecution, {}},
            {features::kOptimizationGuideOnDeviceModel, {}},
        },
        {features::kRequirePersistentModeForScamDetection});
  }
  ~ModelBrokerAndroidFeatureList() = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ModelBrokerAndroidFeatureDisabledList {
 public:
  ModelBrokerAndroidFeatureDisabledList() {
    feature_list_.InitWithFeaturesAndParameters(
        {}, {
                {features::kOptimizationGuideModelExecution},
                {features::kOptimizationGuideOnDeviceModel},
            });
  }
  ~ModelBrokerAndroidFeatureDisabledList() = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class RequirePersistentModeForScamDetectionEnabledFeatureList {
 public:
  RequirePersistentModeForScamDetectionEnabledFeatureList() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kOptimizationGuideModelExecution, {}},
            {features::kOptimizationGuideOnDeviceModel, {}},
            {features::kRequirePersistentModeForScamDetection, {}},
        },
        {});
  }
  ~RequirePersistentModeForScamDetectionEnabledFeatureList() = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ModelBrokerAndroidTest : public testing::Test {
 public:
  ModelBrokerAndroidTest() { java_helper_.SetMockAiCoreFactory(); }
  ~ModelBrokerAndroidTest() override = default;

  ModelBrokerAndroid& EnsureBroker() {
    if (!broker_) {
      broker_.emplace(local_state_.local_state(), provider_);
    }
    return *broker_;
  }

  mojo::PendingRemote<mojom::ModelBroker> BindAndPassRemote() {
    mojo::PendingRemote<mojom::ModelBroker> remote;
    EnsureBroker().BindModelBroker(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void InstallTestFeatureConfig() {
    provider_.UpdateModelImmediatelyForTesting(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
        std::make_unique<ModelInfo>(test_asset_.model_info()));
  }

  void InstallScamDetectionFeatureConfig() {
    provider_.UpdateModelImmediatelyForTesting(
        proto::OptimizationTarget::OPTIMIZATION_TARGET_MODEL_VALIDATION,
        std::make_unique<ModelInfo>(scam_detection_asset_.model_info()));
  }

  std::unique_ptr<OnDeviceSession> DownloadModelAndCreateSession(
      ModelBrokerClient& client,
      mojom::OnDeviceFeature feature) {
    // Requesting the feature we've provided assets for should succeed.
    base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
    client.CreateSession(feature, SessionConfigParams{}, future.GetCallback());
    base::test::RunUntil([&]() {
      return client.GetSubscriber(feature).unavailable_reason() ==
             mojom::ModelUnavailableReason::kPendingAssets;
    });
    java_helper_.TriggerDownloaderOnAvailable(spec_.model_name,
                                              spec_.model_version);
    return future.Take();
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
      .config = UnsafeFeatureConfig(mojom::OnDeviceFeature::kTest),
      .metadata = MatchingMetadata(spec_),
  }};
  FakeAdaptationAsset scam_detection_asset_{{
      .config = UnsafeFeatureConfig(mojom::OnDeviceFeature::kScamDetection),
      .metadata = MatchingMetadata(spec_),
  }};
};

TEST_F(ModelBrokerAndroidTest, RequirePersistentModeForTest) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });

  java_helper_.VerifyDownloaderParams(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST,
      /*require_persistent_mode=*/true);
}

TEST_F(ModelBrokerAndroidTest, DoesNotRequirePersistentModeForScamDetection) {
  InstallScamDetectionFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kScamDetection,
                       SessionConfigParams{}, future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kScamDetection)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });

  java_helper_.VerifyDownloaderParams(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
      /*require_persistent_mode=*/false);
}

// Verify that when requesting a session while assets are still pending, the
// client will wait for the assets before resolving the callback.
TEST_F(ModelBrokerAndroidTest, PendingClient) {
  ModelBrokerClient client(BindAndPassRemote(), nullptr);
  // Requesting test feature, but assets not available.
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
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

// Verify that CreateSession and ExecuteModel works when the download succeeds.
TEST_F(ModelBrokerAndroidTest, ExecuteModel) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(), nullptr);

  auto session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kTest);
  ASSERT_TRUE(session);

  proto::ExampleForTestingRequest context_request;
  context_request.set_string_value("some context ");
  session->AddContext(context_request);

  ResponseHolder response;
  proto::ExampleForTestingRequest request;
  request.set_string_value("some input");
  session->ExecuteModel(request, response.GetStreamingCallback());

  ASSERT_TRUE(response.GetFinalStatus());
  EXPECT_FALSE(response.error().has_value());
  // MockAiCoreFactory returns the input string as the response.
  EXPECT_EQ(*response.value(), "some context some input");

  auto* model_execution_info = response.model_execution_info();
  ASSERT_TRUE(model_execution_info);
  auto& on_device_model_service_version =
      model_execution_info->on_device_model_execution_info()
          .model_versions()
          .on_device_model_service_version();
  EXPECT_EQ(on_device_model_service_version.model_adaptation_version(),
            test_asset_.version());
  EXPECT_EQ(on_device_model_service_version.on_device_base_model_metadata()
                .base_model_name(),
            spec_.model_name);
  EXPECT_EQ(on_device_model_service_version.on_device_base_model_metadata()
                .base_model_version(),
            spec_.model_version);
}

// Verify that ExecuteModel succeeds after the model is disconnected.
TEST_F(ModelBrokerAndroidTest, ExecuteModelAfterModelDisconnected) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(), nullptr);

  auto session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kTest);
  ASSERT_TRUE(session);

  // Fast forward time to trigger idle timeout.
  task_environment_.FastForwardBy(on_device_model::kDefaultModelIdleTimeout +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();

  ResponseHolder response;
  proto::ExampleForTestingRequest request;
  request.set_string_value("some input");
  session->ExecuteModel(request, response.GetStreamingCallback());

  // The execution still succeeds even though the model is disconnected after
  // the session is created. This is because OnDeviceExecution will clone a
  // new session from OnDeviceContext, which creates a new session if the old
  // one was disconnected.
  ASSERT_TRUE(response.GetFinalStatus());
  EXPECT_EQ(*response.value(), "some input");
}

// Verify that when download fails, the client is notified.
TEST_F(ModelBrokerAndroidTest, DownloadFailure) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(), nullptr);

  // Requesting the feature we've provided assets for should fail.
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  java_helper_.TriggerDownloaderOnUnavailable(
      on_device_model::ModelDownloaderAndroid::DownloadFailureReason::
          kUnknownError);
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kNotSupported;
  });
  EXPECT_EQ(future.Get(), nullptr);
}

// Verify that model is disabled when the enterprise policy is set to disallow.
TEST_F(ModelBrokerAndroidTest, EnterprisePolicyDisallowsModel) {
  local_state_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));
  InstallTestFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kNotSupported;
  });
  EXPECT_EQ(future.Get(), nullptr);
}

// Verify that model download is triggered for a feature that has recently
// been used.
TEST_F(ModelBrokerAndroidTest, DownloadSuccessForAlreadyUsedFeature) {
  InstallTestFeatureConfig();
  model_execution::prefs::RecordFeatureUsage(&local_state_.local_state(),
                                             mojom::OnDeviceFeature::kTest);
  task_environment_.FastForwardBy(
      features::GetOnDeviceEligibleModelFeatureRecentUsePeriod() -
      base::Days(1));

  ModelBrokerClient client(BindAndPassRemote(), nullptr);
  auto session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kTest);
  ASSERT_TRUE(session);
}

class ModelBrokerAndroidRequirePersistentModeEnabledTest
    : public ModelBrokerAndroidTest {
 public:
  ModelBrokerAndroidRequirePersistentModeEnabledTest() = default;
  ~ModelBrokerAndroidRequirePersistentModeEnabledTest() override = default;

 private:
  RequirePersistentModeForScamDetectionEnabledFeatureList feature_list_;
};

TEST_F(ModelBrokerAndroidRequirePersistentModeEnabledTest,
       RequirePersistentModeForScamDetection) {
  InstallScamDetectionFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kScamDetection,
                       SessionConfigParams{}, future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kScamDetection)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });

  java_helper_.VerifyDownloaderParams(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
      /*require_persistent_mode=*/true);
}

class ModelBrokerAndroidFeatureDisabledTest : public ModelBrokerAndroidTest {
 public:
  ModelBrokerAndroidFeatureDisabledTest() = default;
  ~ModelBrokerAndroidFeatureDisabledTest() override = default;

 private:
  ModelBrokerAndroidFeatureDisabledList feature_list_;
};

TEST_F(ModelBrokerAndroidFeatureDisabledTest, FeatureDisabled) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kNotSupported;
  });
  EXPECT_EQ(future.Get(), nullptr);
}

}  // namespace

}  // namespace optimization_guide
