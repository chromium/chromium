// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/task/current_thread.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/android/android_manifest.h"
#include "components/optimization_guide/core/model_execution/configs/summarize_solution_config.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/optimization_guide/core/model_execution/test/fake_android_manifest_asset.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker_android.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/mock_download_progress_observer.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
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

proto::OnDeviceModelExecutionFeatureConfig UnsafeFeatureConfig(
    mojom::OnDeviceFeature feature) {
  proto::OnDeviceModelExecutionFeatureConfig cfg = SimpleTestFeatureConfig();
  cfg.set_feature(ToModelExecutionFeatureProto(feature));
  return cfg;
}

std::unique_ptr<FakeAndroidManifestAsset> MakeSummarizerManifestAsset(
    uint32_t max_tokens) {
  proto::SolutionConfig config;
  *config.mutable_feature() =
      UnsafeFeatureConfig(mojom::OnDeviceFeature::kSummarize);
  config.mutable_feature()->mutable_input_config()->set_max_execute_tokens(
      max_tokens);
  auto asset = std::make_unique<FakeAndroidManifestAsset>();
  asset->AddSolutionConfig(ToUseCaseName(mojom::OnDeviceFeature::kSummarize),
                           std::move(config));
  return asset;
}

class ModelBrokerAndroidFeatureDisabledList {
 public:
  ModelBrokerAndroidFeatureDisabledList() {
    feature_list_.InitWithFeaturesAndParameters(
        {}, {
                {features::kOptimizationGuideModelExecution},
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
  ModelBrokerAndroidTest() {
    // Most tests explicitly drive a single feature download. Avoid creating
    // metadata downloaders for every installed AICore model unless a status
    // test opts into that behavior.
    fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
        ModelStatus::kApiNotAvailable);
  }
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
  OnDeviceBaseModelSpec spec_{.model_name = "Test", .model_version = "0.0.1"};
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
  UsageTracker(&fake_broker_.local_state())
      .RaisePriority(ToUseCaseName(mojom::OnDeviceFeature::kTest),
                     UsageTracker::Priority::kBestEffort);
  task_environment_.FastForwardBy(kRecentUsePeriod.Get() - base::Days(1));

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

TEST_F(ModelBrokerAndroidTest, BuiltinConfigProvidesExpectedTokenLimits) {
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  auto session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kSummarize);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->GetTokenLimits().min_context_tokens, 0u);
  EXPECT_EQ(session->GetTokenLimits().max_context_tokens,
            kAndroidMaxExecuteTokens);
  EXPECT_EQ(session->GetTokenLimits().max_execute_tokens,
            kAndroidMaxExecuteTokens);
  EXPECT_EQ(session->GetTokenLimits().max_output_tokens,
            kAndroidMaxOutputTokens);
}

TEST_F(ModelBrokerAndroidTest, ManifestConfigDoesNotWaitForAdaptation) {
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kSummarize,
                       SessionConfigParams{}, future.GetCallback());

  fake_broker_.java_helper().TriggerDownloaderOnAvailable(spec_.model_name,
                                                          spec_.model_version);
  const auto target =
      GetOptimizationTargetForFeature(mojom::OnDeviceFeature::kSummarize);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_broker_.model_provider()
        .GetRegistrationMetadata(target)
        .has_value();
  }));

  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());
}

TEST_F(ModelBrokerAndroidTest, ComponentConfigAppliesBeforeFirstUse) {
  // Storing a config before first use should not start AICore status checks.
  fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
      std::nullopt);
  fake_broker_.UpdateManifest(MakeSummarizerManifestAsset(1234));
  ASSERT_EQ(fake_broker_.java_helper().GetStatusCheckerCount(), 0);

  // The first session should use the stored component config when the solution
  // factory is initialized.
  fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
      ModelStatus::kDownloadable);
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kSummarize,
                       SessionConfigParams{}, future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return client.GetSubscriber(mojom::OnDeviceFeature::kSummarize)
               .unavailable_reason() ==
           mojom::ModelUnavailableReason::kPendingUsage;
  }));
  fake_broker_.java_helper().TriggerDownloaderOnAvailable(spec_.model_name,
                                                          spec_.model_version);
  auto session = future.Take();
  ASSERT_TRUE(session);
  EXPECT_EQ(session->GetTokenLimits().max_execute_tokens, 1234u);
}

TEST_F(ModelBrokerAndroidTest, FailedManifestLoadPreservesCurrentSolution) {
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  auto session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kSummarize);
  ASSERT_TRUE(session);
  auto& subscriber = client.GetSubscriber(mojom::OnDeviceFeature::kSummarize);
  auto original_client = subscriber.client()->GetWeakPtr();

  fake_broker_.broker().OnManifestLoaded(
      base::unexpected(AndroidManifest::ParseError::kInvalidConfig));

  // Verify the existing session still works.
  base::test::TestFuture<std::optional<uint32_t>> token_count;
  session->GetSizeInTokens("input", token_count.GetCallback());
  ASSERT_EQ(token_count.Get(), 5u);
  ASSERT_TRUE(original_client);
  ASSERT_TRUE(subscriber.client().has_value());
  EXPECT_EQ(subscriber.client()->GetWeakPtr().get(), original_client.get());
  EXPECT_EQ(subscriber.client()->token_limits().max_execute_tokens,
            kAndroidMaxExecuteTokens);
}

TEST_F(ModelBrokerAndroidTest,
       ManifestReplacementPreservesAdaptationBackedSession) {
  // Scam Detection gets its config from an adaptation, not the manifest.
  // Replacing the manifest must preserve its client and allow the same session
  // to execute again using the original adaptation.
  InstallScamDetectionFeatureConfig();
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  auto session = DownloadModelAndCreateSession(
      client, mojom::OnDeviceFeature::kScamDetection);
  ASSERT_TRUE(session);
  auto& subscriber =
      client.GetSubscriber(mojom::OnDeviceFeature::kScamDetection);
  auto original_client = subscriber.client()->GetWeakPtr();
  ASSERT_TRUE(original_client);

  proto::ExampleForTestingRequest request;
  request.set_string_value("legacy response");
  ResponseHolder initial_response;
  session->ExecuteModel(request, initial_response.GetStreamingCallback());
  ASSERT_TRUE(initial_response.GetFinalStatus());
  EXPECT_EQ(initial_response.value(), "legacy response");

  fake_broker_.UpdateManifest(MakeSummarizerManifestAsset(1234));

  // Unlike manifest-backed sessions, the existing adaptation-backed session
  // can start another execution after the snapshot is replaced.
  ResponseHolder response;
  session->ExecuteModel(request, response.GetStreamingCallback());
  ASSERT_TRUE(response.GetFinalStatus());
  EXPECT_EQ(response.value(), "legacy response");
  ASSERT_TRUE(original_client);
  ASSERT_TRUE(subscriber.client().has_value());
  EXPECT_EQ(subscriber.client()->GetWeakPtr().get(), original_client.get());
  ASSERT_TRUE(response.model_execution_info());
  EXPECT_EQ(response.model_execution_info()
                ->on_device_model_execution_info()
                .model_versions()
                .on_device_model_service_version()
                .model_adaptation_version(),
            scam_detection_asset_.version());
}

TEST_F(ModelBrokerAndroidTest, ComponentConfigReplacesBuiltinForNewSessions) {
  constexpr uint32_t kComponentMaxExecuteTokens = 1234;

  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  auto builtin_session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kSummarize);
  ASSERT_TRUE(builtin_session);
  EXPECT_EQ(builtin_session->GetTokenLimits().max_execute_tokens,
            kAndroidMaxExecuteTokens);
  auto& subscriber = client.GetSubscriber(mojom::OnDeviceFeature::kSummarize);

  fake_broker_.UpdateManifest(
      MakeSummarizerManifestAsset(kComponentMaxExecuteTokens));

  // Wait for the replacement to reach the subscriber through Mojo. Its token
  // limit distinguishes the component config from the previous built-in config.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return subscriber.client().has_value() &&
           subscriber.client()->token_limits().max_execute_tokens ==
               kComponentMaxExecuteTokens;
  }));

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kSummarize,
                       SessionConfigParams{}, future.GetCallback());
  auto component_session = future.Take();
  // The new component session is ready and should reflect the component
  // manifest's token limits.
  ASSERT_TRUE(component_session);
  EXPECT_EQ(component_session->GetTokenLimits().max_execute_tokens,
            kComponentMaxExecuteTokens);

  // The old builtin session should not be able to execute successfully because
  // it is tied to the replaced built-in client.
  proto::ExampleForTestingRequest request;
  request.set_string_value("component input");
  ResponseHolder stale_response;
  builtin_session->ExecuteModel(request, stale_response.GetStreamingCallback());
  EXPECT_FALSE(stale_response.GetFinalStatus());
  EXPECT_EQ(stale_response.error(), OnDeviceError::kGenericFailure);

  // The new component session should execute successfully.
  ResponseHolder response;
  component_session->ExecuteModel(request, response.GetStreamingCallback());
  ASSERT_TRUE(response.GetFinalStatus());
  EXPECT_EQ(response.value(), "component input");
}

// When a new component snapshot omits the Summarizer override, new sessions
// should use the built-in config for the omitted feature.
TEST_F(ModelBrokerAndroidTest,
       OmittedComponentConfigFallsBackToBuiltinForNewSessions) {
  fake_broker_.UpdateManifest(MakeSummarizerManifestAsset(1234));
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  auto component_session =
      DownloadModelAndCreateSession(client, mojom::OnDeviceFeature::kSummarize);
  ASSERT_TRUE(component_session);
  ASSERT_EQ(component_session->GetTokenLimits().max_execute_tokens, 1234u);
  auto& subscriber = client.GetSubscriber(mojom::OnDeviceFeature::kSummarize);
  auto old_client = subscriber.client()->GetWeakPtr();
  ASSERT_TRUE(old_client);

  // The next component snapshot still has configs, but no Summarizer override.
  auto asset = std::make_unique<FakeAndroidManifestAsset>();
  proto::SolutionConfig config;
  *config.mutable_feature() =
      UnsafeFeatureConfig(mojom::OnDeviceFeature::kTest);
  asset->AddSolutionConfig(ToUseCaseName(mojom::OnDeviceFeature::kTest),
                           std::move(config));
  fake_broker_.UpdateManifest(std::move(asset));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return subscriber.client().has_value() &&
           subscriber.client()->token_limits().max_execute_tokens ==
               kAndroidMaxExecuteTokens;
  }));
  EXPECT_FALSE(old_client);

  base::test::TestFuture<ModelBrokerClient::CreateSessionResult> future;
  client.CreateSession(mojom::OnDeviceFeature::kSummarize,
                       SessionConfigParams{}, future.GetCallback());
  auto builtin_session = future.Take();
  ASSERT_TRUE(builtin_session);
  EXPECT_EQ(builtin_session->GetTokenLimits().max_execute_tokens,
            kAndroidMaxExecuteTokens);
}

TEST_F(ModelBrokerAndroidTest, ProvidesSummarizerFeatureRoutingConfig) {
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);

  base::test::TestFuture<std::optional<mojo_base::ProtoWrapper>>
      summarizer_future;
  client.GetConfig(mojom::OnDeviceFeature::kSummarize,
                   summarizer_future.GetCallback());
  auto summarizer_wrapper = summarizer_future.Take();
  ASSERT_TRUE(summarizer_wrapper);
  auto summarizer_any = summarizer_wrapper->As<proto::Any>();
  ASSERT_TRUE(summarizer_any);
  proto::SummarizerFeatureConfig summarizer_config;
  ASSERT_TRUE(summarizer_config.ParseFromString(summarizer_any->value()));
  EXPECT_EQ(summarizer_config.default_use_case(),
            ToUseCaseName(mojom::OnDeviceFeature::kSummarize));
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

TEST_F(ModelBrokerAndroidStatusCheckTest,
       AvailableStatusFetchesModelSpecWithoutPriorUsage) {
  fake_broker_.java_helper().settings().SetDefaultStatusCheckResult(
      ModelStatus::kAvailable);
  ModelBrokerClient client(fake_broker_.BindAndPassRemote(), nullptr);
  // Subscribing does not raise usage priority, so only the kAvailable status
  // path can start the metadata request that retrieves the model spec.
  auto& subscriber = client.GetSubscriber(mojom::OnDeviceFeature::kSummarize);

  fake_broker_.java_helper().TriggerDownloaderOnAvailable(spec_.model_name,
                                                          spec_.model_version);

  // The returned BaseModelSpec makes the summarize solution available.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !subscriber.unavailable_reason().has_value(); }));
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
