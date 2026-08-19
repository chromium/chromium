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
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/model_info.h"
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
using ::on_device_model::mojom::PerformanceClass;
using ExecuteModelResult = ::optimization_guide::OnDeviceExecution::Result;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::ResultOf;

auto SimpleComposeConfigForTesting() {
  auto cfg = SimpleComposeConfig();
  auto* sampling = cfg.mutable_sampling_params();
  sampling->set_top_k(1);
  sampling->set_temperature(0);
  return cfg;
}

auto UnsafeComposeConfig() {
  auto cfg = SimpleComposeConfigForTesting();
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
      .config = SimpleComposeConfigForTesting(),
  }};
  FakeSafetyModelAsset safety{ComposeSafetyConfig()};
  FakeLanguageModelAsset language;
};

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

constexpr auto kFeature = mojom::OnDeviceFeature::kCompose;

class OnDeviceModelServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kOnDeviceModelPerformanceParams,
          {{"compatible_on_device_performance_classes", "3,4,5,6"},
           {"compatible_low_tier_on_device_performance_classes", "3"}}},
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

TEST_F(OnDeviceModelServiceControllerTest, BaseModelExecutionSuccess) {
  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfigForTesting(),
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

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceFirstResponseTime.Compose",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceResponseCompleteTime.Compose",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceResponseCompleteTokens.Compose",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution."
      "OnDeviceResponseTokensTimeToNextToken.Compose",
      1);

  // If we destroy all sessions and wait long enough, everything should idle out
  // and the service should get terminated.
  session.reset();
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(broker_.launcher().is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest, CacheWeightExecutionSuccess) {
  // TODO(crbug.com/461547475): Determine whether weight caches should be used
  // for GPU or just CPU only. Stop setting this feature flag once that's no
  // longer the case.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      on_device_model::features::kOnDeviceModelForceCpuBackend);

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

TEST_F(OnDeviceModelServiceControllerTest,
       ShaderCacheExecutionSuccessWithFastestInferenceGpuModel) {
  base::test::ScopedFeatureList feature_list;
  // TODO(crbug.com/461547475): GPU cache flag is experimental for now, remove
  // once it's no longer needed.
  feature_list.InitAndEnableFeature(
      on_device_model::features::kOnDeviceModelGpuProgramCache);

  broker_.InstallBaseModel(std::make_unique<FakeBaseModelAsset>(
      std::vector<proto::OnDeviceModelPerformanceHint>{
          proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE},
      FakeBaseModelAsset::Content{
          .cache_weight = 1015,
          .encoder_cache_weight = 1016,
          .adapter_cache_weight = 1017,
          .shader_cache_data = "0xcafebabe",
      }));
  Initialize(InitializeParams{
      .base_model_content = std::nullopt,
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
            "Fastest inference"
            "Encoder cache weight: 1016"
            "Adapter cache weight: 1017"
            "Shader cache data: 0xcafebabe"
            "execute:foo max:1024");
  // Destroy the session and run until the service is no longer running.
  session.reset();
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  EXPECT_TRUE(broker_.launcher().did_launch_service());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !broker_.launcher().is_service_running(); }));
}

TEST_F(OnDeviceModelServiceControllerTest, AdaptationModelExecutionSuccess) {
  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfigForTesting(),
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
      .config = SimpleComposeConfigForTesting(),
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
      .config = SimpleComposeConfigForTesting(),
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
  FakeAdaptationAsset compose_asset(
      {.config = SimpleComposeConfigForTesting()});
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
  EXPECT_EQ(compose_response.error(), OnDeviceError::kFiltered);
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
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }
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

TEST_F(OnDeviceModelServiceControllerTest, DisconnectsWhenIdle) {
  const base::TimeDelta idle_timeout = features::GetOnDeviceModelIdleTimeout();
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

  EXPECT_EQ(broker_.GetOrCreateBrokerState()
                .base_model_controller()
                .GetCapabilities(),
            on_device_model::Capabilities(
                {on_device_model::CapabilityFlags::kImageInput}));
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

TEST_F(OnDeviceModelServiceControllerTest,
       BrokerCreateSessionFailedOnDeviceIncapable) {
  // Arrange for a performance check to be run which will classify the device as
  // ineligible for both GPU and CPU models.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      on_device_model::features::kOnDeviceModelCpuBackend);
  broker_.service_settings().performance_class = PerformanceClass::kVeryLow;
  broker_.local_state().SetString(
      model_execution::prefs::localstate::kOnDevicePerformanceClassVersion,
      "0.0.0.1");

  mojo::PendingReceiver<mojom::ModelBroker> pending_broker;
  OptimizationGuideLogger logger;

  ModelBrokerClient broker_client(pending_broker.InitWithNewPipeAndPassRemote(),
                                  logger.GetWeakPtr());
  base::test::TestFuture<std::unique_ptr<OnDeviceSession>> session_future;
  broker_client.CreateSession(mojom::OnDeviceFeature::kCompose,
                              SessionConfigParams{},
                              session_future.GetCallback());
  broker_.GetOrCreateBrokerState().BindModelBroker(std::move(pending_broker));
  EXPECT_EQ(session_future.Take(), nullptr);

  // Create session with another feature will also fail.
  broker_client.CreateSession(mojom::OnDeviceFeature::kTest,
                              SessionConfigParams{},
                              session_future.GetCallback());
  EXPECT_EQ(session_future.Take(), nullptr);
}

TEST_F(OnDeviceModelServiceControllerTest,
       BrokerCreateSessionFailedOnNotEnoughDiskSpace) {
  // 20gb is the default in `IsFreeDiskSpaceSufficientForOnDeviceModelInstall`.
  broker_.component_state().SetFreeDiskSpace(base::GiB(20) -
                                             base::ByteSizeDelta(1));

  mojo::PendingReceiver<mojom::ModelBroker> pending_broker;
  OptimizationGuideLogger logger;

  ModelBrokerClient broker_client(pending_broker.InitWithNewPipeAndPassRemote(),
                                  logger.GetWeakPtr());
  base::test::TestFuture<std::unique_ptr<OnDeviceSession>> session_future;
  broker_client.CreateSession(mojom::OnDeviceFeature::kCompose,
                              SessionConfigParams{},
                              session_future.GetCallback());
  broker_.GetOrCreateBrokerState().BindModelBroker(std::move(pending_broker));
  EXPECT_EQ(session_future.Take(), nullptr);

  // Create session with another feature will also fail
  broker_client.CreateSession(mojom::OnDeviceFeature::kTest,
                              SessionConfigParams{},
                              session_future.GetCallback());
  EXPECT_EQ(session_future.Take(), nullptr);
}

TEST_F(OnDeviceModelServiceControllerTest, EvictModelForRankUpdate) {
  std::vector<uint32_t> initial_ranks = {32};

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
            config.set_adaptation_rank(32);
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

  std::vector<uint32_t> expected_ranks{32, 2};
  EXPECT_EQ(get_current_ranks(), expected_ranks);
}

// Test fixtures for background download experiments.
struct BackgroundDownloadTestParams {
  std::string test_name;
  std::string allowed_features;
  bool was_feature_recently_used = false;
  bool is_feature_installed = false;
  std::vector<std::pair<mojom::OnDeviceFeature, OnDeviceModelEligibilityReason>>
      expectations;
};

class OnDeviceModelServiceControllerBackgroundDownloadTest
    : public OnDeviceModelServiceControllerTest,
      public testing::WithParamInterface<BackgroundDownloadTestParams> {};

TEST_P(OnDeviceModelServiceControllerBackgroundDownloadTest, Run) {
  const auto& params = GetParam();

  if (params.was_feature_recently_used) {
    model_execution::prefs::RecordFeatureUsage(
        &broker_.local_state(), mojom::OnDeviceFeature::kPromptApi);
  } else {
    broker_.local_state().ClearPref(
        model_execution::prefs::localstate::kLastUsageByFeature);
  }

  base::test::ScopedPowerMonitorTestSource power_monitor_source;
  // Set to external power so that `RegisterInstaller` is called and
  // `GetOnDeviceModelState` returns kSuccess.
  power_monitor_source.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kExternalPower);

  base::test::ScopedFeatureList feature_list;
  if (!params.allowed_features.empty()) {
    feature_list.InitAndEnableFeatureWithParameters(
        features::kOnDeviceModelBackgroundDownload,
        {{"allowed_features", params.allowed_features}});
  } else {
    feature_list.InitAndDisableFeature(
        features::kOnDeviceModelBackgroundDownload);
  }

  std::optional<FakeAdaptationAsset> fake_asset;
  std::vector<FakeAdaptationAsset*> adaptations = {&standard_assets_.compose};

  if (params.is_feature_installed) {
    fake_asset.emplace(FakeAdaptationAsset::Content{.config = [&] {
      auto config = SimpleComposeConfig();
      config.set_feature(proto::MODEL_EXECUTION_FEATURE_PROMPT_API);
      config.set_can_skip_text_safety(true);
      return config;
    }()});
    adaptations.push_back(&*fake_asset);
  }

  Initialize({
      .base_model_content = standard_assets_.base_model_content,
      .safety = nullptr,
      .language = nullptr,
      .adaptations = adaptations,
  });

  for (const auto& [feature, expected] : params.expectations) {
    EXPECT_EQ(
        broker_.GetOrCreateBrokerState().GetOnDeviceModelEligibility(feature),
        expected)
        << "for feature: " << feature;
  }
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OnDeviceModelServiceControllerBackgroundDownloadTest,
    testing::Values(
        BackgroundDownloadTestParams{
            .test_name = "SuccessIfBackgroundDownloadEnabled",
            .allowed_features = "PromptApi",
            .is_feature_installed = true,
            .expectations = {{mojom::OnDeviceFeature::kPromptApi,
                              OnDeviceModelEligibilityReason::kSuccess}}},
        BackgroundDownloadTestParams{
            .test_name = "SuccessIfFeatureUsed",
            .allowed_features = "Summarize",
            .was_feature_recently_used = true,
            .is_feature_installed = true,
            .expectations = {{mojom::OnDeviceFeature::kPromptApi,
                              OnDeviceModelEligibilityReason::kSuccess}}},
        BackgroundDownloadTestParams{
            .test_name =
                "ConfigNotAvailableIfBackgroundDownloadEnabledAndAssetPending",
            .allowed_features = "PromptApi",
            .expectations = {{mojom::OnDeviceFeature::kPromptApi,
                              OnDeviceModelEligibilityReason::
                                  kConfigNotAvailableForFeature}}},
        BackgroundDownloadTestParams{
            .test_name = "NoFeatureUsedIfBackgroundDownloadDisabledForFeature",
            .allowed_features = "Summarize",
            .expectations =
                {{mojom::OnDeviceFeature::kPromptApi,
                  OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed}}},
        BackgroundDownloadTestParams{
            .test_name = "NoFeatureUsedIfBackgroundDownloadDisabled",
            .expectations =
                {{mojom::OnDeviceFeature::kPromptApi,
                  OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed}}},
        BackgroundDownloadTestParams{
            .test_name = "NoFeatureUsedIfModelInstalledForOtherFeature",
            .allowed_features = "Summarize",
            .is_feature_installed = true,
            .expectations =
                {{mojom::OnDeviceFeature::kPromptApi,
                  OnDeviceModelEligibilityReason::kNoOnDeviceFeatureUsed}}}),
    [](const testing::TestParamInfo<BackgroundDownloadTestParams>& info) {
      return info.param.test_name;
    });

}  // namespace optimization_guide
