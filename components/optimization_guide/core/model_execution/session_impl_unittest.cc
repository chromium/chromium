// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/session_impl.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/fake_manifest_broker.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/scenario_builder.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_execution.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/performance_class.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/manifest.pb.h"
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

const std::string& GetCheckText(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.request().text_safety_model_request().text();
}

}  // namespace

std::string ConcatResponses(const std::vector<std::string>& responses) {
  std::string concat_responses;
  for (const std::string& response : responses) {
    concat_responses += response;
  }
  return concat_responses;
}

class SessionImplTest : public testing::Test {
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
    model_execution::prefs::RecordFeatureUsage(&broker_.local_state(),
                                               mojom::OnDeviceFeature::kTest);
  }

  void Initialize(proto::SolutionConfig solution_config) {
    if (solution_config.capabilities().empty()) {
      solution_config.add_capabilities(
          proto::ON_DEVICE_MODEL_CAPABILITY_IMAGE_INPUT);
      solution_config.add_capabilities(
          proto::ON_DEVICE_MODEL_CAPABILITY_AUDIO_INPUT);
    }

    // TODO(crbug.com/512149280): Move repetition checker to solution config.
    // Currently, this is a hardcoded override in IsRepetitionTrackedFeature.
    if (solution_config.feature().feature() !=
        proto::MODEL_EXECUTION_FEATURE_PROOFREADER_API) {
      solution_config.mutable_feature()->set_feature(
          ToModelExecutionFeatureProto(mojom::OnDeviceFeature::kTest));
    }
    proto::ModelExecutionFeature feature_proto =
        solution_config.feature().feature();
    mojom::OnDeviceFeature feature = *ToOnDeviceFeature(feature_proto);
    std::string use_case = ToUseCaseName(feature);
    if (solution_config.has_safety()) {
      solution_config.mutable_safety()->set_feature(feature_proto);
    }

    ScenarioBuilder builder(broker_.component_state());
    builder.AddBaseModel(
        "base_model",
        BaseModelRecipeArgs(
            proto::BaseModelRecipe::BACKEND_TYPE_GPU,
            proto::BaseModelRecipe::PERFORMANCE_HINT_HIGHEST_QUALITY, {},
            /*max_tokens=*/0),
        FakeBaseModelAsset::Content{}, "1");
    if (solution_config.has_safety()) {
      builder.AddSafetyModel("safety_model");
      builder.AddSafeSolution(use_case, "base_model", "safety_model",
                              std::move(solution_config));
    } else {
      builder.AddUnsafeSolution(use_case, "base_model",
                                std::move(solution_config));
    }
    builder.Finish();

    broker_.Startup();
    broker_.client().RequestAssetsFor(use_case);
    // Wait for configs and assets to be loaded.
    task_environment_.RunUntilIdle();
  }

  void Initialize() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfigForTesting();
    *config.mutable_safety() = ComposeSafetyConfig();
    Initialize(std::move(config));
  }

  void SimulateShutdown() {
    broker_.SimulateShutdown();
    broker_.launcher().CrashService();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  std::unique_ptr<OnDeviceSession> CreateSession(
      const SessionConfigParams& params) {
    return CreateSession(mojom::OnDeviceFeature::kTest, params);
  }
  std::unique_ptr<OnDeviceSession> CreateSession(
      mojom::OnDeviceFeature feature,
      const SessionConfigParams& params) {
    return broker_.state().StartSession(feature, params, logger_.GetWeakPtr());
  }

  void ExpectFailedSession(OnDeviceModelEligibilityReason reason) {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(CreateSession(SessionConfigParams{}));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Test",
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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeManifestBroker broker_;
  ResponseHolder response_;
  base::test::ScopedFeatureList feature_list_;
  OptimizationGuideLogger logger_;
};

TEST_F(SessionImplTest, ScoreBeforeContext) {
  Initialize();

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_NE(score_future.Get(), std::nullopt);
}

TEST_F(SessionImplTest, ScorePresentAfterContext) {
  Initialize();

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));

  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_EQ(score_future.Get(), 0.5);
}

TEST_F(SessionImplTest, ScoreAfterExecute) {
  Initialize();

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));
  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());

  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_NE(score_future.Get(), std::nullopt);
}

TEST_F(SessionImplTest, TokenLimits) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& input_config = *config.mutable_feature()->mutable_input_config();
    input_config.set_min_context_tokens(5);
    input_config.set_max_context_tokens(5);
    input_config.set_max_execute_tokens(3);
    config.mutable_feature()->mutable_output_config()->set_max_output_tokens(1);
    *config.mutable_safety() = ComposeSafetyConfig();
    return config;
  }());
  auto session = CreateSession(SessionConfigParams{});
  const TokenLimits& limits = session->GetTokenLimits();
  EXPECT_EQ(limits.max_tokens, 10240u);
  EXPECT_EQ(limits.min_context_tokens, 5u);
  EXPECT_EQ(limits.max_context_tokens, 5u);
  EXPECT_EQ(limits.max_execute_tokens, 3u);
  EXPECT_EQ(limits.max_output_tokens, 1u);
}

TEST_F(SessionImplTest, TokenLimitsCapped) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& input_config = *config.mutable_feature()->mutable_input_config();
    input_config.set_min_context_tokens(100000);
    input_config.set_max_context_tokens(100000);
    input_config.set_max_execute_tokens(100000);
    config.mutable_feature()->mutable_output_config()->set_max_output_tokens(
        100000);
    *config.mutable_safety() = ComposeSafetyConfig();
    return config;
  }());
  auto session = CreateSession(SessionConfigParams{});
  const TokenLimits& limits = session->GetTokenLimits();
  EXPECT_EQ(limits.max_tokens, 10240u);
  EXPECT_EQ(limits.min_context_tokens, 10240u);
  EXPECT_EQ(limits.max_context_tokens, 10240u);
  EXPECT_EQ(limits.max_execute_tokens, 10240u);
  EXPECT_EQ(limits.max_output_tokens, 10240u);
}

TEST_F(SessionImplTest, ExecutionDisconnectUnknown) {
  Initialize();
  auto session = CreateSession(SessionConfigParams{});
  broker_.settings().set_execute_error(
      on_device_model::mojom::GenerateError::kUnknown);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  EXPECT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(response_.error(), OnDeviceError::kCancelled);
}

TEST_F(SessionImplTest, ExecutionDisconnectInvalidConstraint) {
  Initialize();
  auto session = CreateSession(SessionConfigParams{});
  broker_.settings().set_execute_error(
      on_device_model::mojom::GenerateError::kInvalidConstraint);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  EXPECT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(response_.error(), OnDeviceError::kInvalidRequest);
}

TEST_F(SessionImplTest, SucceedsWithPassingSafetyChecks) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({"safe_output"});
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

TEST_F(SessionImplTest, FailsWithFailingRequestSafetyChecks) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("unsafe_url"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.error(), OnDeviceError::kFiltered);

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

TEST_F(SessionImplTest, FailsWithInvalidRequestSafetyChecks) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(SessionImplTest, FailsWithFailingRawOutputSafetyChecks) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({"unsafe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.error(), OnDeviceError::kFiltered);

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

TEST_F(SessionImplTest, FailsWithInvalidRawOutputChecks) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());

  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(SessionImplTest, SucceedsWithPassingResponseSafetyCheck) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({"safe_output"});
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

TEST_F(SessionImplTest, FailsWithFailingResponseSafetyCheck) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_un"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.error(), OnDeviceError::kFiltered);
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_unsafe_output")));
}

TEST_F(SessionImplTest, FailsWithInvalidResponseSafetyCheck) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_very_"),
                        response_.GetStreamingCallback());

  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(SessionImplTest, ReturnsErrorOnServiceDisconnect) {
  Initialize();

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  task_environment_.RunUntilIdle();

  broker_.launcher().CrashService();
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kDisconnectAndCancel, 1);

  ASSERT_TRUE(response_.error());
  EXPECT_EQ(*response_.error(), OnDeviceError::kCancelled);
}

TEST_F(SessionImplTest, CancelsExecuteOnAddContext) {
  Initialize();
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  task_environment_.RunUntilIdle();

  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  session->AddContext(UserInputRequest("bar"));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kCancelled, 1);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_.error());
  EXPECT_EQ(*response_.error(), OnDeviceError::kCancelled);
}

TEST_F(SessionImplTest, CancelsExecuteOnExecute) {
  Initialize();
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  ResponseHolder resp1;
  ResponseHolder resp2;
  session->ExecuteModel(PageUrlRequest("foo"), resp1.GetStreamingCallback());
  session->ExecuteModel(PageUrlRequest("bar"), resp2.GetStreamingCallback());

  EXPECT_FALSE(resp1.GetFinalStatus());
  EXPECT_TRUE(resp2.GetFinalStatus());
  EXPECT_EQ(*resp1.error(), OnDeviceError::kCancelled);
  EXPECT_EQ(*resp2.value(), "execute:bar max:1024");
}

TEST_F(SessionImplTest, AddContextDisconnectExecute) {
  Initialize();
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
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kUsedOnDevice, 1);
  std::string expected_response =
      ("ctx:foo max:8192"
       "execute:foobaz max:1024");
  EXPECT_EQ(*response_.value(), expected_response);
}

TEST_F(SessionImplTest, AddContextExecuteDisconnect) {
  Initialize();
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

TEST_F(SessionImplTest, AddContextMultipleSessions) {
  Initialize();
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

TEST_F(SessionImplTest, FailsOnGpuBlockedService) {
  Initialize();
  broker_.settings().service_disconnect_reason =
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
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Test",
        SessionImpl::AddContextResult::kUsingServer, 1);
  }
  session->ExecuteModel(PageUrlRequest("2"), response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_FALSE(broker_.launcher().did_launch_service());
}

TEST_F(SessionImplTest, AddContextInvalidConfig) {
  Initialize([]() {
    proto::SolutionConfig config;
    auto* feature = config.mutable_feature();
    feature->set_can_skip_text_safety(true);
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  {
    base::HistogramTester histogram_tester;
    session->AddContext(UserInputRequest("foo"));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Test",
        SessionImpl::AddContextResult::kFailedConstructingInput, 1);
  }
  task_environment_.RunUntilIdle();
  {
    base::HistogramTester histogram_tester;
    session->ExecuteModel(PageUrlRequest("2"),
                          response_.GetStreamingCallback());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
        ExecuteModelResult::kOnDeviceNotUsed, 1);
  }
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(SessionImplTest, ExecuteInvalidConfig) {
  Initialize([]() {
    proto::SolutionConfig config;
    auto* feature = config.mutable_feature();
    feature->set_can_skip_text_safety(true);
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  base::HistogramTester histogram_tester;
  session->ExecuteModel(PageUrlRequest("2"), response_.GetStreamingCallback());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kFailedConstructingMessage, 1);
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(SessionImplTest, FailOnDisconnectWhileWaitingForExecute) {
  Initialize();
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  task_environment_.RunUntilIdle();
  broker_.launcher().CrashService();
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kDisconnectAndCancel, 1);
  ASSERT_FALSE(response_.GetFinalStatus());
}

TEST_F(SessionImplTest, DestroySessionWhileWaitingForResponse) {
  Initialize();
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  const auto total_time = base::Seconds(11);
  task_environment_.AdvanceClock(total_time);
  session.reset();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kDestroyedWhileWaitingForResponse, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceDestroyedWhileWaitingForResponseTime.Test",
      total_time, 1);
}


TEST_F(SessionImplTest, DetectsRepeatsAndCancelsResponse) {
  base::HistogramTester histogram_tester;
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = UnsafeComposeConfig();
    return config;
  }());

  broker_.settings().set_execute_result({
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
  EXPECT_EQ(*response_.error(), OnDeviceError::kResponseLowQuality);

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
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kResponseHadRepeats, 1);
}

TEST_F(SessionImplTest, ExcusedFeaturesIgnoreRepeats) {
  // TODO(crbug.com/512149280): Move repetition checker to solution config.
  // Currently, this is a hardcoded override in IsRepetitionTrackedFeature.
  // Mark kProofreaderApi as used so the model is eligible.
  model_execution::prefs::RecordFeatureUsage(
      &broker_.local_state(), mojom::OnDeviceFeature::kProofreaderApi);

  base::HistogramTester histogram_tester;
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = UnsafeComposeConfig();
    config.mutable_feature()->set_feature(
        proto::MODEL_EXECUTION_FEATURE_PROOFREADER_API);
    return config;
  }());

  const std::vector<std::string> expected_responses = {
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  };
  broker_.settings().set_execute_result(expected_responses);

  auto session = CreateSession(mojom::OnDeviceFeature::kProofreaderApi,
                               SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  EXPECT_TRUE(response_.GetFinalStatus());

  EXPECT_TRUE(response_.value());
  EXPECT_FALSE(response_.error());

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
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult."
      "ProofreaderApi",
      ExecuteModelResult::kUsedOnDevice, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats."
      "ProofreaderApi",
      false, 1);
}

TEST_F(SessionImplTest, DetectsRepeatsAcrossResponses) {
  base::HistogramTester histogram_tester;
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = UnsafeComposeConfig();
    return config;
  }());

  broker_.settings().set_execute_result({
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

  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(*response_.error(), OnDeviceError::kResponseLowQuality);

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
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kResponseHadRepeats, 1);
}

TEST_F(SessionImplTest, IgnoresNonRepeatingText) {
  base::HistogramTester histogram_tester;
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = UnsafeComposeConfig();
    return config;
  }());

  broker_.settings().set_execute_result({
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
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Test",
      false, 1);
}

TEST_F(SessionImplTest, WithholdsTrailingNewlinesAcrossResponses) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = UnsafeComposeConfig();
    return config;
  }());

  broker_.settings().set_execute_result({
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

TEST_F(SessionImplTest, WithholdsTrailingNewlinesNoTrailingNewlines) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = UnsafeComposeConfig();
    return config;
  }());

  broker_.settings().set_execute_result({
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


TEST_F(SessionImplTest, UsesSessionTopKAndTemperature) {
  // Session sampling params should have precedence over feature ones.
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    config.mutable_feature()->mutable_sampling_params()->set_top_k(4);
    config.mutable_feature()->mutable_sampling_params()->set_temperature(1.5);
    *config.mutable_safety() = ComposeSafetyConfig();
    return config;
  }());

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
TEST_F(SessionImplTest, TsInterval0) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    *config.mutable_safety() = std::move(safety_config);
    return config;
  }());
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"token1", " token2", " token3",
                                           " token4"};
  broker_.settings().set_execute_result(tokens);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(*response_.value(), ConcatResponses(tokens));
  EXPECT_THAT(response_.partials(), ElementsAreArray(tokens));
}

// Validate that token interval 1 evaluates all partial output.
TEST_F(SessionImplTest, TsInterval1) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(1);
    *config.mutable_safety() = std::move(safety_config);
    return config;
  }());
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"token1", " token2", " token3",
                                           " token4"};
  broker_.settings().set_execute_result(tokens);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(*response_.value(), ConcatResponses(tokens));
  EXPECT_THAT(response_.partials(), ElementsAreArray(tokens));
}

// Validate that token interval 3 only evaluates every third and final chunk.
TEST_F(SessionImplTest, TsInterval3) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(3);
    *config.mutable_safety() = std::move(safety_config);
    return config;
  }());
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"token1",  " token2", " token3",
                                           " token4", " token5", " token6",
                                           " token7"};
  broker_.settings().set_execute_result(tokens);
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
TEST_F(SessionImplTest, MinimumSafetyTokens) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_minimum_tokens(2);
    safety_config.mutable_partial_output_checks()->set_token_interval(1);
    *config.mutable_safety() = std::move(safety_config);
    return config;
  }());
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"token1", " token2", " token3",
                                           " token4"};
  broker_.settings().set_execute_result(tokens);
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

TEST_F(SessionImplTest, WaitUntilCompleteToCancel) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto safety_config = ComposeSafetyConfig();
    safety_config.set_only_cancel_unsafe_response_on_complete(true);
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.add_allowed_languages("en");
    *config.mutable_safety() = std::move(safety_config);
    return config;
  }());
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  const std::vector<std::string> tokens = {"safe", " safe", " lang:en=1.0",
                                           " safe", " unsafe"};
  broker_.settings().set_execute_result(tokens);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());

  // The full output was unsafe so it resulted it in it being filtered.
  EXPECT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(response_.error(), OnDeviceError::kFiltered);

  const std::vector<std::string> expected_responses = {
      // The first two responses are filtered because their language hasn't been
      // detected yet. Because `only_cancel_unsafe_response_on_complete` is
      // true, this doesn't cause the input to be cancelled.
      //
      // "safe", "safe safe",

      // The next two responses are not filtered because the language has been
      // reliably detected as a supported language.
      "safe safe lang:en=1.0",
      " safe",
      // The last response is unsafe so it is filtered. Since the output is
      // complete the response is cancelled.
      //
      // "safe safe lang:en=1.0 safe unsafe",
  };
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));
}

class SessionImplTsIntervalTest : public SessionImplTest,
                                  public ::testing::WithParamInterface<int> {};

TEST_P(SessionImplTsIntervalTest, DetectsRepeatsWithSafetyModel) {
  base::HistogramTester histogram_tester;
  Initialize([&]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(
        GetParam());
    *config.mutable_safety() = std::move(safety_config);
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  broker_.settings().set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " unsafe stuff not processed",
  });
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(*response_.error(), OnDeviceError::kResponseLowQuality);

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
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Test",
      ExecuteModelResult::kResponseHadRepeats, 1);
}

INSTANTIATE_TEST_SUITE_P(SessionImplTsIntervalTests,
                         SessionImplTsIntervalTest,
                         testing::ValuesIn<int>({1, 2, 3, 4, 10}));

TEST_F(SessionImplTest, ImageExecutionSuccess) {
  using RequestProto = ::optimization_guide::proto::ExampleForTestingRequest;
  using NestedProto = ::optimization_guide::proto::ExampleForTestingMessage;
  proto::SolutionConfig solution_config;
  auto* config = solution_config.mutable_feature();
  auto& input_config = *config->mutable_input_config();
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
  auto* sampling = config->mutable_sampling_params();
  sampling->set_top_k(1);
  sampling->set_temperature(0);
  *config->mutable_output_config() = ResponseHolderOutputConfig();
  *solution_config.mutable_safety() = ComposeSafetyConfig();
  Initialize(std::move(solution_config));
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

TEST_F(SessionImplTest, KeepInputOnExtension) {
  using Request = proto::ExampleForTestingRequest;
  auto kRepeatedTag = Request::kRepeatedFieldFieldNumber;
  using Msg = proto::ExampleForTestingMessage;
  // A simple config that includes content from the
  // proto::ExampleForTestingRequest::repeated_field
  Initialize([]() {
    proto::SolutionConfig config;
    auto* feature = config.mutable_feature();
    auto* sampling = feature->mutable_sampling_params();
    sampling->set_top_k(1);
    sampling->set_temperature(0);
    *feature->mutable_input_config() = TestInputConfig(
        ForEachRepeated(FormatTestMessage()), EmptySubstitution());
    *feature->mutable_output_config() = ResponseHolderOutputConfig();
    *config.mutable_safety() = ComposeSafetyConfig();
    return config;
  }());
  base::test::TestFuture<base::expected<size_t, OnDeviceError>>
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

TEST_F(SessionImplTest, OmitEmptyInputs) {
  // Avoid calling Append with empty inputs.
  Initialize([]() {
    proto::SolutionConfig config;
    auto* feature = config.mutable_feature();
    auto* sampling = feature->mutable_sampling_params();
    sampling->set_top_k(1);
    sampling->set_temperature(0);
    auto& input_config = *feature->mutable_input_config();
    input_config.set_request_base_name(
        proto::ExampleForTestingRequest().GetTypeName());
    *input_config.add_input_context_substitutions() = EmptySubstitution();
    *input_config.add_execute_substitutions() = EmptySubstitution();
    *feature->mutable_output_config() = ResponseHolderOutputConfig();
    *config.mutable_safety() = ComposeSafetyConfig();
    return config;
  }());
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

TEST_F(SessionImplTest, CloneUsesSessionTopKAndTemperature) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    config.mutable_feature()->mutable_sampling_params()->set_top_k(4);
    config.mutable_feature()->mutable_sampling_params()->set_temperature(1.5);
    *config.mutable_safety() = ComposeSafetyConfig();
    return config;
  }());

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

TEST_F(SessionImplTest, CloneFailsWithFailingRequestSafetyChecks) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfig();
    auto& safety_config = *config.mutable_safety();
    safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_COMPOSE);
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
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  auto clone = session->Clone();
  EXPECT_TRUE(clone);

  broker_.settings().set_execute_result({"safe_output"});
  clone->ExecuteModel(PageUrlRequest("unsafe_url"),
                      response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.error(), OnDeviceError::kFiltered);

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

TEST_F(SessionImplTest, ScoreAfterClone) {
  Initialize();

  base::HistogramTester histogram_tester;
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));

  auto clone = session->Clone();
  base::test::TestFuture<std::optional<float>> score_future;
  clone->Score("token", score_future.GetCallback());
  EXPECT_EQ(score_future.Get(), 0.5);
}

TEST_F(SessionImplTest, AddContextAndClone) {
  Initialize();
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

TEST_F(SessionImplTest, CloneBeforeAddContext) {
  Initialize();
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

TEST_F(SessionImplTest, CancelAddContextAndClone) {
  Initialize();
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

TEST_F(SessionImplTest, CloneAddContextDisconnectExecute) {
  Initialize();
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

TEST_F(SessionImplTest, Priority) {
  Initialize();

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

TEST_F(SessionImplTest, PriorityClone) {
  Initialize();

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

TEST_F(SessionImplTest, SetInputCallback) {
  Initialize();

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  MultimodalMessage request((UserInputRequest("foo")));
  base::test::TestFuture<base::expected<size_t, OnDeviceError>> future;
  session->SetInput(std::move(request), future.GetCallback());
  EXPECT_EQ(*future.Get(), std::string("ctx:foo").size());

  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "ctx:foo max:8192execute:foobar "
            "max:1024");
}

TEST_F(SessionImplTest, SetInputCallbackCancelled) {
  Initialize();

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  MultimodalMessage request((UserInputRequest("foo")));
  base::test::TestFuture<base::expected<size_t, OnDeviceError>> future1;
  base::test::TestFuture<base::expected<size_t, OnDeviceError>> future2;
  session->SetInput(request.Clone(), future1.GetCallback());
  session->SetInput(std::move(request), future2.GetCallback());

  // First request is cancelled, second request completes.
  EXPECT_EQ(future1.Get().error(), OnDeviceError::kCancelled);
  EXPECT_EQ(*future2.Get(), std::string("ctx:foo").size());

  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "ctx:foo max:8192execute:foobar "
            "max:1024");
}

TEST_F(SessionImplTest, SetInputCallbackError) {
  Initialize();

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  MultimodalMessage request((proto::ExampleForTestingRequest()));
  base::test::TestFuture<base::expected<size_t, OnDeviceError>> future;
  session->SetInput(std::move(request), future.GetCallback());
  EXPECT_EQ(future.Get().error(), OnDeviceError::kInvalidRequest);
}

TEST_F(SessionImplTest, TokenCounts) {
  Initialize();

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(), "execute:foo max:1024");
  EXPECT_EQ(response_.input_token_count(), strlen("execute:foo"));
  // +1 accounts for the terminating EOS/End-of-turn token.
  EXPECT_EQ(response_.output_token_count(), strlen("execute:foo max:1024") + 1);
}

TEST_F(SessionImplTest, ResponseConstraintOnExecute) {
  Initialize();
  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);
  session->ExecuteModelWithResponseConstraint(
      PageUrlRequest("input"),
      on_device_model::mojom::ResponseConstraint::NewRegex("[A-Z]*"),
      response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "Hint: constrained_decoding "
            "Constraint: regex [A-Z]*"
            "execute:input max:1024");
}

TEST_F(SessionImplTest, ResponseConstraintConfigJson) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfigForTesting();
    config.mutable_feature()
        ->mutable_output_config()
        ->mutable_response_constraint()
        ->set_json_schema("{ type: \"object\"}");
    *config.mutable_safety() = ComposeSafetyConfig();
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->ExecuteModel(PageUrlRequest("input"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "Hint: constrained_decoding "
            "Constraint: json { type: \"object\"}"
            "execute:input max:1024");
}

TEST_F(SessionImplTest, ResponseConstraintConfigRegex) {
  Initialize([]() {
    proto::SolutionConfig config;
    *config.mutable_feature() = SimpleComposeConfigForTesting();
    config.mutable_feature()
        ->mutable_output_config()
        ->mutable_response_constraint()
        ->set_regex("[A-Z]*");
    *config.mutable_safety() = ComposeSafetyConfig();
    return config;
  }());

  auto session = CreateSession(SessionConfigParams{});
  ASSERT_TRUE(session);

  session->ExecuteModel(PageUrlRequest("input"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(response_.value(),
            "Hint: constrained_decoding "
            "Constraint: regex [A-Z]*"
            "execute:input max:1024");
}

}  // namespace optimization_guide
