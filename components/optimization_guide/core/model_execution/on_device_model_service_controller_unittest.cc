// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/model_broker_state.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_execution.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/on_device_base_model_metadata.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/redaction.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/cpp/features.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ::on_device_model::mojom::LoadModelResult;
using ExecuteModelResult = ::optimization_guide::OnDeviceExecution::Result;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::ResultOf;

auto UnsafeComposeConfig() {
  auto cfg = SimpleComposeConfig();
  cfg.set_can_skip_text_safety(true);
  return cfg;
}

auto UnsafeTestConfig() {
  auto cfg = UnsafeComposeConfig();
  cfg.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  return cfg;
}

// A complete set of assets for the most common case.
struct StandardAssets {
  FakeBaseModelAsset::Content base_model_content;
  FakeAdaptationAsset compose{{
      .config = SimpleComposeConfig(),
  }};
  FakeSafetyModelAsset safety{ComposeSafetyConfig()};
  FakeLanguageModelAsset language;
};

const std::string& GetCheckText(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.request().text_safety_model_request().text();
}

class FakeOnDeviceModelAvailabilityObserver
    : public OnDeviceModelAvailabilityObserver {
 public:
  explicit FakeOnDeviceModelAvailabilityObserver(
      mojom::OnDeviceFeature expected_feature) {
    expected_feature_ = expected_feature;
  }

  void OnDeviceModelAvailabilityChanged(
      mojom::OnDeviceFeature feature,
      OnDeviceModelEligibilityReason reason) override {
    EXPECT_EQ(expected_feature_, feature);
    reason_ = reason;
  }
  mojom::OnDeviceFeature expected_feature_;
  std::optional<OnDeviceModelEligibilityReason> reason_;
};

}  // namespace

std::string ConcatResponses(const std::vector<std::string>& responses) {
  std::string concat_responses;
  for (const std::string& response : responses) {
    concat_responses += response;
  }
  return concat_responses;
}

constexpr auto kFeature = mojom::OnDeviceFeature::kCompose;

class OnDeviceModelServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kOptimizationGuideOnDeviceModel,
          {{"on_device_model_topk", "1"},
           {"on_device_model_temperature", "0"},
           {"on_device_model_disable_crash_count", "3"},
           {"on_device_model_crash_backoff_base_time", "1m"},
           {"on_device_model_max_crash_backoff_time", "1h"}}},
         {features::kOnDeviceModelPerformanceParams,
          {{"compatible_on_device_performance_classes", "*"},
           {"compatible_low_tier_on_device_performance_classes", "3"}}},
         {features::kTextSafetyClassifier, {}},
         {features::kOnDeviceModelValidation,
          {{"on_device_model_validation_delay", "0"}}}},
        {});
    // Mark a feature used so the model is eligible to install.
    model_execution::prefs::RecordFeatureUsage(
        &broker_.local_state(), mojom::OnDeviceFeature::kCompose);
    model_execution::prefs::RecordFeatureUsage(&broker_.local_state(),
                                               mojom::OnDeviceFeature::kTest);
  }

  struct InitializeParams {
    std::optional<FakeBaseModelAsset::Content> base_model_content;
    raw_ptr<FakeSafetyModelAsset> safety;
    raw_ptr<FakeLanguageModelAsset> language;
    std::vector<FakeAdaptationAsset*> adaptations;
    bool instantiate_broker = true;
  };

  void Initialize(const InitializeParams& params) {
    if (params.base_model_content) {
      broker_.InstallBaseModel(
          std::make_unique<FakeBaseModelAsset>(*params.base_model_content));
    }
    if (params.safety) {
      broker_.UpdateSafetyModel(*params.safety);
    }
    if (params.language) {
      broker_.UpdateLanguageDetectionModel(*params.language);
    }
    for (auto* adaptation : params.adaptations) {
      broker_.UpdateModelAdaptation(*adaptation);
    }
    if (params.instantiate_broker) {
      broker_.GetOrCreateBrokerState();  // Force instantiation.
      // Wait for configs to be read from disk.
      task_environment_.RunUntilIdle();
    }
  }

  void Initialize(StandardAssets& assets) {
    Initialize(InitializeParams{
        .base_model_content = standard_assets_.base_model_content,
        .safety = &standard_assets_.safety,
        .language = &standard_assets_.language,
        .adaptations = {&standard_assets_.compose},
    });
  }

  void SimulateShutdown() {
    broker_.SimulateShutdown();
    broker_.launcher().CrashService();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  std::unique_ptr<OnDeviceSession> CreateSession(
      const SessionConfigParams& params) {
    return broker_.GetOrCreateBrokerState().StartSession(kFeature, params,
                                                         logger_.GetWeakPtr());
  }
  std::unique_ptr<OnDeviceSession> CreateSession(
      mojom::OnDeviceFeature feature,
      const SessionConfigParams& params) {
    return broker_.GetOrCreateBrokerState().StartSession(feature, params,
                                                         logger_.GetWeakPtr());
  }

  void ExpectFailedSession(OnDeviceModelEligibilityReason reason) {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(CreateSession(SessionConfigParams{}));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        reason, 1);
  }

  std::string GetResponse(OnDeviceSession& session, const std::string& prompt) {
    ResponseHolder response;
    session.ExecuteModel(PageUrlRequest(prompt),
                         response.GetStreamingCallback());
    EXPECT_TRUE(response.GetFinalStatus());
    return *response.value();
  }

 protected:
  StandardAssets standard_assets_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeModelBroker broker_{{
      .preinstall_base_model = false,
  }};
  ResponseHolder response_;
  base::test::ScopedFeatureList feature_list_;
  OptimizationGuideLogger logger_;
};

TEST_F(OnDeviceModelServiceControllerTest, ScoreBeforeContext) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_NE(score_future.Get(), std::nullopt);
}

TEST_F(OnDeviceModelServiceControllerTest, ScorePresentAfterContext) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));

  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_EQ(score_future.Get(), 0.5);
}

TEST_F(OnDeviceModelServiceControllerTest, ScoreAfterExecute) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));
  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());

  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_NE(score_future.Get(), std::nullopt);
}

TEST_F(OnDeviceModelServiceControllerTest, BaseModelExecutionSuccess) {
  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfig(),
      // No weight, so will use base model.
  });
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });

  base::HistogramTester histogram_tester;
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  const std::string expected_response = "execute:foo max:1024";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_TRUE(*response_.provided_by_on_device());
  EXPECT_THAT(response_.partials(), ElementsAre(expected_response));

  EXPECT_TRUE(response_.model_execution_info());
  auto logged_on_device_model_execution_info =
      response_.model_execution_info()->on_device_model_execution_info();
  auto model_version = logged_on_device_model_execution_info.model_versions()
                           .on_device_model_service_version();
  EXPECT_EQ(model_version.component_version(), "0.0.1");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_name(),
            "Test");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_version(),
            "0.0.1");
  EXPECT_EQ(model_version.model_adaptation_version(), compose_asset.version());
  EXPECT_GT(logged_on_device_model_execution_info.execution_infos_size(), 0);
  EXPECT_EQ(logged_on_device_model_execution_info.execution_infos(0)
                .response()
                .on_device_model_service_response()
                .status(),
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_SUCCESS);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kSuccess, 1);

  // If we destroy all sessions and wait long enough, everything should idle out
  // and the service should get terminated.
  session.reset();
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(broker_.launcher().is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest, TokenLimits) {
  auto config = SimpleComposeConfig();
  config.mutable_input_config()->set_min_context_tokens(5);
  config.mutable_input_config()->set_max_context_tokens(5);
  config.mutable_input_config()->set_max_execute_tokens(3);
  config.mutable_output_config()->set_max_output_tokens(1);
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });
  auto session = CreateSession(SessionConfigParams{});
  const TokenLimits& limits = session->GetTokenLimits();
  EXPECT_EQ(limits.max_tokens, 10240u);
  EXPECT_EQ(limits.min_context_tokens, 5u);
  EXPECT_EQ(limits.max_context_tokens, 5u);
  EXPECT_EQ(limits.max_execute_tokens, 3u);
  EXPECT_EQ(limits.max_output_tokens, 1u);
}

TEST_F(OnDeviceModelServiceControllerTest, TokenLimitsCapped) {
  auto config = SimpleComposeConfig();
  config.mutable_input_config()->set_min_context_tokens(100000);
  config.mutable_input_config()->set_max_context_tokens(100000);
  config.mutable_input_config()->set_max_execute_tokens(100000);
  config.mutable_output_config()->set_max_output_tokens(100000);
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });
  auto session = CreateSession(SessionConfigParams{});
  const TokenLimits& limits = session->GetTokenLimits();
  EXPECT_EQ(limits.max_tokens, 10240u);
  EXPECT_EQ(limits.min_context_tokens, 10240u);
  EXPECT_EQ(limits.max_context_tokens, 10240u);
  EXPECT_EQ(limits.max_execute_tokens, 10240u);
  EXPECT_EQ(limits.max_output_tokens, 10240u);
}

TEST_F(OnDeviceModelServiceControllerTest, CacheWeightExecutionSuccess) {
  // TODO(crbug.com/400998489): Cache files are experimental for now. Stop
  // setting this feature flag once that's no longer the case.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOptimizationGuideOnDeviceModel,
        {{"on_device_model_topk", "1"}, {"on_device_model_temperature", "0"}}},
       {on_device_model::features::kOnDeviceModelForceCpuBackend, {}}},
      {});

  Initialize(InitializeParams{
      .base_model_content =
          FakeBaseModelAsset::Content{
              .cache_weight = 1015,
              .encoder_cache_weight = 1016,
              .adapter_cache_weight = 1017,
          },
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(),
            "CPU backendCache weight: 1015Encoder cache weight: 1016"
            "Adapter cache weight: 1017execute:foo max:1024");

  // If we destroy all sessions and wait long enough, everything should idle out
  // and the service should get terminated.
  session.reset();
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(broker_.launcher().is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest, AdaptationModelExecutionSuccess) {
  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfig(),
      .weight = 1015,
  });
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "Adaptation model: 1015execute:foo max:1024");

  // If we destroy all sessions and wait long enough, everything should idle out
  // and the service should get terminated.
  session.reset();
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(broker_.launcher().is_service_running());
}

// Sessions using different adaptations should be able to execute
// concurrently.
TEST_F(OnDeviceModelServiceControllerTest,
       MultipleModelAdaptationExecutionSuccess) {
  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfig(),
      .weight = 1015,
  });
  FakeAdaptationAsset test_asset({
      .config = UnsafeTestConfig(),
      .weight = 2024,
  });
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset, &test_asset},
  });

  auto session_compose =
      CreateSession(mojom::OnDeviceFeature::kCompose, SessionConfigParams{});
  ASSERT_TRUE(session_compose);
  auto session_test =
      CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{});
  ASSERT_TRUE(session_test);

  ResponseHolder compose_response;
  session_compose->ExecuteModel(PageUrlRequest("foo"),
                                compose_response.GetStreamingCallback());
  ResponseHolder test_response;
  session_test->ExecuteModel(PageUrlRequest("bar"),
                             test_response.GetStreamingCallback());

  ASSERT_TRUE(compose_response.GetFinalStatus());
  EXPECT_EQ(*compose_response.value(),
            "Adaptation model: 1015execute:foo max:1024");
  EXPECT_TRUE(*compose_response.provided_by_on_device());
  ASSERT_TRUE(test_response.GetFinalStatus());
  EXPECT_EQ(*test_response.value(),
            "Adaptation model: 2024execute:bar max:1024");
  EXPECT_TRUE(*test_response.provided_by_on_device());

  session_compose.reset();
  session_test.reset();

  // Fast forward by the amount of time that triggers an idle disconnect. All
  // adaptations and the base model should be reset.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(broker_.launcher().is_service_running());
}

// A session using the base model should be able to execute concurrently
// with one using and adaptation.
TEST_F(OnDeviceModelServiceControllerTest, ModelAdaptationAndBaseModelSuccess) {
  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfig(),
      .weight = 1015,
  });
  FakeAdaptationAsset test_asset({
      .config = UnsafeTestConfig(),
      // no weight, will use base model.
  });
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset, &test_asset},
  });

  auto session_compose =
      CreateSession(mojom::OnDeviceFeature::kCompose, SessionConfigParams{});
  ASSERT_TRUE(session_compose);
  auto session_test =
      CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{});
  ASSERT_TRUE(session_test);

  ResponseHolder compose_response;
  session_compose->ExecuteModel(PageUrlRequest("foo"),
                                compose_response.GetStreamingCallback());
  ResponseHolder test_response;
  session_test->ExecuteModel(PageUrlRequest("bar"),
                             test_response.GetStreamingCallback());

  ASSERT_TRUE(compose_response.GetFinalStatus());
  EXPECT_EQ(*compose_response.value(),
            "Adaptation model: 1015execute:foo max:1024");
  EXPECT_TRUE(*compose_response.provided_by_on_device());
  ASSERT_TRUE(test_response.GetFinalStatus());
  EXPECT_EQ(*test_response.value(), "execute:bar max:1024");
  EXPECT_TRUE(*test_response.provided_by_on_device());

  session_compose.reset();
  session_test.reset();

  // If we wait long enough, everything should idle out and the service should
  // get terminated. This requires 2 idle timeout intervals (one for the
  // adaptation and one for the base model).
  task_environment_.FastForwardBy(2 * features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(broker_.launcher().is_service_running());
}

// Without a base model available, sessions should fail to be created.
TEST_F(OnDeviceModelServiceControllerTest, BaseModelToBeInstalled) {
  Initialize(InitializeParams{
      .base_model_content = std::nullopt,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  base::HistogramTester histogram_tester;
  auto session = CreateSession(SessionConfigParams{});
  EXPECT_FALSE(session);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kModelToBeInstalled, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, BaseModelAvailableAfterInit) {
  Initialize(InitializeParams{
      .base_model_content = std::nullopt,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  // Model not yet available.
  auto session = CreateSession(SessionConfigParams{});
  EXPECT_FALSE(session);
  broker_.InstallBaseModel(std::make_unique<FakeBaseModelAsset>());
  task_environment_.RunUntilIdle();

  // Model now available.
  session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
}

// Updating the model should not break existing sessions until a new session
// is started.
TEST_F(OnDeviceModelServiceControllerTest, MidSessionModelUpdate) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});

  // Simulate a model update.
  broker_.InstallBaseModel(FakeBaseModelAsset::Content{
      .weight = 2,
  });
  task_environment_.RunUntilIdle();

  // Existing session will fail / fallback to remote.
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(OnDeviceModelServiceControllerTest, SessionBeforeAndAfterModelUpdate) {
  Initialize(standard_assets_);

  auto session1 = CreateSession(SessionConfigParams{});
  session1->AddContext(UserInputRequest("context"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1ull, broker_.launcher().on_device_model_receiver_count());

  // Simulates a model update. This should close the model remote.
  broker_.InstallBaseModel(FakeBaseModelAsset::Content{
      .weight = 2,
  });
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0ull, broker_.launcher().on_device_model_receiver_count());

  // Create a new session and verify it uses the new model.
  auto session2 = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session2);
  ResponseHolder response2;
  session2->ExecuteModel(PageUrlRequest("foo"),
                         response2.GetStreamingCallback());
  ASSERT_TRUE(response2.GetFinalStatus());
  EXPECT_EQ(*response2.value(), "Base model: 2execute:foo max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, SessionFailsForInvalidFeature) {
  Initialize(standard_assets_);
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(
      CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{}));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
      "Test",
      OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, UpdatingSafetyModelEnablesModels) {
  // Verifies that when we start a session before safety is available, that
  // future session that require a safety model still get one.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {features::kTextSafetyClassifier,
           {{"on_device_retract_unsafe_content", "true"}}},
      },
      {});

  FakeAdaptationAsset compose_asset({.config = SimpleComposeConfig()});
  FakeAdaptationAsset test_asset({.config = UnsafeTestConfig()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = nullptr,
      .language = nullptr,
      .adaptations = {&compose_asset, &test_asset},
  });

  // Compose capability can't start because it's missing safety model.
  EXPECT_FALSE(
      CreateSession(mojom::OnDeviceFeature::kCompose, SessionConfigParams{}));

  // Test capability starts because it doesn't require a safety model.
  auto test_session =
      CreateSession(mojom::OnDeviceFeature::kTest, SessionConfigParams{});
  EXPECT_TRUE(test_session);

  // Executing with test_session should force model to be loaded.
  ResponseHolder test_response;
  test_session->ExecuteModel(PageUrlRequest("unsafe"),
                             test_response.GetStreamingCallback());
  EXPECT_TRUE(test_response.GetFinalStatus());

  // Compose capability should be available after safety model loads.
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  broker_.UpdateSafetyModel(safety_asset);
  task_environment_.RunUntilIdle();  // Wait for assets to be read from disk.
  auto compose_session =
      CreateSession(mojom::OnDeviceFeature::kCompose, SessionConfigParams{});
  ASSERT_TRUE(compose_session);

  ResponseHolder compose_response;
  compose_session->ExecuteModel(PageUrlRequest("unsafe"),
                                compose_response.GetStreamingCallback());

  // Compose should run and be rejected as unsafe.
  EXPECT_FALSE(compose_response.GetFinalStatus());
  EXPECT_EQ(
      compose_response.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
}

TEST_F(OnDeviceModelServiceControllerTest, SessionRequiresSafetyModel) {
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = nullptr,
      .language = nullptr,
      .adaptations = {&standard_assets_.compose},
  });

  // No safety model received yet.
  {
    base::HistogramTester histogram_tester;

    EXPECT_FALSE(CreateSession(SessionConfigParams{}));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyModelNotAvailable, 1);
  }

  // Safety model info is valid but no config for feature, session not created
  // successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset(FakeSafetyModelAsset::Content{
        .metadata = SafetyMetadata({[]() {
          auto safety_config = ComposeSafetyConfig();
          safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
          return safety_config;
        }()}),
        .model_info_version = 10,
    });
    broker_.UpdateSafetyModel(safety_asset);
    task_environment_.RunUntilIdle();  // Wait for assets to be read from disk.
    EXPECT_FALSE(CreateSession(SessionConfigParams{}));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyConfigNotAvailableForFeature, 1);
  }

  // Safety model info is valid, session created successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset(FakeSafetyModelAsset::Content{
        .metadata = SafetyMetadata({ComposeSafetyConfig()}),
        .model_info_version = 20,
    });
    broker_.UpdateSafetyModel(safety_asset);
    task_environment_.RunUntilIdle();  // Wait for assets to be read from disk.
    EXPECT_TRUE(CreateSession(SessionConfigParams{}));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }

  // Safety model info is valid and requires language but no language detection
  // model, session not created successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset(FakeSafetyModelAsset::Content{
        .metadata = SafetyMetadata({[]() {
          auto safety_config = ComposeSafetyConfig();
          safety_config.add_allowed_languages("en");
          return safety_config;
        }()}),
        .model_info_version = 30,
    });
    broker_.UpdateSafetyModel(safety_asset);
    task_environment_.RunUntilIdle();  // Wait for assets to be read from disk.

    EXPECT_FALSE(CreateSession(SessionConfigParams{}));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kLanguageDetectionModelNotAvailable, 1);
  }

  // Safety model info is valid and requires language, all models available and
  // session created successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset(FakeSafetyModelAsset::Content{
        .metadata = SafetyMetadata({[]() {
          auto safety_config = ComposeSafetyConfig();
          safety_config.add_allowed_languages("en");
          return safety_config;
        }()}),
        .model_info_version = 40,
    });
    broker_.UpdateSafetyModel(safety_asset);
    broker_.UpdateLanguageDetectionModel(standard_assets_.language);
    task_environment_.RunUntilIdle();  // Wait for assets to be read from disk.

    EXPECT_TRUE(CreateSession(SessionConfigParams{}));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }

  // No safety model received yet but feature flag should disable safety check.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kTextSafetyClassifier);
    base::HistogramTester histogram_tester;

    broker_.model_provider().RemoveModel(
        proto::OPTIMIZATION_TARGET_TEXT_SAFETY);
    EXPECT_TRUE(CreateSession(SessionConfigParams{}));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, SucceedsWithPassingSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: safe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingRequestSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("unsafe_url"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: unsafe_url")
                          // Raw output check not done.
                          ));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithInvalidRequestSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", ProtoField({9999})));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingRawOutputSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({"unsafe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest, FailsWithInvalidRawOutputChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", ProtoField({9999})));
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());

  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(OnDeviceModelServiceControllerTest,
       SucceedsWithPassingResponseSafetyCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_very_"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_very_safe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingResponseSafetyCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_un"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithInvalidResponseSafetyCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_UNSPECIFIED);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_very_"),
                        response_.GetStreamingCallback());

  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(OnDeviceModelServiceControllerTest, NoRetractUnsafeContent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "false"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  // Should fail the configured checks, but not not be retracted.
  broker_.service_settings().set_execute_result({"unsafe_output"});
  session->ExecuteModel(PageUrlRequest("unsafe_url"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  // Make sure T&S logged.
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(
      response_.model_execution_info()
          ->on_device_model_execution_info()
          .execution_infos(),
      ElementsAre(
          testing::_,  // Base Model Execution
          ResultOf("check text", &GetCheckText, "request_check: unsafe_url"),
          ResultOf("check text", &GetCheckText,  // partial check
                   "raw_output_check: unsafe_output"),
          ResultOf("check text", &GetCheckText,  // complete check
                   "raw_output_check: unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest, ReturnsErrorOnServiceDisconnect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_fallback_to_server_on_disconnect", "false"}});

  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  task_environment_.RunUntilIdle();

  broker_.launcher().CrashService();
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kDisconnectAndCancel, 1);

  ASSERT_TRUE(response_.error());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnAddContext) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  task_environment_.RunUntilIdle();

  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  session->AddContext(UserInputRequest("bar"));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kCancelled, 1);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_.error());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnExecute) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  ResponseHolder resp1;
  ResponseHolder resp2;
  session->ExecuteModel(PageUrlRequest("foo"), resp1.GetStreamingCallback());
  session->ExecuteModel(PageUrlRequest("bar"), resp2.GetStreamingCallback());

  EXPECT_FALSE(resp1.GetFinalStatus());
  EXPECT_TRUE(resp2.GetFinalStatus());
  EXPECT_EQ(
      *resp1.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
  EXPECT_EQ(*resp2.value(), "execute:bar max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, WontStartSessionAfterGpuBlocked) {
  Initialize(standard_assets_);
  // Start a session.
  broker_.service_settings().service_disconnect_reason =
      on_device_model::ServiceDisconnectReason::kGpuBlocked;
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();

  {
    base::HistogramTester histogram_tester;

    // Because the model returned kGpuBlocked, no more sessions should start.
    EXPECT_FALSE(CreateSession(SessionConfigParams{}));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kGpuBlocked, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, DontRecreateSessionIfGpuBlocked) {
  Initialize(standard_assets_);
  broker_.service_settings().service_disconnect_reason =
      on_device_model::ServiceDisconnectReason::kGpuBlocked;
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();
  broker_.launcher().clear_did_launch_service();

  // Adding context should not trigger launching the service again.
  session->AddContext(UserInputRequest("baz"));
  EXPECT_FALSE(broker_.launcher().did_launch_service());
}

TEST_F(OnDeviceModelServiceControllerTest, StopsConnectingAfterMultipleDrops) {
  Initialize(standard_assets_);
  // Start a session.
  broker_.service_settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession(SessionConfigParams{})) << i;
    task_environment_.RunUntilIdle();
  }

  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);
}

TEST_F(OnDeviceModelServiceControllerTest, IdleTimeoutNotCountedAsCrash) {
  Initialize(standard_assets_);
  broker_.service_settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kIdleShutdown);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession(SessionConfigParams{})) << i;
    task_environment_.RunUntilIdle();
  }

  EXPECT_TRUE(CreateSession(SessionConfigParams{}));
}

TEST_F(OnDeviceModelServiceControllerTest, AllowsConnectingAfterBackoffPeriod) {
  Initialize(standard_assets_);
  broker_.service_settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);

  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession(SessionConfigParams{})) << i;
    task_environment_.RunUntilIdle();
  }

  // Immediately starting a session should fail.
  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Fast forward by backoff time and starting a session should succeed.
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  EXPECT_TRUE(CreateSession(SessionConfigParams{}));
  task_environment_.RunUntilIdle();

  // Starting another session after another crash should fail.
  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Fast forward base time should not work.
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Fast forward again should allow retrying (now 2 * base time).
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  EXPECT_TRUE(CreateSession(SessionConfigParams{}));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ClearsCrashDataOnSuccessAfterBackoff) {
  Initialize(standard_assets_);
  broker_.service_settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);

  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession(SessionConfigParams{})) << i;
    task_environment_.RunUntilIdle();
  }

  // Immediately starting a session should fail.
  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Fast forward by backoff time and starting a session should succeed.
  broker_.service_settings().set_drop_connection_request(std::nullopt);
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  EXPECT_TRUE(CreateSession(SessionConfigParams{}));
  task_environment_.RunUntilIdle();

  // Second session should succeed.
  EXPECT_TRUE(CreateSession(SessionConfigParams{}));

  // Single crash should not disable sessions.
  broker_.service_settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);
  EXPECT_TRUE(CreateSession(SessionConfigParams{}));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(CreateSession(SessionConfigParams{}));
}

TEST_F(OnDeviceModelServiceControllerTest, AlternatingDisconnectSucceeds) {
  Initialize(standard_assets_);
  // Start a session.
  for (int i = 0; i < 10; ++i) {
    broker_.service_settings().set_drop_connection_request(
        i % 2 == 1 ? std::make_optional(
                         on_device_model::ModelDisconnectReason::kUnspecified)
                   : std::nullopt);
    EXPECT_TRUE(CreateSession(SessionConfigParams{})) << i;
    task_environment_.RunUntilIdle();
  }
}

TEST_F(OnDeviceModelServiceControllerTest,
       MultipleDisconnectsThenVersionChangeRetries) {
  Initialize(standard_assets_);
  // Create enough sessions that fail to trigger no longer creating a session.
  broker_.service_settings().set_drop_connection_request(
      on_device_model::ModelDisconnectReason::kUnspecified);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession(SessionConfigParams{})) << i;
    task_environment_.RunUntilIdle();
  }
  EXPECT_FALSE(CreateSession(SessionConfigParams{}));
  EXPECT_EQ(
      broker_.GetOrCreateBrokerState().GetOnDeviceModelEligibility(kFeature),
      OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Change the pref to a different value and recreate the service.
  SimulateShutdown();
  broker_.local_state().SetString(
      model_execution::prefs::localstate::kOnDeviceModelChromeVersion,
      "BOGUS VERSION");
  Initialize(standard_assets_);
  // Wait until configuration is read.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(
      broker_.GetOrCreateBrokerState().GetOnDeviceModelEligibility(kFeature),
      OnDeviceModelEligibilityReason::kSuccess);

  // A new session should be started because the version changed.
  EXPECT_TRUE(CreateSession(SessionConfigParams{}));
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextDisconnectExecute) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();

  // Launch the service again, which triggers disconnect.
  broker_.launcher().CrashService();
  task_environment_.RunUntilIdle();

  // Send some text, ensuring the context is received.
  base::HistogramTester histogram_tester;
  session->ExecuteModel(PageUrlRequest("baz"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kUsedOnDevice, 1);
  std::string expected_response =
      ("ctx:foo max:8192"
       "execute:foobaz max:1024");
  EXPECT_EQ(*response_.value(), expected_response);
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextExecuteDisconnect) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();
  // Send the text, this won't make it because the service is immediately
  // killed.
  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());
  broker_.launcher().CrashService();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(response_.value());
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextMultipleSessions) {
  Initialize(standard_assets_);
  auto session1 = CreateSession(SessionConfigParams{});
  EXPECT_TRUE(session1);
  session1->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();

  // Start another session.
  auto session2 = CreateSession(SessionConfigParams{});
  EXPECT_TRUE(session2);
  session2->AddContext(UserInputRequest("bar"));
  task_environment_.RunUntilIdle();

  session2->ExecuteModel(PageUrlRequest("2"), response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  std::string expected_response1 =
      ("ctx:bar max:8192"
       "execute:bar2 max:1024");
  EXPECT_EQ(*response_.value(), expected_response1);

  ResponseHolder response2;
  session1->ExecuteModel(PageUrlRequest("1"), response2.GetStreamingCallback());
  ASSERT_TRUE(response2.GetFinalStatus());
  std::string expected_response2 =
      ("ctx:foo max:8192"
       "execute:foo1 max:1024");
  EXPECT_EQ(*response2.value(), expected_response2);
}

TEST_F(OnDeviceModelServiceControllerTest, FailsOnGpuBlockedService) {
  Initialize(standard_assets_);
  broker_.service_settings().service_disconnect_reason =
      on_device_model::ServiceDisconnectReason::kGpuBlocked;
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();
  broker_.launcher().clear_did_launch_service();

  // Adding context should not trigger launching the service again.
  {
    base::HistogramTester histogram_tester;
    session->AddContext(UserInputRequest("baz"));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kUsingServer, 1);
  }
  session->ExecuteModel(PageUrlRequest("2"), response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_FALSE(broker_.launcher().did_launch_service());
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextInvalidConfig) {
  FakeAdaptationAsset bad_compose_asset({.config = [] {
    proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    config.set_feature(ToModelExecutionFeatureProto(kFeature));
    return config;
  }()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&bad_compose_asset},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  {
    base::HistogramTester histogram_tester;
    session->AddContext(UserInputRequest("foo"));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kFailedConstructingInput, 1);
  }
  task_environment_.RunUntilIdle();
  {
    base::HistogramTester histogram_tester;
    session->ExecuteModel(PageUrlRequest("2"),
                          response_.GetStreamingCallback());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
        ExecuteModelResult::kOnDeviceNotUsed, 1);
  }
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(OnDeviceModelServiceControllerTest, ExecuteInvalidConfig) {
  FakeAdaptationAsset bad_compose_asset({.config = [] {
    proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_can_skip_text_safety(true);
    config.set_feature(ToModelExecutionFeatureProto(kFeature));
    return config;
  }()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&bad_compose_asset},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  base::HistogramTester histogram_tester;
  session->ExecuteModel(PageUrlRequest("2"), response_.GetStreamingCallback());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kFailedConstructingMessage, 1);
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailOnDisconnectWhileWaitingForExecute) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  task_environment_.RunUntilIdle();
  broker_.launcher().CrashService();
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kDisconnectAndCancel, 1);
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(OnDeviceModelServiceControllerTest,
       DestroySessionWhileWaitingForResponse) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  const auto total_time = base::Seconds(11);
  task_environment_.AdvanceClock(total_time);
  session.reset();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kDestroyedWhileWaitingForResponse, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceDestroyedWhileWaitingForResponseTime.Compose",
      total_time, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, DisconnectsWhenIdle) {
  const base::TimeDelta idle_timeout = base::Seconds(10);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_service_idle_timeout", "10s"}});
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  session.reset();

  task_environment_.FastForwardBy(idle_timeout / 2 + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  // Should still be connected after half the idle time.
  EXPECT_TRUE(broker_.launcher().is_service_running());

  // Fast forward by the amount of time that triggers a disconnect.
  task_environment_.FastForwardBy(idle_timeout / 2 + base::Milliseconds(1));
  // As there are no sessions and no traffic for GetOnDeviceModelIdleTimeout()
  // the connection should be dropped.
  EXPECT_FALSE(broker_.launcher().is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest,
       ShutsDownServiceAfterPerformanceCheck) {
  base::HistogramTester histogram_tester;
  broker_.local_state().SetString(
      model_execution::prefs::localstate::kOnDevicePerformanceClassVersion,
      "0.0.0.1");
  EXPECT_FALSE(broker_.GetOrCreateBrokerState()
                   .performance_classifier()
                   .IsPerformanceClassAvailable());
  broker_.launcher().clear_did_launch_service();
  base::RunLoop run_loop;
  broker_.GetOrCreateBrokerState()
      .performance_classifier()
      .EnsurePerformanceClassAvailable(run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass",
      OnDeviceModelPerformanceClass::kVeryHigh, 1);
  EXPECT_TRUE(broker_.launcher().did_launch_service());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !broker_.launcher().is_service_running(); }));
}

TEST_F(OnDeviceModelServiceControllerTest, RedactedField) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar");
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  // `foo` doesn't match the redaction, so should be returned.
  auto session1 = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session1);
  session1->ExecuteModel(UserInputRequest("foo"),
                         response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::string expected_response1 = "execute:foo max:1024";
  EXPECT_EQ(*response_.value(), expected_response1);
  EXPECT_THAT(response_.partials(), IsEmpty());

  // Input and output contain text matching redact, so should not be redacted.
  auto session2 = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session2);
  ResponseHolder response2;
  session2->ExecuteModel(UserInputRequest("abarx"),
                         response2.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::string expected_response2 = "execute:abarx max:1024";
  EXPECT_EQ(*response2.value(), expected_response2);
  EXPECT_THAT(response2.partials(), IsEmpty());

  // Output contains redacted text (and  input doesn't), so redact.
  broker_.service_settings().set_execute_result({"abarx max:1024"});
  auto session3 = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session3);
  ResponseHolder response3;
  session3->ExecuteModel(UserInputRequest("foo"),
                         response3.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::string expected_response3 = "a[###]x max:1024";
  EXPECT_EQ(*response3.value(), expected_response3);
  EXPECT_THAT(response3.partials(), IsEmpty());
}

TEST_F(OnDeviceModelServiceControllerTest, RejectedField) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar", proto::RedactBehavior::REJECT);
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });

  auto session1 = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session1);
  session1->ExecuteModel(UserInputRequest("bar"),
                         response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  // Although we send an error, we should be sending a log entry back so the
  // filtering can be logged.
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_EQ(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos(0)
                .response()
                .on_device_model_service_response()
                .status(),
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
}

TEST_F(OnDeviceModelServiceControllerTest, UsePreviousResponseForRewrite) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar");
  // Add a rule that identifies `previous_response` of `rewrite_params`.
  auto& output_config = *config.mutable_output_config();
  auto& redact_rules = *output_config.mutable_redact_rules();
  redact_rules.mutable_fields_to_check()->Add(PreviousResponseField());
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  // Force 'bar' to be returned from model.
  broker_.service_settings().set_execute_result({"bar max:1024"});

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->ExecuteModel(RewriteRequest("bar"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  // `bar` shouldn't be rewritten as it's in the input.
  const std::string expected_response = "bar max:1024";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_THAT(response_.partials(), IsEmpty());
}

TEST_F(OnDeviceModelServiceControllerTest, ReplacementText) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar", proto::REDACT_IF_ONLY_IN_OUTPUT, "[redacted]");
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  // Output contains redacted text (and  input doesn't), so redact.
  broker_.service_settings().set_execute_result({"abarx max:1024"});
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::string expected_response = "a[redacted]x max:1024";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_THAT(response_.partials(), IsEmpty());
}

TEST_F(OnDeviceModelServiceControllerTest, DetectsRepeats) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "false"}});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  broker_.service_settings().set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = {
      "some text",
      " some more repeating text",
  };
  EXPECT_EQ(*response_.value(),
            ConcatResponses(expected_responses) + " some more repeating text");
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      true, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, DetectsRepeatsAndCancelsResponse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "true"}});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  broker_.service_settings().set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(*response_.error(), OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kResponseLowQuality);

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kResponseHadRepeats, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, DetectsRepeatsAcrossResponses) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "false"}});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  broker_.service_settings().set_execute_result({
      "some text",
      " some more repeating",
      " text",
      " some more ",
      "repeating text",
      " more stuff",
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> partial_responses = {
      "some text",
      " some more repeating",
      " text",
      " some more ",
  };
  EXPECT_EQ(*response_.value(),
            ConcatResponses(partial_responses) + "repeating text");
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      true, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, IgnoresNonRepeatingText) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "false"}});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  broker_.service_settings().set_execute_result({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = {
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  };
  EXPECT_EQ(*response_.value(), ConcatResponses(expected_responses));
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_FALSE(response_.model_execution_info()
                   ->on_device_model_execution_info()
                   .execution_infos(0)
                   .response()
                   .on_device_model_service_response()
                   .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      false, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       WithholdsTrailingNewlinesAcrossResponses) {
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  broker_.service_settings().set_execute_result({
      "some text",
      " texts with newlines\n\n",
      "\n",
      "\n\n",
      "\n",
      "\n",
      "\n no trailing newline",
      "\n more trailing newlines\n\n",
      "\n\n",
      "\n",
      "",
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  const std::vector<std::string> partial_responses = {
      "some text",
      " texts with newlines",
      "\n\n\n\n\n\n\n\n no trailing newline",
      "\n more trailing newlines",
  };
  EXPECT_EQ(*response_.value(), ConcatResponses(partial_responses));
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       WithholdsTrailingNewlinesNoTrailingNewlines) {
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  broker_.service_settings().set_execute_result({
      "some text",
      " texts with newlines\n",
      "\n",
      "\n\n",
      "\n",
      "\n",
      "\n no trailing newline",
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  const std::vector<std::string> partial_responses = {
      "some text",
      " texts with newlines",
      "\n\n\n\n\n\n\n no trailing newline",
  };
  EXPECT_EQ(*response_.value(), ConcatResponses(partial_responses));
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, NoWithholdsTrailingNewlines) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_withhold_newlines", "false"}});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .adaptations = {&compose_asset},
  });

  broker_.service_settings().set_execute_result({
      "some text",
      " texts with newlines\n\n",
      "\n",
      "\n\n",
      "\n",
      "\n",
      "\n no trailing newline",
      "\n more trailing newlines\n\n",
      "\n\n",
      "\n",
      "",
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  const std::vector<std::string> partial_responses = {
      "some text",
      " texts with newlines\n\n",
      "\n",
      "\n\n",
      "\n",
      "\n",
      "\n no trailing newline",
      "\n more trailing newlines\n\n",
      "\n\n",
      "\n",
      "",
  };
  EXPECT_EQ(*response_.value(), ConcatResponses(partial_responses));
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, UsesSessionTopKAndTemperature) {
  // Session sampling params should have precedence over feature ones.
  auto config = SimpleComposeConfig();
  config.mutable_sampling_params()->set_top_k(4);
  config.mutable_sampling_params()->set_temperature(1.5);
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });

  const SamplingParams expected_sampling_params{
      .top_k = 3,
      .temperature = 2,
  };

  auto session = CreateSession(
      SessionConfigParams{.sampling_params = expected_sampling_params});
  ASSERT_TRUE(session);

  const auto session_sampling_params = session->GetSamplingParams();
  EXPECT_EQ(session_sampling_params.top_k, expected_sampling_params.top_k);
  EXPECT_EQ(session_sampling_params.temperature,
            expected_sampling_params.temperature);

  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  const std::vector<std::string> partial_responses = {
      "execute:foo max:1024",
      "TopK: 3, Temp: 2",
  };
  EXPECT_EQ(*response_.value(), ConcatResponses(partial_responses));
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));
}

// Validate that a missing partial output config suppresses partial output.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval0) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"token1", " token2", " token3",
                                           " token4"};
  broker_.service_settings().set_execute_result(tokens);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(*response_.value(), ConcatResponses(tokens));
  EXPECT_THAT(response_.partials(), ElementsAreArray(tokens));
}

// Validate that token interval 1 evaluates all partial output.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval1) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(1);
    return safety_config;
  }());
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"token1", " token2", " token3",
                                           " token4"};
  broker_.service_settings().set_execute_result(tokens);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(*response_.value(), ConcatResponses(tokens));
  EXPECT_THAT(response_.partials(), ElementsAreArray(tokens));
}

// Validate that token interval 3 only evaluates every third and final chunk.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval3) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(3);
    return safety_config;
  }());
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"token1",  " token2", " token3",
                                           " token4", " token5", " token6",
                                           " token7"};
  broker_.service_settings().set_execute_result(tokens);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(*response_.value(), ConcatResponses(tokens));
  EXPECT_THAT(response_.partials(), ElementsAreArray({
                                        "token1 token2 token3",
                                        " token4 token5 token6",
                                    }));
}

// Validate that PartialOutputChecks::minimum_tokens is respected.
TEST_F(OnDeviceModelServiceControllerTest, MinimumSafetyTokens) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_minimum_tokens(2);
    safety_config.mutable_partial_output_checks()->set_token_interval(1);
    return safety_config;
  }());
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"token1", " token2", " token3",
                                           " token4"};
  broker_.service_settings().set_execute_result(tokens);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1 token2",
      " token3",
      " token4",
  };
  EXPECT_EQ(*response_.value(), ConcatResponses(tokens));
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, WaitUntilCompleteToCancel) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.set_only_cancel_unsafe_response_on_complete(true);
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.add_allowed_languages("en");
    return safety_config;
  }());
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"safe", " safe", " lang:en=1.0",
                                           " safe", " unsafe"};
  broker_.service_settings().set_execute_result(tokens);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());

  // The full output was unsafe so it resulted it in it being filtered.
  EXPECT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);

  const std::vector<std::string> expected_responses = {
      // The first two responses are filtered because their language hasn't been
      // detected yet. Because `only_cancel_unsafe_response_on_complete` is
      // true, this doesn't cause the input to be cancelled.
      //
      // "safe", "safe safe",

      // The next two responses are not filtered because the language has been
      // reliably detected as a supported language.
      "safe safe lang:en=1.0", " safe",
      // The last response is unsafe so it is filtered. Since the output is
      // complete the response is cancelled.
      //
      // "safe safe lang:en=1.0 safe unsafe",
  };
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, TestAvailabilityObserver) {
  FakeAdaptationAsset test_asset_noweight({.config = UnsafeTestConfig()});
  Initialize({
      .base_model_content = std::nullopt,
      .adaptations = {&test_asset_noweight},
  });

  FakeOnDeviceModelAvailabilityObserver availability_observer_compose(
      mojom::OnDeviceFeature::kCompose),
      availability_observer_test(mojom::OnDeviceFeature::kTest);
  broker_.GetOrCreateBrokerState().AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature::kCompose, &availability_observer_compose);
  broker_.GetOrCreateBrokerState().AddOnDeviceModelAvailabilityChangeObserver(
      mojom::OnDeviceFeature::kTest, &availability_observer_test);

  broker_.InstallBaseModel(std::make_unique<FakeBaseModelAsset>());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_test.reason_);

  FakeAdaptationAsset adaptation_asset({
      .config = UnsafeComposeConfig(),
      .weight = 1015,
  });
  broker_.UpdateModelAdaptation(adaptation_asset);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_test.reason_);
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_compose.reason_);
}

class OnDeviceModelServiceControllerTsIntervalTest
    : public OnDeviceModelServiceControllerTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(OnDeviceModelServiceControllerTsIntervalTest,
       DetectsRepeatsWithSafetyModel) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOptimizationGuideOnDeviceModel,
        {{"on_device_model_retract_repeats", "false"}}},
       {features::kTextSafetyClassifier,
        {{"on_device_retract_unsafe_content", "true"}}}},
      {});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(
        GetParam());
    return safety_config;
  }());
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.service_settings().set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " unsafe stuff not processed",
  });
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_.value());
  EXPECT_EQ(*response_.value(),
            "some text some more repeating text some more repeating text");

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      true, 1);
}

INSTANTIATE_TEST_SUITE_P(OnDeviceModelServiceControllerTsIntervalTests,
                         OnDeviceModelServiceControllerTsIntervalTest,
                         testing::ValuesIn<int>({1, 2, 3, 4, 10}));

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationSucceeds) {
  base::HistogramTester histogram_tester;
  broker_.InstallBaseModel(FakeBaseModelAsset::Content{
      .config = ExecutionConfigWithValidation(WillPassValidationConfig())});
  Initialize({});
  task_environment_.RunUntilIdle();
  // Service should be immediately shut down.
  EXPECT_FALSE(broker_.launcher().is_service_running());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationSucceedsImmediatelyWithNoPrompts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "30s"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{
              .config = ExecutionConfigWithValidation(
                  proto::OnDeviceModelValidationConfig{})},
      .adaptations = {&compose_asset},
  });
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(CreateSession(SessionConfigParams{}));

  // Full validation did not need to run.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationBlocksSession) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{.config = ExecutionConfigWithValidation(
                                          WillFailValidationConfig())},
      .adaptations = {&compose_asset},
  });

  EXPECT_EQ(broker_.GetOrCreateBrokerState().GetOnDeviceModelEligibility(
                mojom::OnDeviceFeature::kCompose),
            OnDeviceModelEligibilityReason::kValidationFailed);

  broker_.InstallBaseModel(FakeBaseModelAsset::Content{
      .config = ExecutionConfigWithValidation(WillPassValidationConfig())});
  task_environment_.FastForwardBy(base::Seconds(30) + base::Milliseconds(1));
  EXPECT_EQ(broker_.GetOrCreateBrokerState().GetOnDeviceModelEligibility(
                mojom::OnDeviceFeature::kCompose),
            OnDeviceModelEligibilityReason::kSuccess);
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationBlocksSessionPendingCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "30s"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{.config = ExecutionConfigWithValidation(
                                          WillPassValidationConfig())},
      .adaptations = {&compose_asset},
  });
  EXPECT_EQ(broker_.GetOrCreateBrokerState().GetOnDeviceModelEligibility(
                mojom::OnDeviceFeature::kCompose),
            OnDeviceModelEligibilityReason::kValidationPending);
  task_environment_.FastForwardBy(base::Seconds(30) + base::Milliseconds(1));
  EXPECT_EQ(broker_.GetOrCreateBrokerState().GetOnDeviceModelEligibility(
                mojom::OnDeviceFeature::kCompose),
            OnDeviceModelEligibilityReason::kSuccess);
}

// TODO(crbug.com/380229867): Flaky on Mac and Android.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_ModelValidationNewModelVersion \
  DISABLED_ModelValidationNewModelVersion
#else
#define MAYBE_ModelValidationNewModelVersion ModelValidationNewModelVersion
#endif
TEST_F(OnDeviceModelServiceControllerTest,
       MAYBE_ModelValidationNewModelVersion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});
  broker_.InstallBaseModel(FakeBaseModelAsset::Content{
      .config = ExecutionConfigWithValidation(WillPassValidationConfig())});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  broker_.UpdateModelAdaptation(compose_asset);
  {
    base::HistogramTester histogram_tester;
    ASSERT_TRUE(base::test::RunUntil([&] {
      OnDeviceModelEligibilityReason reason =
          broker_.GetOrCreateBrokerState().GetOnDeviceModelEligibility(
              mojom::OnDeviceFeature::kCompose);
      return reason == OnDeviceModelEligibilityReason::kSuccess;
    }));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  EXPECT_TRUE(CreateSession(SessionConfigParams{}));

  auto next_model =
      std::make_unique<FakeBaseModelAsset>(FakeBaseModelAsset::Content{
          .weight = 2,
          .config = ExecutionConfigWithValidation(WillFailValidationConfig()),
      });
  next_model->set_version("0.0.2");
  {
    base::HistogramTester histogram_tester;
    broker_.InstallBaseModel(std::move(next_model));
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  EXPECT_FALSE(CreateSession(SessionConfigParams{}));
}

TEST_F(OnDeviceModelServiceControllerTest, GetCapabilities) {
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{
              .config = ExecutionConfigWithCapabilities(
                  {proto::OnDeviceModelCapability::
                       ON_DEVICE_MODEL_CAPABILITY_IMAGE_INPUT}),
          },
  });
  task_environment_.RunUntilIdle();

  EXPECT_EQ(broker_.GetOrCreateBrokerState().GetOnDeviceCapabilities(),
            on_device_model::Capabilities(
                {on_device_model::CapabilityFlags::kImageInput}));

  broker_.InstallBaseModel({
      .config = ExecutionConfigWithCapabilities(
          {proto::OnDeviceModelCapability::
               ON_DEVICE_MODEL_CAPABILITY_AUDIO_INPUT}),
  });
  task_environment_.RunUntilIdle();

  EXPECT_EQ(broker_.GetOrCreateBrokerState().GetOnDeviceCapabilities(),
            on_device_model::Capabilities(
                {on_device_model::CapabilityFlags::kAudioInput}));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationNewModelVersionCancelsPreviousValidation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "10s"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});

  base::HistogramTester histogram_tester;
  broker_.launcher().clear_did_launch_service();

  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize(
      {.base_model_content =
           FakeBaseModelAsset::Content{.config = ExecutionConfigWithValidation(
                                           WillPassValidationConfig())},
       .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  // Send a new model update with no validation config.

  auto next_model =
      std::make_unique<FakeBaseModelAsset>(FakeBaseModelAsset::Content{
          .weight = 2,
      });
  next_model->set_version("0.0.2");
  broker_.InstallBaseModel(std::move(next_model));
  task_environment_.RunUntilIdle();

  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));

  // Full validation should never run.
  EXPECT_FALSE(broker_.launcher().did_launch_service());
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelValidationResultOnValidationStarted",
      OnDeviceModelValidationResult::kUnknown, 2);

  EXPECT_TRUE(CreateSession(SessionConfigParams{}));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDoesNotRepeat) {
  broker_.InstallBaseModel(FakeBaseModelAsset::Content{
      .config = ExecutionConfigWithValidation(WillPassValidationConfig())});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  broker_.UpdateModelAdaptation(compose_asset);
  {
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
    SimulateShutdown();
  }

  {
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationRepeatsOnFailure) {
  broker_.InstallBaseModel(FakeBaseModelAsset::Content{
      .config = ExecutionConfigWithValidation([] {
        proto::OnDeviceModelValidationConfig validation_config;
        auto* prompt = validation_config.add_validation_prompts();
        prompt->set_prompt("hello");
        prompt->set_expected_output("goodbye");
        return validation_config;
      }())});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  broker_.UpdateModelAdaptation(compose_asset);

  {
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
    SimulateShutdown();
  }

  {
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
    SimulateShutdown();
  }

  {
    broker_.service_settings().set_execute_result({"goodbye"});
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationMaximumRetry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"},
         {"on_device_model_validation_attempt_count", "2"}}}},
      {});
  broker_.InstallBaseModel(FakeBaseModelAsset::Content{
      .config = ExecutionConfigWithValidation([] {
        proto::OnDeviceModelValidationConfig validation_config;
        auto* prompt = validation_config.add_validation_prompts();
        prompt->set_prompt("hello");
        prompt->set_expected_output("goodbye");
        return validation_config;
      }())});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  broker_.UpdateModelAdaptation(compose_asset);

  {
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
    SimulateShutdown();
  }

  {
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
    SimulateShutdown();
  }

  // Limit reached, does not retry.
  {
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
    SimulateShutdown();
  }

  // After a new version, we should re-check.
  broker_.local_state().SetString(
      model_execution::prefs::localstate::kOnDeviceModelChromeVersion,
      "OLD_VERSION");
  {
    base::HistogramTester histogram_tester;
    Initialize({});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kOnDeviceModelValidation);
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  base::HistogramTester histogram_tester;
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{
              .config =
                  ExecutionConfigWithValidation(WillPassValidationConfig()),
          },
      .adaptations = {&compose_asset},
  });
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDelayed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "30s"}}}},
      {});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  base::HistogramTester histogram_tester;
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{
              .config =
                  ExecutionConfigWithValidation(WillPassValidationConfig()),
          },
      .adaptations = {&compose_asset},
  });
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  task_environment_.FastForwardBy(base::Seconds(15) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  task_environment_.FastForwardBy(base::Seconds(15) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       SessionDoesNotInterruptModelValidation) {
  broker_.service_settings().set_execute_delay(base::Seconds(10));

  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  base::HistogramTester histogram_tester;
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{
              .config =
                  ExecutionConfigWithValidation(WillPassValidationConfig()),
          },
      .adaptations = {&compose_asset},
  });
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  EXPECT_TRUE(response_.GetFinalStatus());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);

  // Session was created so the service should still be connected.
  EXPECT_TRUE(broker_.launcher().is_service_running());

  // If we destroy all sessions and wait long enough, everything should idle out
  // and the service should get terminated.
  session.reset();
  task_environment_.FastForwardBy(2 * features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(broker_.launcher().is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationFails) {
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  base::HistogramTester histogram_tester;
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{
              .config =
                  ExecutionConfigWithValidation(WillFailValidationConfig()),
          },
      .adaptations = {&compose_asset},
  });
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kNonMatchingOutput, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationFailsOnCrash) {
  broker_.service_settings().set_execute_delay(base::Seconds(10));
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  base::HistogramTester histogram_tester;
  Initialize({
      .base_model_content =
          FakeBaseModelAsset::Content{
              .config =
                  ExecutionConfigWithValidation(WillPassValidationConfig()),
          },
      .adaptations = {&compose_asset},
  });
  task_environment_.RunUntilIdle();

  broker_.launcher().CrashService();
  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kServiceCrash, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, SendsPerformanceHint) {
  broker_.InstallBaseModel(std::make_unique<FakeBaseModelAsset>(
      std::vector<proto::OnDeviceModelPerformanceHint>{
          proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE}));
  Initialize(InitializeParams{
      .base_model_content = std::nullopt,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession(SessionConfigParams{});
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "Fastest inferenceexecute:foo max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, UsesCpuModel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      on_device_model::features::kOnDeviceModelCpuBackend,
      {{"on_device_cpu_ram_threshold_mb", "0"},
       {"on_device_cpu_processor_count_threshold", "0"},
       {"on_device_cpu_require_64_bit_processor", "false"}});
  broker_.InstallBaseModel(std::make_unique<FakeBaseModelAsset>(
      std::vector<proto::OnDeviceModelPerformanceHint>{
          proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU}));
  Initialize(InitializeParams{
      .base_model_content = std::nullopt,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession(SessionConfigParams{});
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "CPU backendexecute:foo max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, ImageExecutionSuccess) {
  using RequestProto = ::optimization_guide::proto::ExampleForTestingRequest;
  using NestedProto = ::optimization_guide::proto::ExampleForTestingMessage;
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_feature(
      ToModelExecutionFeatureProto(mojom::OnDeviceFeature::kCompose));
  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name(
      proto::ExampleForTestingRequest().GetTypeName());
  {
    auto& substitution = *input_config.add_input_context_substitutions();
    substitution.set_string_template("%s");
    *substitution.add_substitutions()
         ->add_candidates()
         ->mutable_media_field()
         ->mutable_proto_field() = ProtoField(
        {RequestProto::kNested1FieldNumber, NestedProto::kMediaFieldNumber});
  }
  {
    auto& substitution = *input_config.add_execute_substitutions();
    substitution.set_string_template("%s");
    *substitution.add_substitutions()
         ->add_candidates()
         ->mutable_media_field()
         ->mutable_proto_field() = ProtoField(
        {RequestProto::kNested2FieldNumber, NestedProto::kMediaFieldNumber});
  }
  *config.mutable_output_config() = ResponseHolderOutputConfig();
  FakeAdaptationAsset compose_asset({
      .config = config,
  });
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });
  MultimodalMessage request((proto::ExampleForTestingRequest()));
  request.edit()
      .GetMutableMessage(RequestProto::kNested1FieldNumber)
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));
  request.edit()
      .GetMutableMessage(RequestProto::kNested2FieldNumber)
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));
  {
    ResponseHolder response;
    auto session = CreateSession(SessionConfigParams{
        .capabilities = {on_device_model::CapabilityFlags::kImageInput},
    });
    ASSERT_TRUE(session);
    session->SetInput(request.Clone(), {});
    session->ExecuteModel(proto::ExampleForTestingRequest(),
                          response.GetStreamingCallback());
    ASSERT_TRUE(response.GetFinalStatus());
    EXPECT_EQ(*response.value(), "<image> max:8192<image> max:1024");
  }

  // Session without capabilities should not allow images.
  {
    ResponseHolder response;
    auto session = CreateSession(SessionConfigParams{});
    ASSERT_TRUE(session);
    session->SetInput(std::move(request), {});
    session->ExecuteModel(proto::ExampleForTestingRequest(),
                          response.GetStreamingCallback());
    ASSERT_TRUE(response.GetFinalStatus());
    EXPECT_EQ(*response.value(),
              "<unsupported> max:8192<unsupported> "
              "max:1024");
  }
}

proto::SubstitutedString EmptySubstitution() {
  proto::SubstitutedString result;
  result.set_string_template("%s");
  result.add_substitutions()->add_candidates()->set_raw_string("");
  return result;
}

TEST_F(OnDeviceModelServiceControllerTest, KeepInputOnExtension) {
  using Request = proto::ExampleForTestingRequest;
  auto kRepeatedTag = Request::kRepeatedFieldFieldNumber;
  using Msg = proto::ExampleForTestingMessage;
  // A simple config that includes content from the
  // proto::ExampleForTestingRequest::repeated_field
  FakeAdaptationAsset compose_asset({
      .config =
          []() {
            proto::OnDeviceModelExecutionFeatureConfig config;
            config.set_feature(
                ToModelExecutionFeatureProto(mojom::OnDeviceFeature::kCompose));
            *config.mutable_input_config() = TestInputConfig(
                ForEachRepeated(FormatTestMessage()), EmptySubstitution());
            *config.mutable_output_config() = ResponseHolderOutputConfig();
            return config;
          }(),
  });
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });
  base::test::TestFuture<
      base::expected<size_t, OptimizationGuideModelExecutionError>>
      set_input_future;

  auto session = CreateSession(SessionConfigParams{
      .capabilities = {on_device_model::CapabilityFlags::kImageInput,
                       on_device_model::CapabilityFlags::kAudioInput},
  });
  ASSERT_TRUE(session);
  MultimodalMessage request((Request()));
  request.edit()
      .MutableRepeatedField(kRepeatedTag)
      .Add()
      .Set(Msg::kStringValueFieldNumber, "v1");
  request.edit()
      .MutableRepeatedField(kRepeatedTag)
      .Add()
      .Set(Msg::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));
  request.edit()
      .MutableRepeatedField(kRepeatedTag)
      .Add()
      .Set(Msg::kMediaFieldNumber, CreateDummyAudioBuffer());
  session->SetInput(request.Clone(), {});
  request.edit()
      .MutableRepeatedField(kRepeatedTag)
      .Add()
      .Set(Msg::kStringValueFieldNumber, "v2");
  session->SetInput(request.Clone(), set_input_future.GetCallback());
  // Waiting for outstanding calls should let max_tokens be updated.
  EXPECT_EQ(*set_input_future.Take(), 18ul);
  request.edit()
      .MutableRepeatedField(kRepeatedTag)
      .Add()
      .Set(Msg::kStringValueFieldNumber, "v3");
  session->SetInput(request.Clone(), {});

  // Make a clone that extends from the original input.
  auto extended_clone = session->Clone();
  request.edit()
      .MutableRepeatedField(kRepeatedTag)
      .Add()
      .Set(Msg::kStringValueFieldNumber, "v4");
  extended_clone->SetInput(request.Clone(), {});

  // Make a clone that also alters the original request.
  auto altered_clone = session->Clone();
  request.edit()
      .MutableRepeatedField(kRepeatedTag)
      .Get(1)
      .Set(Msg::kMediaFieldNumber, CreateBlackSkBitmap(2, 2));
  altered_clone->SetInput(request.Clone(), {});

  // The altered clone should have reset + resent all input in one chunk.
  ResponseHolder altered_response;
  altered_clone->ExecuteModel(proto::ExampleForTestingRequest(),
                              altered_response.GetStreamingCallback());
  ASSERT_TRUE(altered_response.GetFinalStatus());
  EXPECT_EQ(*altered_response.value(), "v1<image><audio>v2v3v4 max:8192");

  // The clone that only extended should have sent input in separate chunks.
  ResponseHolder extended_response;
  extended_clone->ExecuteModel(proto::ExampleForTestingRequest(),
                               extended_response.GetStreamingCallback());
  ASSERT_TRUE(extended_response.GetFinalStatus());
  EXPECT_EQ(*extended_response.value(),
            "v1<image><audio> max:8192"
            "v2 max:8192"
            "v3 max:8174"
            "v4 max:8174");

  // The original should have input in separate chunks.
  session->ExecuteModel(proto::ExampleForTestingRequest(),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(),
            "v1<image><audio> max:8192"
            "v2 max:8192"
            "v3 max:8174");
}

TEST_F(OnDeviceModelServiceControllerTest, OmitEmptyInputs) {
  // Avoid calling Append with empty inputs.
  FakeAdaptationAsset compose_asset({
      .config =
          []() {
            proto::OnDeviceModelExecutionFeatureConfig config;
            config.set_feature(
                ToModelExecutionFeatureProto(mojom::OnDeviceFeature::kCompose));
            auto& input_config = *config.mutable_input_config();
            input_config.set_request_base_name(
                proto::ExampleForTestingRequest().GetTypeName());
            *input_config.add_input_context_substitutions() =
                EmptySubstitution();
            *input_config.add_execute_substitutions() = EmptySubstitution();
            *config.mutable_output_config() = ResponseHolderOutputConfig();
            return config;
          }(),
  });
  Initialize(InitializeParams{
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  MultimodalMessage request((proto::ExampleForTestingRequest()));
  session->SetInput(std::move(request), {});
  session->ExecuteModel(proto::ExampleForTestingRequest(),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  // No "Context:" chunks should appear in the output.
  EXPECT_EQ(*response_.value(), "");
}

TEST_F(OnDeviceModelServiceControllerTest, CloneUsesSessionTopKAndTemperature) {
  auto config = SimpleComposeConfig();
  config.mutable_sampling_params()->set_top_k(4);
  config.mutable_sampling_params()->set_temperature(1.5);
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });

  const SamplingParams expected_sampling_params{
      .top_k = 3,
      .temperature = 2,
  };

  auto session = CreateSession(
      SessionConfigParams{.sampling_params = expected_sampling_params});
  ASSERT_TRUE(session);
  auto clone = session->Clone();
  EXPECT_TRUE(clone);

  const auto clone_sampling_params = clone->GetSamplingParams();
  EXPECT_EQ(clone_sampling_params.top_k, expected_sampling_params.top_k);
  EXPECT_EQ(clone_sampling_params.temperature,
            expected_sampling_params.temperature);

  clone->ExecuteModel(PageUrlRequest("foo"), response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  const std::vector<std::string> partial_responses = {
      "execute:foo max:1024",
      "TopK: 3, Temp: 2",
  };
  EXPECT_EQ(*response_.value(), ConcatResponses(partial_responses));
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       CloneFailsWithFailingRequestSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  auto clone = session->Clone();
  EXPECT_TRUE(clone);

  broker_.service_settings().set_execute_result({"safe_output"});
  clone->ExecuteModel(PageUrlRequest("unsafe_url"),
                      response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: unsafe_url")
                          // Raw output check not done.
                          ));
}

TEST_F(OnDeviceModelServiceControllerTest, ScoreAfterClone) {
  Initialize(standard_assets_);

  base::HistogramTester histogram_tester;
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));

  auto clone = session->Clone();
  base::test::TestFuture<std::optional<float>> score_future;
  clone->Score("token", score_future.GetCallback());
  EXPECT_EQ(score_future.Get(), 0.5);
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextAndClone) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  auto clone = session->Clone();

  // Cloned session should be able to execute.
  {
    ResponseHolder response;
    clone->ExecuteModel(PageUrlRequest("bar"), response.GetStreamingCallback());
    ASSERT_TRUE(response.GetFinalStatus());
    std::string expected_response =
        ("ctx:foo max:8192"
         "execute:foobar max:1024");
    EXPECT_EQ(*response.value(), expected_response);
  }

  // Original session should also be able to execute.
  {
    ResponseHolder response;
    session->ExecuteModel(PageUrlRequest("blah"),
                          response.GetStreamingCallback());
    ASSERT_TRUE(response.GetFinalStatus());
    std::string expected_response =
        ("ctx:foo max:8192"
         "execute:fooblah max:1024");
    EXPECT_EQ(*response.value(), expected_response);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, CloneBeforeAddContext) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  // Clone happens before context is added to the parent session.
  auto clone = session->Clone();
  session->AddContext(UserInputRequest("foo"));

  // Cloned session should execute without context.
  {
    ResponseHolder response;
    clone->ExecuteModel(PageUrlRequest("bar"), response.GetStreamingCallback());
    ASSERT_TRUE(response.GetFinalStatus());
    EXPECT_EQ(*response.value(), "execute:bar max:1024");
  }

  // Original session should execute with context
  {
    ResponseHolder response;
    session->ExecuteModel(PageUrlRequest("blah"),
                          response.GetStreamingCallback());
    ASSERT_TRUE(response.GetFinalStatus());
    std::string expected_response =
        ("ctx:foo max:8192"
         "execute:fooblah max:1024");
    EXPECT_EQ(*response.value(), expected_response);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, CancelAddContextAndClone) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  auto clone = session->Clone();
  // Deleting the parent session cancels the context chunk.
  // TODO: crbug.com/396211270 - Make clone independent of parent.
  session = nullptr;

  ResponseHolder response;
  clone->ExecuteModel(PageUrlRequest("bar"), response.GetStreamingCallback());
  ASSERT_TRUE(response.GetFinalStatus());
  EXPECT_EQ(*response.value(), "execute:foobar max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, CloneAddContextDisconnectExecute) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  auto clone = session->Clone();
  task_environment_.RunUntilIdle();

  // Launch the service again, which triggers disconnect.
  broker_.launcher().CrashService();
  task_environment_.RunUntilIdle();

  ResponseHolder response;
  clone->ExecuteModel(PageUrlRequest("bar"), response.GetStreamingCallback());
  ASSERT_TRUE(response.GetFinalStatus());
  std::string expected_response =
      ("ctx:foo max:8192"
       "execute:foobar max:1024");
  EXPECT_EQ(*response.value(), expected_response);
}

TEST_F(OnDeviceModelServiceControllerTest, Broker) {
  mojo::PendingReceiver<mojom::ModelBroker> pending_broker;

  OptimizationGuideLogger logger;

  ModelBrokerClient broker_client(pending_broker.InitWithNewPipeAndPassRemote(),
                                  logger.GetWeakPtr());

  base::test::TestFuture<std::unique_ptr<OnDeviceSession>> session_future;
  broker_client.CreateSession(mojom::OnDeviceFeature::kCompose,
                              SessionConfigParams{},
                              session_future.GetCallback());

  Initialize(standard_assets_);
  broker_.GetOrCreateBrokerState().BindModelBroker(std::move(pending_broker));

  auto session = session_future.Take();
  ASSERT_TRUE(session);

  ResponseHolder response;
  session->ExecuteModel(PageUrlRequest("bar"), response.GetStreamingCallback());
  ASSERT_TRUE(response.GetFinalStatus());
  EXPECT_EQ(*response.value(), "execute:bar max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest,
       BrokerCreateSessionRunsPerformanceClassCheck) {
  base::HistogramTester histogram_tester;
  mojo::PendingReceiver<mojom::ModelBroker> pending_broker;

  broker_.local_state().SetString(
      model_execution::prefs::localstate::kOnDevicePerformanceClassVersion,
      "0.0.0.1");

  OptimizationGuideLogger logger;

  ModelBrokerClient broker_client(pending_broker.InitWithNewPipeAndPassRemote(),
                                  logger.GetWeakPtr());
  base::test::TestFuture<std::unique_ptr<OnDeviceSession>> session_future;
  broker_client.CreateSession(mojom::OnDeviceFeature::kCompose,
                              SessionConfigParams{},
                              session_future.GetCallback());
  broker_.GetOrCreateBrokerState().BindModelBroker(std::move(pending_broker));
  broker_.component_state().WaitForRegistration();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass",
      OnDeviceModelPerformanceClass::kVeryHigh, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, Priority) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  EXPECT_TRUE(session);

  EXPECT_EQ(GetResponse(*session, "foo"), "execute:foo max:1024");

  session->SetPriority(on_device_model::mojom::Priority::kBackground);
  EXPECT_EQ(GetResponse(*session, "foo"),
            "Priority: backgroundexecute:foo max:1024");
  EXPECT_EQ(GetResponse(*session, "foo"),
            "Priority: backgroundexecute:foo max:1024");

  session->SetPriority(on_device_model::mojom::Priority::kForeground);
  EXPECT_EQ(GetResponse(*session, "foo"), "execute:foo max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, PriorityClone) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  EXPECT_TRUE(session);

  EXPECT_EQ(GetResponse(*session, "foo"), "execute:foo max:1024");

  session->SetPriority(on_device_model::mojom::Priority::kBackground);
  EXPECT_EQ(GetResponse(*session, "foo"),
            "Priority: backgroundexecute:foo max:1024");

  auto clone = session->Clone();
  EXPECT_EQ(GetResponse(*clone, "foo"),
            "Priority: backgroundexecute:foo max:1024");
  EXPECT_EQ(GetResponse(*clone, "foo"),
            "Priority: backgroundexecute:foo max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, SetInputCallback) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  MultimodalMessage request((UserInputRequest("foo")));
  base::test::TestFuture<
      base::expected<size_t, OptimizationGuideModelExecutionError>>
      future;
  session->SetInput(std::move(request), future.GetCallback());
  EXPECT_EQ(*future.Get(), std::string("ctx:foo").size());

  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "ctx:foo max:8192execute:foobar "
            "max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, SetInputCallbackCancelled) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  MultimodalMessage request((UserInputRequest("foo")));
  base::test::TestFuture<
      base::expected<size_t, OptimizationGuideModelExecutionError>>
      future1;
  base::test::TestFuture<
      base::expected<size_t, OptimizationGuideModelExecutionError>>
      future2;
  session->SetInput(request.Clone(), future1.GetCallback());
  session->SetInput(std::move(request), future2.GetCallback());

  // First request is cancelled, second request completes.
  EXPECT_EQ(
      future1.Get().error().error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
  EXPECT_EQ(*future2.Get(), std::string("ctx:foo").size());

  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "ctx:foo max:8192execute:foobar "
            "max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, SetInputCallbackError) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  MultimodalMessage request((proto::ExampleForTestingRequest()));
  base::test::TestFuture<
      base::expected<size_t, OptimizationGuideModelExecutionError>>
      future;
  session->SetInput(std::move(request), future.GetCallback());
  EXPECT_EQ(future.Get().error().error(),
            OptimizationGuideModelExecutionError::ModelExecutionError::
                kInvalidRequest);
}

TEST_F(OnDeviceModelServiceControllerTest, TokenCounts) {
  Initialize(standard_assets_);

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(), "execute:foo max:1024");
  EXPECT_EQ(response_.input_token_count(), strlen("execute:foo"));
  EXPECT_EQ(response_.output_token_count(), strlen("execute:foo max:1024"));
}

TEST_F(OnDeviceModelServiceControllerTest, ResponseConstraintOnExecute) {
  Initialize(standard_assets_);
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModelWithResponseConstraint(
      PageUrlRequest("input"),
      on_device_model::mojom::ResponseConstraint::NewRegex("[A-Z]*"),
      response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "Constraint: regex [A-Z]*"
            "execute:input max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, ResponseConstraintConfigJson) {
  FakeAdaptationAsset test_asset({
      .config =
          []() {
            auto config = SimpleComposeConfig();
            config.mutable_output_config()
                ->mutable_response_constraint()
                ->set_json_schema("{ type: \"object\"}");
            return config;
          }(),
  });

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&test_asset},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->ExecuteModel(PageUrlRequest("input"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "Constraint: json { type: \"object\"}"
            "execute:input max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, ResponseConstraintConfigRegex) {
  FakeAdaptationAsset test_asset({
      .config =
          []() {
            auto config = SimpleComposeConfig();
            config.mutable_output_config()
                ->mutable_response_constraint()
                ->set_regex("[A-Z]*");
            return config;
          }(),
  });

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&test_asset},
  });

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->ExecuteModel(PageUrlRequest("input"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "Constraint: regex [A-Z]*"
            "execute:input max:1024");
}

TEST_F(OnDeviceModelServiceControllerTest, EvictModelForRankUpdate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {features::kOptimizationGuideOnDeviceModel,
           {{"allowed_adaptation_ranks", "1,32"}}},
      },
      /*disabled_features=*/{});
  std::vector<uint32_t> initial_ranks = {1, 32};

  auto get_current_ranks =
      [launcher = &broker_.launcher()]() -> std::vector<uint32_t> {
    auto* service = launcher->service();
    if (!service) {
      return std::vector<uint32_t>();
    }
    auto* model = service->model();
    if (!model) {
      return std::vector<uint32_t>();
    }
    return model->data().adaptation_ranks;
  };

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset rank1_asset({
      .config =
          []() {
            auto config = SimpleComposeConfig();
            config.set_can_skip_text_safety(true);
            config.set_adaptation_rank(1);
            config.mutable_sampling_params()->set_top_k(1);
            config.mutable_sampling_params()->set_temperature(0);
            return config;
          }(),
      .weight = 10,
  });
  FakeAdaptationAsset rank2_asset({
      .config =
          []() {
            auto config = SimpleComposeConfig();
            config.set_can_skip_text_safety(true);
            config.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
            config.set_adaptation_rank(2);
            return config;
          }(),
      .weight = 20,
  });

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      // Init with just the
      .adaptations = {&rank1_asset},
  });

  auto session = CreateSession(rank1_asset.feature(), SessionConfigParams{});
  ASSERT_TRUE(session);
  MultimodalMessage msg1(PageUrlRequest("input"));
  session->SetInput(std::move(msg1), {});

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(get_current_ranks(), initial_ranks);

  // The rank1 feature shouldn't require an eviction at any point, because
  // it's in the allowed_adaptation_ranks.

  // The rank2 feature "download" finishing should evict the model.
  broker_.UpdateModelAdaptation(rank2_asset);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(get_current_ranks(), std::vector<uint32_t>());

  // Session should work even after the eviction, and just reload the model.
  session->ExecuteModel(proto::ComposeRequest(),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "Adaptation model: 10"
            "ctx: max:8192"
            "execute:input max:1024");

  std::vector<uint32_t> expected_ranks{1, 32, 2};
  EXPECT_EQ(get_current_ranks(), expected_ranks);
}

}  // namespace optimization_guide
