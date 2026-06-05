// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"

#include <optional>

#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker_android.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/mock_download_progress_observer.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ModelStatus = on_device_model::ModelDownloaderAndroid::ModelStatus;

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
            {features::kAICorePrompt, {}},
            {features::kAICoreScamDetection, {}},
            {features::kAICoreTest, {}},
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
  ModelBrokerAndroidTest() = default;
  ~ModelBrokerAndroidTest() override = default;

  void InstallTestFeatureConfig() {
    fake_broker_.UpdateModelAdaptation(test_asset_);
  }

  void InstallScamDetectionFeatureConfig() {
    fake_broker_.UpdateModelAdaptation(scam_detection_asset_);
  }

  std::unique_ptr<OnDeviceSession> DownloadModelAndCreateSession(
      ModelBrokerClient& client,
      mojom::OnDeviceFeature feature) {
    base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
    client.CreateSession(feature, SessionConfigParams{}, future.GetCallback());
    base::test::RunUntil([&]() {
      return client.GetSubscriber(feature).unavailable_reason() ==
             mojom::ModelUnavailableReason::kPendingAssets;
    });
    fake_broker_.java_helper().TriggerDownloaderOnAvailable(
        spec_.model_name, spec_.model_version);
    return future.Take();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  OnDeviceBaseModelSpec spec_{
      "Test", "0.0.1", proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_UNSPECIFIED};
  FakeModelBrokerAndroid fake_broker_{{
      .metadata = MatchingMetadata(spec_),
      .preinstall_base_model = false,
  }};
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
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });

  fake_broker_.java_helper().VerifyDownloaderParams(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TEST,
      /*require_persistent_mode=*/true);
}

TEST_F(ModelBrokerAndroidTest, DoesNotRequirePersistentModeForScamDetection) {
  InstallScamDetectionFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kScamDetection,
                       SessionConfigParams{}, future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kScamDetection)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });

  fake_broker_.java_helper().VerifyDownloaderParams(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_SCAM_DETECTION,
      /*require_persistent_mode=*/false);
}

// Verify that when requesting a session while assets are still pending, the
// client will wait for the assets before resolving the callback.
TEST_F(ModelBrokerAndroidTest, PendingClient) {
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
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
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

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
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

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
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  // Requesting the feature we've provided assets for should fail.
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  fake_broker_.java_helper().TriggerDownloaderOnUnavailable(
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
  fake_broker_.local_state().SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));
  InstallTestFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

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
  model_execution::prefs::RecordFeatureUsage(&fake_broker_.local_state(),
                                             mojom::OnDeviceFeature::kTest);
  task_environment_.FastForwardBy(
      features::GetOnDeviceEligibleModelFeatureRecentUsePeriod() -
      base::Days(1));

  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  auto session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kTest);
  ASSERT_TRUE(session);
}

// Verify that download progress updates are forwarded to observers, and that a
// late-joining observer receives an initial zero-progress event.
TEST_F(ModelBrokerAndroidTest, DownloadProgressObserver) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  // Add the first observer and request a session to trigger the download.
  MockDownloadProgressObserver observer1;
  client.AddModelDownloadProgressObserver(
      ToUseCaseName(mojom::OnDeviceFeature::kTest),
      observer1.BindNewPipeAndPassRemote());

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });

  // Trigger download progress — observer1 should receive the normalized update.
  fake_broker_.java_helper().TriggerDownloaderOnDownloadProgress(500, 1000);
  observer1.ExpectReceivedNormalizedUpdate(500, 1000);

  // Add a second observer after download progress has started — it should
  // receive the initial (0, max) event.
  MockDownloadProgressObserver observer2;
  client.AddModelDownloadProgressObserver(
      ToUseCaseName(mojom::OnDeviceFeature::kTest),
      observer2.BindNewPipeAndPassRemote());
  observer2.ExpectReceivedUpdate(0, kNormalizedDownloadProgressMax);
}

// Verify that an observer added after the model download has completed receives
// 0% and 100% progress updates, matching desktop behavior.
TEST_F(ModelBrokerAndroidTest, DownloadProgressObserverAfterDownloadComplete) {
  InstallTestFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  auto session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kTest);
  ASSERT_TRUE(session);

  MockDownloadProgressObserver observer;
  client.AddModelDownloadProgressObserver(
      ToUseCaseName(mojom::OnDeviceFeature::kTest),
      observer.BindNewPipeAndPassRemote());
  observer.ExpectReceivedUpdate(0, kNormalizedDownloadProgressMax);
  observer.ExpectReceivedUpdate(kNormalizedDownloadProgressMax,
                                kNormalizedDownloadProgressMax);
}

// Test fixture for verifying model status check behavior with specific
// statuses set per test.
class ModelBrokerAndroidStatusCheckTest : public ModelBrokerAndroidTest {
 public:
  ModelBrokerAndroidStatusCheckTest() = default;
  ~ModelBrokerAndroidStatusCheckTest() override = default;
};

// Verify that when model status is unavailable, the subscriber gets
// kNotSupported and the session future resolves to nullptr.
TEST_F(ModelBrokerAndroidStatusCheckTest, ModelStatusUnavailable) {
  fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
      ModelStatus::kUnavailable);
  InstallTestFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

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

// Verify that when model status is downloading, the subscriber gets
// kPendingAssets.
TEST_F(ModelBrokerAndroidStatusCheckTest, ModelStatusDownloading) {
  fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
      ModelStatus::kDownloading);
  InstallTestFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  EXPECT_FALSE(future.IsReady());
}

// Verify that when model status is downloadable, the subscriber gets
// kPendingUsage.
TEST_F(ModelBrokerAndroidStatusCheckTest, ModelStatusDownloadable) {
  fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
      ModelStatus::kDownloadable);
  InstallTestFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingUsage;
  });
  EXPECT_FALSE(future.IsReady());
}

// Verify that when model status is kApiNotAvailable (e.g., Chrome-branded
// builds without MLKit), the flow continues as if the check passed. The
// subscriber should reach kPendingAssets since the model adaptation may still
// need to be downloaded.
TEST_F(ModelBrokerAndroidStatusCheckTest, ModelStatusApiNotAvailable) {
  fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
      ModelStatus::kApiNotAvailable);
  InstallTestFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  EXPECT_FALSE(future.IsReady());
}

// Verify that the init callback is not fired until all AICore features' status
// checks complete (BarrierClosure).
TEST_F(ModelBrokerAndroidStatusCheckTest, BarrierWaitsForAllStatusChecks) {
  // Clear auto-respond so status checks wait for explicit triggering.
  fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
      std::nullopt);
  InstallTestFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{},
                       future.GetCallback());

  // CheckModelStatus creates one status checker per unique AICore
  // feature. With kAICorePrompt and kScamDetection enabled plus kAICoreTest
  // enabled in the test feature list, the unique AICore features are: PROMPT
  // (covers kPromptApi, kSummarize), SCAM_DETECTION (kScamDetection), and TEST.
  base::test::RunUntil([&]() {
    return fake_broker_.java_helper().GetStatusCheckerCount() == 3;
  });

  // Complete status checks for all AICore features. The barrier fires,
  // solutions are updated, and the subscriber for kTest feature is added.
  fake_broker_.java_helper().TriggerAllDownloadersOnStatusCheckResult(
      ModelStatus::kAvailable);

  // All status checkers should have been destroyed after completion.
  EXPECT_EQ(fake_broker_.java_helper().GetStatusCheckerCount(), 0);
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kTest)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });
  EXPECT_FALSE(future.IsReady());
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
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kScamDetection,
                       SessionConfigParams{}, future.GetCallback());
  base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kScamDetection)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingAssets;
  });

  fake_broker_.java_helper().VerifyDownloaderParams(
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
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

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
