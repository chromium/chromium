// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
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
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/redaction.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ::on_device_model::mojom::LoadModelResult;
using ExecuteModelResult = SessionImpl::ExecuteModelResult;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::ResultOf;

const std::string& GetCheckText(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.request().text_safety_model_request().text();
}

void FailRemote(ModelBasedCapabilityKey key,
                const google::protobuf::MessageLite& req,
                std::unique_ptr<proto::LogAiDataRequest> log,
                OptimizationGuideModelExecutionResultCallback callback) {
  EXPECT_TRUE(false) << "Unexpected use of remote fallback";
  std::move(callback).Run(
      base::unexpected(OptimizationGuideModelExecutionError::FromHttpStatusCode(
          net::HTTP_BAD_REQUEST)),
      nullptr);
}

ExecuteRemoteFn FailOnRemoteFallback() {
  return base::BindRepeating(&FailRemote);
}

class FakeOnDeviceModelAvailabilityObserver
    : public OnDeviceModelAvailabilityObserver {
 public:
  explicit FakeOnDeviceModelAvailabilityObserver(
      ModelBasedCapabilityKey expected_feature) {
    expected_feature_ = expected_feature;
  }

  void OnDeviceModelAvailabilityChanged(
      ModelBasedCapabilityKey feature,
      OnDeviceModelEligibilityReason reason) override {
    EXPECT_EQ(expected_feature_, feature);
    reason_ = reason;
  }
  ModelBasedCapabilityKey expected_feature_;
  std::optional<OnDeviceModelEligibilityReason> reason_;
};

}  // namespace

std::vector<std::string> ConcatResponses(
    const std::vector<std::string>& responses) {
  std::vector<std::string> concat_responses;
  std::string current_response;
  for (const std::string& response : responses) {
    current_response += response;
    concat_responses.push_back(current_response);
  }
  return concat_responses;
}

constexpr auto kFeature = ModelBasedCapabilityKey::kCompose;

class ExpectedRemoteFallback final {
 public:
  struct FallbackArgs {
    ModelBasedCapabilityKey feature;
    std::unique_ptr<google::protobuf::MessageLite> request;
    std::unique_ptr<proto::LogAiDataRequest> log;
    OptimizationGuideModelExecutionResultCallback callback;

    const auto& logged_executions() {
      return log->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
    }
  };

  ExecuteRemoteFn CreateExecuteRemoteFn() {
    return base::BindLambdaForTesting(
        [&](ModelBasedCapabilityKey feature,
            const google::protobuf::MessageLite& m,
            std::unique_ptr<proto::LogAiDataRequest> l,
            OptimizationGuideModelExecutionResultCallback c) {
          auto request = base::WrapUnique(m.New());
          request->CheckTypeAndMergeFrom(m);
          future_.GetCallback().Run(FallbackArgs{
              feature,
              std::move(request),
              std::move(l),
              std::move(c),
          });
        });
  }

  proto::Any ComposeResponse(const std::string& output) {
    proto::ComposeResponse response;
    response.set_output(output);
    return AnyWrapProto(response);
  }

  FallbackArgs Take() { return future_.Take(); }

 private:
  base::test::TestFuture<FallbackArgs> future_;
};

class OnDeviceModelServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kOptimizationGuideOnDeviceModel,
          {{"on_device_model_min_tokens_for_context", "10"},
           {"on_device_model_max_tokens_for_context", "22"},
           {"on_device_model_context_token_chunk_size", "4"},
           {"on_device_model_topk", "1"},
           {"on_device_model_temperature", "0"}}},
         {features::kTextSafetyClassifier, {}},
         {features::kOnDeviceModelValidation,
          {{"on_device_model_validation_delay", "0"}}}},
        {features::internal::kModelAdaptationCompose});
    model_execution::prefs::RegisterLocalStatePrefs(pref_service_.registry());

    // Fake the requirements to install the model.
    pref_service_.SetInteger(
        model_execution::prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kHigh));
    pref_service_.SetTime(
        model_execution::prefs::GetOnDeviceFeatureRecentlyUsedPref(
            ModelBasedCapabilityKey::kCompose),
        base::Time::Now());
  }

  void TearDown() override {
    access_controller_ = nullptr;
    test_controller_ = nullptr;
  }

  struct InitializeParams {
    // The model execution config to write before initialization. Writes a
    // default configuration if not provided.
    std::optional<proto::OnDeviceModelExecutionFeatureConfig> config;
    std::optional<proto::OnDeviceModelExecutionFeatureConfig> config2;
    // Whether to make the downloaded model available prior to initialization of
    // the service controller.
    bool model_component_ready = true;

    std::optional<proto::OnDeviceModelValidationConfig> validation_config;
  };

  void Initialize() { Initialize({}); }

  void Initialize(const InitializeParams& params) {
    if (params.config) {
      base_model_asset_.Write(*params.config, params.config2,
                              params.validation_config);
    } else {
      auto default_config = SimpleComposeConfig();
      default_config.set_can_skip_text_safety(true);
      base_model_asset_.Write(default_config, std::nullopt,
                              params.validation_config);
    }

    if (params.model_component_ready) {
      on_device_component_state_manager_.get()->OnStartup();
      task_environment_.FastForwardBy(base::Seconds(1));
      on_device_component_state_manager_.SetReady(base_model_asset_.path());
    }

    RecreateServiceController();
    // Wait until the OnDeviceModelExecutionConfig has been read.
    task_environment_.RunUntilIdle();
  }

  ExecuteRemoteFn CreateExecuteRemoteFn() {
    return base::BindLambdaForTesting(
        [=, this](ModelBasedCapabilityKey feature,
                  const google::protobuf::MessageLite& m,
                  std::unique_ptr<proto::LogAiDataRequest> l,
                  OptimizationGuideModelExecutionResultCallback c) {
          remote_execute_called_ = true;
          last_remote_message_ = base::WrapUnique(m.New());
          last_remote_message_->CheckTypeAndMergeFrom(m);
          log_ai_data_request_passed_to_remote_ = std::move(l);

          if (feature == ModelBasedCapabilityKey::kTextSafety) {
            last_remote_ts_callback_ = std::move(c);
          }
        });
  }

  void RecreateServiceController() {
    access_controller_ = nullptr;
    test_controller_ = nullptr;

    auto access_controller =
        std::make_unique<OnDeviceModelAccessController>(pref_service_);
    access_controller_ = access_controller.get();
    test_controller_ = base::MakeRefCounted<FakeOnDeviceModelServiceController>(
        &fake_settings_, std::move(access_controller),
        on_device_component_state_manager_.get()->GetWeakPtr());

    test_controller_->Init();
  }

  std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationController>&
  GetModelAdaptationControllers() const {
    return test_controller_->model_adaptation_controllers_;
  }

 protected:
  FakeBaseModelAsset base_model_asset_;
  FakeLanguageModelAsset language_asset_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  TestOnDeviceModelComponentStateManager on_device_component_state_manager_{
      &pref_service_};
  scoped_refptr<FakeOnDeviceModelServiceController> test_controller_;
  // Owned by FakeOnDeviceModelServiceController.
  raw_ptr<OnDeviceModelAccessController> access_controller_ = nullptr;
  ResponseHolder response_;
  base::test::ScopedFeatureList feature_list_;
  bool remote_execute_called_ = false;
  std::unique_ptr<google::protobuf::MessageLite> last_remote_message_;
  std::unique_ptr<proto::LogAiDataRequest>
      log_ai_data_request_passed_to_remote_;
  OptimizationGuideModelExecutionResultCallback last_remote_ts_callback_;
  OptimizationGuideLogger logger_;
};

TEST_F(OnDeviceModelServiceControllerTest, ScoreNullBeforeContext) {
  Initialize();

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_EQ(score_future.Get(), std::nullopt);
}

TEST_F(OnDeviceModelServiceControllerTest, ScorePresentAfterContext) {
  Initialize();

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));

  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_EQ(score_future.Get(), 0.5);
}

TEST_F(OnDeviceModelServiceControllerTest, ScoreNullAfterExecute) {
  Initialize();

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));
  session->ExecuteModel(PageUrlRequest("bar"), response_.callback());

  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_EQ(score_future.Get(), std::nullopt);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionSuccess) {
  Initialize();

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  const std::string expected_response = "Input: execute:foo\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_TRUE(*response_.provided_by_on_device());
  EXPECT_THAT(response_.streamed(), ElementsAre(expected_response));
  EXPECT_TRUE(response_.log_entry());
  const auto logged_on_device_model_execution_info =
      response_.log_entry()
          ->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info();
  const auto& model_version =
      logged_on_device_model_execution_info.model_versions()
          .on_device_model_service_version();
  EXPECT_EQ(model_version.component_version(), "0.0.1");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_name(),
            "Test");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_version(),
            "0.0.1");
  EXPECT_FALSE(model_version.model_adaptation_version());
  EXPECT_GT(logged_on_device_model_execution_info.execution_infos_size(), 0);
  EXPECT_EQ(logged_on_device_model_execution_info.execution_infos(0)
                .response()
                .on_device_model_service_response()
                .status(),
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_SUCCESS);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       MultipleModelAdaptationExecutionSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kModelAdaptationCompose, {}},
       {features::internal::kOnDeviceModelTestFeature,
        {{"enable_adaptation", "true"}}}},
      {});

  auto config_compose = SimpleComposeConfig();
  config_compose.set_can_skip_text_safety(true);
  auto config_test = SimpleComposeConfig();
  config_test.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  config_test.set_can_skip_text_safety(true);

  Initialize({.config = config_compose, .config2 = config_test});

  FakeOnDeviceModelAvailabilityObserver availability_observer_compose(
      ModelBasedCapabilityKey::kCompose),
      availability_observer_test(ModelBasedCapabilityKey::kTest);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kCompose, &availability_observer_compose);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kTest, &availability_observer_test);

  FakeAdaptationAsset compose_asset({
      .config = config_compose,
      .weight = 1015,
  });
  test_controller_->MaybeUpdateModelAdaptation(compose_asset.feature(),
                                               compose_asset.metadata());
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_compose.reason_);
  EXPECT_FALSE(availability_observer_test.reason_);

  FakeAdaptationAsset test_asset({
      .config = config_test,
      .weight = 2024,
  });
  test_controller_->MaybeUpdateModelAdaptation(test_asset.feature(),
                                               test_asset.metadata());
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_test.reason_);

  auto session_compose = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session_compose);
  task_environment_.RunUntilIdle();
  auto session_test = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, base::DoNothing(), logger_.GetWeakPtr(),
      nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session_test);

  EXPECT_EQ(2u, GetModelAdaptationControllers().size());

  session_compose->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  EXPECT_EQ(*response_.value(), "Adaptation model: 1015\nInput: execute:foo\n");
  EXPECT_TRUE(*response_.provided_by_on_device());

  session_test->ExecuteModel(PageUrlRequest("bar"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  EXPECT_EQ(*response_.value(), "Adaptation model: 2024\nInput: execute:bar\n");
  EXPECT_TRUE(*response_.provided_by_on_device());

  EXPECT_TRUE(response_.log_entry());
  const auto logged_on_device_model_execution_info =
      response_.log_entry()
          ->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info();
  const auto& model_version =
      logged_on_device_model_execution_info.model_versions()
          .on_device_model_service_version();
  EXPECT_EQ(model_version.component_version(), "0.0.1");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_name(),
            "Test");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_version(),
            "0.0.1");
  EXPECT_EQ(model_version.model_adaptation_version(), compose_asset.version());

  session_compose.reset();
  session_test.reset();

  // Fast forward by the amount of time that triggers an idle disconnect. All
  // adaptations and the base model should be reset.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  EXPECT_TRUE(GetModelAdaptationControllers().empty());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());
}

TEST_F(OnDeviceModelServiceControllerTest, ModelAdaptationAndBaseModelSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kModelAdaptationCompose, {}},
       {features::internal::kOnDeviceModelTestFeature,
        {{"enable_adaptation", "false"}}}},
      {});

  auto config_compose = SimpleComposeConfig();
  config_compose.set_can_skip_text_safety(true);
  auto config_test = SimpleComposeConfig();
  config_test.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  config_test.set_can_skip_text_safety(true);

  Initialize({.config = config_compose, .config2 = config_test});

  FakeOnDeviceModelAvailabilityObserver availability_observer_compose(
      ModelBasedCapabilityKey::kCompose);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kCompose, &availability_observer_compose);

  FakeAdaptationAsset compose_asset({
      .config = config_compose,
      .weight = 1015,
  });
  test_controller_->MaybeUpdateModelAdaptation(compose_asset.feature(),
                                               compose_asset.metadata());
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_compose.reason_);

  auto session_compose = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  task_environment_.RunUntilIdle();
  auto session_test = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, base::DoNothing(), logger_.GetWeakPtr(),
      nullptr,
      /*config_params=*/std::nullopt);

  EXPECT_EQ(1u, GetModelAdaptationControllers().size());

  session_compose->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  EXPECT_EQ(*response_.value(), "Adaptation model: 1015\nInput: execute:foo\n");
  EXPECT_TRUE(*response_.provided_by_on_device());

  session_test->ExecuteModel(PageUrlRequest("bar"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  EXPECT_EQ(*response_.value(), "Input: execute:bar\n");
  EXPECT_TRUE(*response_.provided_by_on_device());

  session_compose.reset();
  session_test.reset();

  // Fast forward by the amount of time that triggers an idle disconnect. The
  // base model will still be connected since it needs to wait for 2 idle
  // timeouts (one for the adaptation and one for it's own timeout).
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  EXPECT_TRUE(GetModelAdaptationControllers().empty());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());
  EXPECT_EQ(1ull, test_controller_->on_device_model_receiver_count());

  // Fast forward by another idle timeout. The base model remote will be reset.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelAdaptationEmptyWeightsUsesBaseModel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kModelAdaptationCompose, {}}}, {});

  auto config_compose = SimpleComposeConfig();
  config_compose.set_can_skip_text_safety(true);

  Initialize({.config = config_compose});

  FakeOnDeviceModelAvailabilityObserver availability_observer_compose(
      ModelBasedCapabilityKey::kCompose);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kCompose, &availability_observer_compose);

  FakeAdaptationAsset compose_asset({
      .config = config_compose,
      .weight = std::nullopt,
  });
  test_controller_->MaybeUpdateModelAdaptation(compose_asset.feature(),
                                               compose_asset.metadata());
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_compose.reason_);

  auto session_compose = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(GetModelAdaptationControllers().empty());

  session_compose->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  EXPECT_EQ(*response_.value(), "Input: execute:foo\n");
  EXPECT_TRUE(*response_.provided_by_on_device());

  session_compose.reset();

  // Fast forward by idle timeout. The base model remote will be reset.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelExecutionFeatureExecutionNotEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kOptimizationGuideComposeOnDeviceEval});

  Initialize();

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_FALSE(session);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionWithContext) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  {
    base::HistogramTester histogram_tester;
    session->AddContext(UserInputRequest("foo"));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kUsingOnDevice, 1);
  }
  task_environment_.RunUntilIdle();

  session->AddContext(UserInputRequest("bar"));
  session->ExecuteModel(PageUrlRequest("baz"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  const std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:bar off:0 max:10\n",
      "Input: execute:barbaz\n",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelExecutionLoadsSingleContextChunk) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  session->AddContext(UserInputRequest("context"));
  task_environment_.RunUntilIdle();

  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:contex off:0 max:10\n",
      "Context: t off:10 max:4\n",
      "Input: execute:contextfoo\n",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelExecutionLoadsLongContextInChunks) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  session->AddContext(UserInputRequest("this is long context"));
  task_environment_.RunUntilIdle();

  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:this i off:0 max:10\n",
      "Context: s lo off:10 max:4\n",
      "Context: ng c off:14 max:4\n",
      "Context: onte off:18 max:4\n",
      "Input: execute:this is long contextfoo\n",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelExecutionCancelsOptionalContext) {
  Initialize();
  fake_settings_.set_execute_delay(base::Seconds(10));
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  session->AddContext(UserInputRequest("this is long context"));
  // ExecuteModel() directly after AddContext() should only load first chunk.
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());

  // Give time to make sure we don't process the optional context.
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_.value());
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:this i off:0 max:10\n",
      "Input: execute:this is long contextfoo\n",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionModelToBeInstalled) {
  Initialize({.model_component_ready = false});

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_FALSE(session);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kModelToBeInstalled, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelAvailableAfterInit) {
  Initialize({.model_component_ready = false});

  // Model not yet available.
  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_FALSE(session);

  on_device_component_state_manager_.get()->OnStartup();
  task_environment_.RunUntilIdle();
  on_device_component_state_manager_.SetReady(base_model_asset_.path());
  task_environment_.RunUntilIdle();

  // Model now available.
  session = test_controller_->CreateSession(kFeature, base::DoNothing(),
                                            logger_.GetWeakPtr(), nullptr,
                                            /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
}

// Validates behavior of a session when execution config is updated after a
// session is created.
TEST_F(OnDeviceModelServiceControllerTest, MidSessionModelUpdate) {
  Initialize();

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);

  // Simulate a model update.
  base_model_asset_.Write({});
  on_device_component_state_manager_.SetReady(base_model_asset_.path());
  task_environment_.RunUntilIdle();

  // Verify the existing session still works.
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(response_.value());
  const std::string expected_response = "Input: execute:foo\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_TRUE(*response_.provided_by_on_device());
}

TEST_F(OnDeviceModelServiceControllerTest, SessionBeforeAndAfterModelUpdate) {
  Initialize();

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  session->AddContext(UserInputRequest("context"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1ull, test_controller_->on_device_model_receiver_count());

  // Simulates a model update. This should close the model remote.
  // Write a new empty execution config to check that the config is reloaded.
  base_model_asset_.Write({});
  on_device_component_state_manager_.SetReady(base_model_asset_.path());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0ull, test_controller_->on_device_model_receiver_count());

  // Create a new session and verify it fails due to the configuration.
  base::HistogramTester histogram_tester;
  session = test_controller_->CreateSession(kFeature, base::DoNothing(),
                                            logger_.GetWeakPtr(), nullptr,
                                            /*config_params=*/std::nullopt);
  ASSERT_FALSE(session);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, SessionFailsForInvalidFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::internal::kOnDeviceModelTestFeature);

  Initialize();
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, base::DoNothing(), logger_.GetWeakPtr(),
      nullptr, /*config_params=*/std::nullopt));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
      "Test",
      OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, UpdateSafetyModel) {
  FakeSafetyModelAsset fake_safety_asset(ComposeSafetyConfig());

  Initialize();

  // Safety model info is valid but no metadata.
  {
    base::HistogramTester histogram_tester;

    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kNoMetadata, 1);
  }

  // Safety model info is valid but metadata is of wrong type.
  {
    base::HistogramTester histogram_tester;

    proto::Any any;
    any.set_type_url("garbagetype");
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(any)
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kMetadataWrongType, 1);
  }

  // Safety model info is valid but no feature configs.
  {
    base::HistogramTester histogram_tester;

    proto::TextSafetyModelMetadata model_metadata;
    proto::Any any;
    any.set_type_url(
        "type.googleapis.com/optimization_guide.proto.TextSafetyModelMetadata");
    model_metadata.SerializeToString(any.mutable_value());
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(any)
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kNoFeatureConfigs, 1);
  }

  // Safety model info is valid and metadata has feature configs.
  {
    base::HistogramTester histogram_tester;

    proto::TextSafetyModelMetadata model_metadata;
    model_metadata.add_feature_text_safety_configurations()->set_feature(
        ToModelExecutionFeatureProto(kFeature));
    proto::Any any;
    any.set_type_url(
        "type.googleapis.com/optimization_guide.proto.TextSafetyModelMetadata");
    model_metadata.SerializeToString(any.mutable_value());
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(any)
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, UpdatingSafetyModelEnablesModels) {
  // Verifies that when we start a session before safety is available, that
  // future session that require a safety model still get one.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {features::internal::kModelAdaptationCompose, {}},
          {features::internal::kOnDeviceModelTestFeature,
           {{"enable_adaptation", "false"}}},
          {features::kTextSafetyClassifier,
           {{"on_device_retract_unsafe_content", "true"}}},
      },
      {});

  auto config_compose = SimpleComposeConfig();
  config_compose.set_can_skip_text_safety(false);
  auto config_test = SimpleComposeConfig();
  config_test.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  config_test.set_can_skip_text_safety(true);
  Initialize({.config = config_compose, .config2 = config_test});

  // Compose capability can't start because it's missing safety model.
  EXPECT_FALSE(test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, FailOnRemoteFallback(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt));

  // Test capability starts because it doesn't require a safety model.
  auto test_session = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, FailOnRemoteFallback(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(test_session);

  // Executing with test_session should force model to be loaded.
  ResponseHolder test_response;
  test_session->ExecuteModel(PageUrlRequest("unsafe"),
                             test_response.callback());
  EXPECT_TRUE(test_response.GetFinalStatus());

  // Compose capability should be available after safety model loads.
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
  auto compose_session = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, FailOnRemoteFallback(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(compose_session);

  ResponseHolder compose_response;
  compose_session->ExecuteModel(PageUrlRequest("unsafe"),
                                compose_response.callback());

  // Compose should run and be rejected as unsafe.
  EXPECT_FALSE(compose_response.GetFinalStatus());
  EXPECT_EQ(
      compose_response.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
}

TEST_F(OnDeviceModelServiceControllerTest, SessionRequiresSafetyModel) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  // No safety model received yet.
  {
    base::HistogramTester histogram_tester;

    EXPECT_FALSE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyModelNotAvailable, 1);
  }

  // Safety model info is valid but no config for feature, session not created
  // successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset([]() {
      auto safety_config = ComposeSafetyConfig();
      safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
      return safety_config;
    }());
    test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
    EXPECT_FALSE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

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

    FakeSafetyModelAsset safety_asset(ComposeSafetyConfig());
    test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
    EXPECT_TRUE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }

  // Safety model reset to not available, session no longer created
  // successfully.
  {
    base::HistogramTester histogram_tester;

    test_controller_->MaybeUpdateSafetyModel(std::nullopt);
    EXPECT_FALSE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyModelNotAvailable, 1);
    // No model. Shouldn't even record this histogram.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        0);
  }

  // Safety model reset to invalid, session no longer created successfully.
  {
    base::HistogramTester histogram_tester;

    std::unique_ptr<ModelInfo> model_info = TestModelInfoBuilder().Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);
    EXPECT_FALSE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyModelNotAvailable, 1);
    // No required model files. Shouldn't even record this histogram.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        0);
  }

  // Safety model info is valid and requires language but no language detection
  // model, session not created successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset([]() {
      auto safety_config = ComposeSafetyConfig();
      safety_config.add_allowed_languages("en");
      return safety_config;
    }());
    test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

    EXPECT_FALSE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

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

    FakeSafetyModelAsset safety_asset([]() {
      auto safety_config = ComposeSafetyConfig();
      safety_config.add_allowed_languages("en");
      return safety_config;
    }());
    test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
    test_controller_->SetLanguageDetectionModel(language_asset_.model_info());

    EXPECT_TRUE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

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

    test_controller_->MaybeUpdateSafetyModel(std::nullopt);
    EXPECT_TRUE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, SucceedsWithPassingSafetyChecks) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"), response_.callback());
  ASSERT_TRUE(response_.GetFinalStatus());
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: safe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingRequestSafetyChecks) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("unsafe_url"), response_.callback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: unsafe_url")
                          // Raw output check not done.
                          ));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FallbackWithInvalidRequestSafetyChecks) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  ExpectedRemoteFallback fallback;
  auto session = test_controller_->CreateSession(
      kFeature, fallback.CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"), response_.callback());

  auto fallback_call = fallback.Take();
  EXPECT_THAT(
      fallback_call.logged_executions(),
      ElementsAre(testing::_  // Base Model Execution
                              // Request check failed to run, not logged.
                  ));
  EXPECT_EQ(fallback_call.feature, ModelBasedCapabilityKey::kCompose);
  std::move(fallback_call.callback)
      .Run(base::ok(fallback.ComposeResponse("remote response")), nullptr);

  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "remote response");
  EXPECT_FALSE(response_.log_entry());
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingRawOutputSafetyChecks) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"unsafe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"), response_.callback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest, FallbackWithInvalidRawOutputChecks) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  ExpectedRemoteFallback fallback;
  auto session = test_controller_->CreateSession(
      kFeature, fallback.CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"), response_.callback());

  auto fallback_call = fallback.Take();
  EXPECT_THAT(fallback_call.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url")
                          // Raw output failed to run, not logged.
                          ));
  EXPECT_EQ(fallback_call.feature, ModelBasedCapabilityKey::kCompose);
  std::move(fallback_call.callback)
      .Run(base::ok(fallback.ComposeResponse("remote response")), nullptr);

  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "remote response");
  EXPECT_FALSE(response_.log_entry());
}

TEST_F(OnDeviceModelServiceControllerTest,
       SucceedsWithPassingResponseSafetyCheck) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_very_"), response_.callback());
  ASSERT_TRUE(response_.GetFinalStatus());
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_very_safe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingResponseSafetyCheck) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_un"), response_.callback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FallbackWithInvalidResponseSafetyCheck) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  ExpectedRemoteFallback fallback;
  auto session = test_controller_->CreateSession(
      kFeature, fallback.CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_very_"), response_.callback());

  auto fallback_call = fallback.Take();
  EXPECT_THAT(
      fallback_call.logged_executions(),
      ElementsAre(testing::_  // Base Model Execution
                              // response check failed to run, not logged.
                  ));
  EXPECT_EQ(fallback_call.feature, ModelBasedCapabilityKey::kCompose);
  std::move(fallback_call.callback)
      .Run(base::ok(fallback.ComposeResponse("remote response")), nullptr);

  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "remote response");
  EXPECT_FALSE(response_.log_entry());
}

TEST_F(OnDeviceModelServiceControllerTest, NoRetractUnsafeContent) {
  // Tests the behavior of "on_device_retract_unsafe_content = false" flag.
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

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
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  // Should fail the configured checks, but not not be retracted.
  fake_settings_.set_execute_result({"unsafe_output"});
  session->ExecuteModel(PageUrlRequest("unsafe_url"), response_.callback());
  ASSERT_TRUE(response_.GetFinalStatus());
  // Make sure T&S logged.
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: unsafe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionNoMinContext) {
  Initialize();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_min_tokens_for_context", "0"},
       {"on_device_model_max_tokens_for_context", "22"},
       {"on_device_model_context_token_chunk_size", "4"},
       {"on_device_model_topk", "1"},
       {"on_device_model_temperature", "0"}});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  session->AddContext(UserInputRequest("context"));
  task_environment_.RunUntilIdle();

  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx: off:0 max:4\n",
      "Context: cont off:4 max:4\n",
      "Context: ext off:8 max:4\n",
      "Input: execute:contextfoo\n",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, ReturnsErrorOnServiceDisconnect) {
  Initialize();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_fallback_to_server_on_disconnect", "false"}});
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();

  test_controller_->CrashService();
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
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
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();

  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
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
  ASSERT_FALSE(response_.log_entry());
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnExecute) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ResponseHolder resp1;
  ResponseHolder resp2;
  session->ExecuteModel(PageUrlRequest("foo"), resp1.callback());
  session->ExecuteModel(PageUrlRequest("bar"), resp2.callback());

  EXPECT_FALSE(resp1.GetFinalStatus());
  EXPECT_TRUE(resp2.GetFinalStatus());
  EXPECT_EQ(
      *resp1.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
  EXPECT_EQ(*resp2.value(), "Input: execute:bar\n");
}

TEST_F(OnDeviceModelServiceControllerTest, WontStartSessionAfterGpuBlocked) {
  Initialize();
  // Start a session.
  fake_settings_.set_load_model_result(LoadModelResult::kGpuBlocked);
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();

  {
    base::HistogramTester histogram_tester;

    // Because the model returned kGpuBlocked, no more sessions should start.
    EXPECT_FALSE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kGpuBlocked, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, DontRecreateSessionIfGpuBlocked) {
  Initialize();
  fake_settings_.set_load_model_result(LoadModelResult::kGpuBlocked);
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();
  test_controller_->clear_did_launch_service();

  // Adding context should not trigger launching the service again.
  session->AddContext(UserInputRequest("baz"));
  EXPECT_FALSE(test_controller_->did_launch_service());
}

TEST_F(OnDeviceModelServiceControllerTest, StopsConnectingAfterMultipleDrops) {
  Initialize();
  // Start a session.
  fake_settings_.set_drop_connection_request(true);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    auto session = test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt);
    EXPECT_TRUE(session) << i;
    task_environment_.RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    auto session = test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt);
    EXPECT_FALSE(session);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kTooManyRecentCrashes, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, AlternatingDisconnectSucceeds) {
  Initialize();
  // Start a session.
  for (int i = 0; i < 10; ++i) {
    fake_settings_.set_drop_connection_request(i % 2 == 1);
    auto session = test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt);
    EXPECT_TRUE(session) << i;
    task_environment_.RunUntilIdle();
  }
}

TEST_F(OnDeviceModelServiceControllerTest,
       MultipleDisconnectsThenVersionChangeRetries) {
  Initialize();
  // Create enough sessions that fail to trigger no longer creating a session.
  fake_settings_.set_drop_connection_request(true);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    auto session = test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt);
    EXPECT_TRUE(session) << i;
    task_environment_.RunUntilIdle();
  }
  EXPECT_FALSE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                               logger_.GetWeakPtr(), nullptr,
                                               /*config_params=*/std::nullopt));

  // Change the pref to a different value and recreate the service.
  access_controller_ = nullptr;
  test_controller_.reset();
  pref_service_.SetString(
      model_execution::prefs::localstate::kOnDeviceModelChromeVersion,
      "BOGUS VERSION");
  RecreateServiceController();
  // Wait until configuration is read.
  task_environment_.RunUntilIdle();

  // A new session should be started because the version changed.
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextDisconnectExecute) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();

  // Launch the service again, which triggers disconnect.
  test_controller_->CrashService();
  task_environment_.RunUntilIdle();

  // Send some text, ensuring the context is received.
  session->ExecuteModel(PageUrlRequest("baz"), response_.callback());
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kUsedOnDevice, 1);
  ASSERT_TRUE(response_.value());
  const std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:foo off:0 max:10\n",
      "Input: execute:foobaz\n",
  });
  EXPECT_EQ(*response_.value(), expected_responses[1]);
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
  EXPECT_EQ(response_.log_entry()
                ->log_ai_data_request()
                ->compose()
                .request()
                .page_metadata()
                .page_url(),
            "baz");
  EXPECT_EQ(response_.log_entry()
                ->log_ai_data_request()
                ->compose()
                .response()
                .output(),
            "Context: ctx:foo off:0 max:10\nInput: execute:foobaz\n");
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextExecuteDisconnect) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();
  // Send the text, this won't make it because the service is immediately
  // killed.
  session->ExecuteModel(PageUrlRequest("bar"), response_.callback());
  test_controller_->CrashService();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(response_.value());
  ASSERT_FALSE(response_.log_entry());
}

TEST_F(OnDeviceModelServiceControllerTest, ExecuteDisconnectedSession) {
  Initialize();
  auto session1 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session1);
  session1->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();

  // Start another session.
  auto session2 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session2);
  session2->AddContext(UserInputRequest("bar"));
  task_environment_.RunUntilIdle();

  session2->ExecuteModel(PageUrlRequest("2"), response_.callback());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(response_.value());
  const std::vector<std::string> expected_responses1 = {
      "Context: ctx:bar off:0 max:10\n",
      "Context: ctx:bar off:0 max:10\nInput: execute:bar2\n",
  };
  EXPECT_EQ(*response_.value(), expected_responses1[1]);
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses1));
  EXPECT_EQ(response_.log_entry()
                ->log_ai_data_request()
                ->compose()
                .request()
                .page_metadata()
                .page_url(),
            "2");
  EXPECT_EQ(response_.log_entry()
                ->log_ai_data_request()
                ->compose()
                .response()
                .output(),
            "Context: ctx:bar off:0 max:10\nInput: execute:bar2\n");

  ResponseHolder response2;
  session1->ExecuteModel(PageUrlRequest("1"), response2.callback());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(response2.value());
  const std::vector<std::string> expected_responses2 = {
      "Context: ctx:foo off:0 max:10\n",
      "Context: ctx:foo off:0 max:10\nInput: execute:foo1\n",
  };
  EXPECT_EQ(*response2.value(), expected_responses2[1]);
  EXPECT_THAT(response2.streamed(), ElementsAreArray(expected_responses2));
  EXPECT_EQ(response2.log_entry()
                ->log_ai_data_request()
                ->compose()
                .request()
                .page_metadata()
                .page_url(),
            "1");
  EXPECT_EQ(response2.log_entry()
                ->log_ai_data_request()
                ->compose()
                .response()
                .output(),
            "Context: ctx:foo off:0 max:10\nInput: execute:foo1\n");
}

TEST_F(OnDeviceModelServiceControllerTest, CallsRemoteExecute) {
  Initialize();
  fake_settings_.set_load_model_result(LoadModelResult::kGpuBlocked);
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();
  test_controller_->clear_did_launch_service();

  // Adding context should not trigger launching the service again.
  {
    base::HistogramTester histogram_tester;
    session->AddContext(UserInputRequest("baz"));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kUsingServer, 1);
  }
  session->ExecuteModel(PageUrlRequest("2"), response_.callback());
  EXPECT_TRUE(remote_execute_called_);
  EXPECT_FALSE(test_controller_->did_launch_service());
  // Did not start with on-device, so there should not have been a log entry
  // passed.
  ASSERT_FALSE(log_ai_data_request_passed_to_remote_);
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextInvalidConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  config.set_feature(ToModelExecutionFeatureProto(kFeature));
  Initialize({.config = config});

  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
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
    session->ExecuteModel(PageUrlRequest("2"), response_.callback());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
        ExecuteModelResult::kOnDeviceNotUsed, 1);
  }
  EXPECT_TRUE(remote_execute_called_);
  // The execute call never made it to on-device, so we shouldn't have created a
  // log entry.
  EXPECT_FALSE(log_ai_data_request_passed_to_remote_);
}

TEST_F(OnDeviceModelServiceControllerTest, ExecuteInvalidConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  config.set_feature(ToModelExecutionFeatureProto(kFeature));
  Initialize({.config = config});

  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  base::HistogramTester histogram_tester;
  session->ExecuteModel(PageUrlRequest("2"), response_.callback());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kFailedConstructingMessage, 1);
  EXPECT_TRUE(remote_execute_called_);
  // We never actually executed the request on-device so it is expected to not
  // have created a log entry.
  EXPECT_FALSE(log_ai_data_request_passed_to_remote_);
}

TEST_F(OnDeviceModelServiceControllerTest, FallbackToServerAfterDelay) {
  Initialize();
  fake_settings_.set_execute_delay(
      features::GetOnDeviceModelTimeForInitialResponse() * 2);

  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("2z"), response_.callback());
  base::HistogramTester histogram_tester;
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelTimeForInitialResponse() +
      base::Milliseconds(1));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kTimedOut, 1);
  EXPECT_TRUE(response_.streamed().empty());
  EXPECT_FALSE(response_.value());
  EXPECT_TRUE(remote_execute_called_);
  ASSERT_TRUE(last_remote_message_);
  auto& compose_request =
      static_cast<const proto::ComposeRequest&>(*last_remote_message_);
  ASSERT_TRUE(compose_request.has_page_metadata());
  EXPECT_EQ("2z", compose_request.page_metadata().page_url());
  ASSERT_TRUE(log_ai_data_request_passed_to_remote_);
  EXPECT_EQ(log_ai_data_request_passed_to_remote_->compose()
                .request()
                .page_metadata()
                .page_url(),
            "2z");
  EXPECT_FALSE(log_ai_data_request_passed_to_remote_->compose().has_response());
  EXPECT_FALSE(response_.provided_by_on_device().has_value());
}

TEST_F(OnDeviceModelServiceControllerTest,
       FallbackToServerOnDisconnectWhileWaitingForExecute) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();
  test_controller_->CrashService();
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kDisconnectAndMaybeFallback, 1);
  EXPECT_TRUE(remote_execute_called_);
  ASSERT_TRUE(log_ai_data_request_passed_to_remote_);
  EXPECT_EQ(log_ai_data_request_passed_to_remote_->compose()
                .request()
                .page_metadata()
                .page_url(),
            "foo");
  EXPECT_FALSE(log_ai_data_request_passed_to_remote_->compose().has_response());
}

TEST_F(OnDeviceModelServiceControllerTest,
       DestroySessionWhileWaitingForResponse) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
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
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  session.reset();
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());

  task_environment_.FastForwardBy(idle_timeout / 2 + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  // Should still be connected after half the idle time.
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());

  // Fast forward by the amount of time that triggers a disconnect.
  task_environment_.FastForwardBy(idle_timeout / 2 + base::Milliseconds(1));
  // As there are no sessions and no traffic for GetOnDeviceModelIdleTimeout()
  // the connection should be dropped.
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());
}

TEST_F(OnDeviceModelServiceControllerTest,
       ShutsDownServiceAfterPerformanceCheck) {
  Initialize();
  base::test::TestFuture<
      std::optional<on_device_model::mojom::PerformanceClass>>
      result_future;
  test_controller_->GetEstimatedPerformanceClass(result_future.GetCallback());
  EXPECT_EQ(on_device_model::mojom::PerformanceClass::kVeryHigh,
            *result_future.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(test_controller_->IsConnectedForTesting());
}

TEST_F(OnDeviceModelServiceControllerTest, UseServerWithRepeatedDelays) {
  Initialize();
  fake_settings_.set_execute_delay(
      features::GetOnDeviceModelTimeForInitialResponse() * 2);

  // Create a bunch of sessions that all timeout.
  for (int i = 0; i < features::GetOnDeviceModelTimeoutCountBeforeDisable();
       ++i) {
    auto session = test_controller_->CreateSession(
        kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt);
    ASSERT_TRUE(session);
    session->ExecuteModel(PageUrlRequest("2z"), response_.callback());
    task_environment_.FastForwardBy(
        features::GetOnDeviceModelTimeForInitialResponse() +
        base::Milliseconds(1));
    EXPECT_TRUE(response_.streamed().empty());
    EXPECT_FALSE(response_.value());
    EXPECT_TRUE(remote_execute_called_);
    remote_execute_called_ = false;
  }

  // As we reached GetOnDeviceModelTimeoutCountBeforeDisable() timeouts, the
  // next session should use the server.
  EXPECT_EQ(nullptr,
            test_controller_->CreateSession(kFeature, base::DoNothing(),
                                            logger_.GetWeakPtr(), nullptr,
                                            /*config_params=*/std::nullopt));
}

TEST_F(OnDeviceModelServiceControllerTest, RedactedField) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar");
  Initialize({.config = config});

  // `foo` doesn't match the redaction, so should be returned.
  auto session1 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session1);
  session1->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::string expected_response1 = "Input: execute:foo\n";
  EXPECT_EQ(*response_.value(), expected_response1);
  EXPECT_THAT(response_.streamed(), ElementsAre(expected_response1));

  // Input and output contain text matching redact, so should not be redacted.
  auto session2 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session2);
  ResponseHolder response2;
  session2->ExecuteModel(UserInputRequest("abarx"), response2.callback());
  task_environment_.RunUntilIdle();
  const std::string expected_response2 = "Input: execute:abarx\n";
  EXPECT_EQ(*response2.value(), expected_response2);
  EXPECT_THAT(response2.streamed(), ElementsAre(expected_response2));

  // Output contains redacted text (and  input doesn't), so redact.
  fake_settings_.set_execute_result({"Input: abarx\n"});
  auto session3 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session3);
  ResponseHolder response3;
  session3->ExecuteModel(UserInputRequest("foo"), response3.callback());
  task_environment_.RunUntilIdle();
  const std::string expected_response3 = "Input: a[###]x\n";
  EXPECT_EQ(*response3.value(), expected_response3);
  EXPECT_THAT(response3.streamed(), ElementsAre(expected_response3));
}

TEST_F(OnDeviceModelServiceControllerTest, RejectedField) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar", proto::RedactBehavior::REJECT);
  Initialize({.config = config});

  auto session1 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session1);
  session1->ExecuteModel(UserInputRequest("bar"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  // Although we send an error, we should be sending a log entry back so the
  // filtering can be logged.
  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_EQ(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
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
  Initialize({.config = config});

  // Force 'bar' to be returned from model.
  fake_settings_.set_execute_result({"Input: bar\n"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  session->ExecuteModel(RewriteRequest("bar"), response_.callback());
  task_environment_.RunUntilIdle();
  // `bar` shouldn't be rewritten as it's in the input.
  const std::string expected_response = "Input: bar\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_THAT(response_.streamed(), ElementsAre(expected_response));
}

TEST_F(OnDeviceModelServiceControllerTest, ReplacementText) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar", proto::REDACT_IF_ONLY_IN_OUTPUT, "[redacted]");
  Initialize({.config = config});

  // Output contains redacted text (and  input doesn't), so redact.
  fake_settings_.set_execute_result({"Input: abarx\n"});
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::string expected_response = "Input: a[redacted]x\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_THAT(response_.streamed(), ElementsAre(expected_response));
}

TEST_F(OnDeviceModelServiceControllerTest, DetectsRepeats) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "false"}});

  base::HistogramTester histogram_tester;
  proto::OnDeviceModelExecutionFeatureConfig config;
  Initialize();

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more repeating text",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.log_entry()
                  ->log_ai_data_request()
                  ->model_execution_info()
                  .on_device_model_execution_info()
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  Initialize();

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.log_entry()
                  ->log_ai_data_request()
                  ->model_execution_info()
                  .on_device_model_execution_info()
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  Initialize();

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating",
      " text",
      " some more ",
      "repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating",
      " text",
      " some more ",
      "repeating text",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.log_entry()
                  ->log_ai_data_request()
                  ->model_execution_info()
                  .on_device_model_execution_info()
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  Initialize();

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_FALSE(response_.log_entry()
                   ->log_ai_data_request()
                   ->model_execution_info()
                   .on_device_model_execution_info()
                   .execution_infos(0)
                   .response()
                   .on_device_model_service_response()
                   .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      false, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       UseRemoteTextSafetyFallbackButNoSafetyFallbackConfig) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTextSafetyRemoteFallback);

  base::HistogramTester histogram_tester;
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  Initialize({.config = config});

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_.streamed().empty());
  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(*response_.error(), OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kFailedConstructingRemoteTextSafetyRequest, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, UseRemoteTextSafetyFallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTextSafetyRemoteFallback);

  base::HistogramTester histogram_tester;
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_text_safety_fallback_config()
       ->mutable_input_url_proto_field() = UserInputField();
  Initialize({.config = config});

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });

  // Expect remote execute called for T&S.
  EXPECT_TRUE(remote_execute_called_);
  ASSERT_TRUE(last_remote_message_);
  auto& ts_request =
      static_cast<const proto::TextSafetyRequest&>(*last_remote_message_);
  EXPECT_EQ(expected_responses.back(), ts_request.text());
  EXPECT_EQ("foo", ts_request.url());
  ASSERT_TRUE(last_remote_ts_callback_);

  // Invoke T&S callback.
  proto::Any ts_any;
  auto remote_log_ai_data_request = std::make_unique<proto::LogAiDataRequest>();
  remote_log_ai_data_request->mutable_model_execution_info()->set_execution_id(
      "serverexecid");
  auto remote_log_entry = std::make_unique<ModelQualityLogEntry>(
      std::move(remote_log_ai_data_request),
      /*model_quality_uploader_service=*/nullptr);
  std::move(last_remote_ts_callback_)
      .Run(base::ok(ts_any), std::move(remote_log_entry));

  EXPECT_TRUE(response_.streamed().empty());
  EXPECT_EQ(*response_.value(), expected_responses.back());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kUsedOnDevice, 1);

  // Verify log entry.
  ASSERT_TRUE(response_.log_entry());
  // Should have 2 infos: one for text generation, one for safety fallback.
  EXPECT_EQ(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            2);
  auto& ts_exec_info = response_.log_entry()
                           ->log_ai_data_request()
                           ->model_execution_info()
                           .on_device_model_execution_info()
                           .execution_infos(1);
  auto& ts_req_log = ts_exec_info.request().text_safety_model_request();
  EXPECT_EQ(expected_responses.back(), ts_req_log.text());
  EXPECT_EQ("foo", ts_req_log.url());
  auto& ts_resp_log = ts_exec_info.response().text_safety_model_response();
  EXPECT_EQ("serverexecid", ts_resp_log.server_execution_id());
  EXPECT_FALSE(ts_resp_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest,
       UseRemoteTextSafetyFallbackFiltered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTextSafetyRemoteFallback);

  base::HistogramTester histogram_tester;
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  // Create an empty ts fallback config which is valid and will call the
  // fallback.
  config.mutable_text_safety_fallback_config();
  Initialize({.config = config});

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });

  // Expect remote execute called for T&S.
  EXPECT_TRUE(remote_execute_called_);
  ASSERT_TRUE(last_remote_message_);
  auto& ts_request =
      static_cast<const proto::TextSafetyRequest&>(*last_remote_message_);
  EXPECT_EQ(expected_responses.back(), ts_request.text());
  ASSERT_TRUE(last_remote_ts_callback_);

  // Invoke T&S callback.
  auto remote_log_ai_data_request = std::make_unique<proto::LogAiDataRequest>();
  remote_log_ai_data_request->mutable_model_execution_info()->set_execution_id(
      "serverexecid");
  auto remote_log_entry = std::make_unique<ModelQualityLogEntry>(
      std::move(remote_log_ai_data_request),
      /*model_quality_uploader_service=*/nullptr);
  std::move(last_remote_ts_callback_)
      .Run(base::unexpected(
               OptimizationGuideModelExecutionError::FromModelExecutionError(
                   OptimizationGuideModelExecutionError::ModelExecutionError::
                       kFiltered)),
           std::move(remote_log_entry));

  EXPECT_TRUE(response_.streamed().empty());
  EXPECT_FALSE(response_.value());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kUsedOnDeviceOutputUnsafe, 1);

  // Verify log entry.
  ASSERT_TRUE(response_.log_entry());
  // Should have 2 infos: one for text generation, one for safety fallback.
  EXPECT_EQ(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            2);
  auto& ts_exec_info = response_.log_entry()
                           ->log_ai_data_request()
                           ->model_execution_info()
                           .on_device_model_execution_info()
                           .execution_infos(1);
  auto& ts_req_log = ts_exec_info.request().text_safety_model_request();
  EXPECT_EQ(expected_responses.back(), ts_req_log.text());
  auto& ts_resp_log = ts_exec_info.response().text_safety_model_response();
  EXPECT_EQ("serverexecid", ts_resp_log.server_execution_id());
  EXPECT_TRUE(ts_resp_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest,
       UseRemoteTextSafetyFallbackOtherError) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTextSafetyRemoteFallback);

  base::HistogramTester histogram_tester;
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  // Create an empty ts fallback config which is valid and will call the
  // fallback.
  config.mutable_text_safety_fallback_config();
  Initialize({.config = config});

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });

  // Expect remote execute called for T&S.
  EXPECT_TRUE(remote_execute_called_);
  ASSERT_TRUE(last_remote_message_);
  auto& ts_request =
      static_cast<const proto::TextSafetyRequest&>(*last_remote_message_);
  EXPECT_EQ(expected_responses.back(), ts_request.text());
  ASSERT_TRUE(last_remote_ts_callback_);

  // Invoke T&S callback.
  std::move(last_remote_ts_callback_)
      .Run(base::unexpected(
               OptimizationGuideModelExecutionError::FromModelExecutionError(
                   OptimizationGuideModelExecutionError::ModelExecutionError::
                       kRequestThrottled)),
           nullptr);

  ASSERT_TRUE(response_.error());
  EXPECT_EQ(*response_.error(), OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kTextSafetyRemoteRequestFailed, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       UseRemoteTextSafetyFallbackNewRequestBeforeCallbackComesBack) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTextSafetyRemoteFallback);

  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  // Create an empty ts fallback config which is valid and will call the
  // fallback.
  config.mutable_text_safety_fallback_config();
  Initialize({.config = config});

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });

  // Expect remote execute called for T&S.
  EXPECT_TRUE(remote_execute_called_);
  ASSERT_TRUE(last_remote_message_);
  auto& ts_request =
      static_cast<const proto::TextSafetyRequest&>(*last_remote_message_);
  EXPECT_EQ(expected_responses.back(), ts_request.text());
  ASSERT_TRUE(last_remote_ts_callback_);

  {
    base::HistogramTester histogram_tester;

    session->ExecuteModel(UserInputRequest("newquery"), response_.callback());

    ASSERT_TRUE(response_.error());
    EXPECT_EQ(
        *response_.error(),
        OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
        ExecuteModelResult::kCancelled, 1);
  }

  {
    base::HistogramTester histogram_tester;
    // Invoke T&S callback and make sure nothing crashes.
    std::move(last_remote_ts_callback_)
        .Run(base::unexpected(
                 OptimizationGuideModelExecutionError::FromModelExecutionError(
                     OptimizationGuideModelExecutionError::ModelExecutionError::
                         kRequestThrottled)),
             nullptr);
    // Request should have been cancelled and we shouldn't receive anything
    // back.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
        0);
  }
}

TEST_F(OnDeviceModelServiceControllerTest,
       InitWithNoOnDeviceComponentStateManager) {
  access_controller_ = nullptr;
  test_controller_ = nullptr;

  auto access_controller =
      std::make_unique<OnDeviceModelAccessController>(pref_service_);
  access_controller_ = access_controller.get();
  test_controller_ = base::MakeRefCounted<FakeOnDeviceModelServiceController>(
      &fake_settings_, std::move(access_controller),
      on_device_component_state_manager_.get()->GetWeakPtr());

  on_device_component_state_manager_.Reset();
  // Init should not crash.
  test_controller_->Init();
}

TEST_F(OnDeviceModelServiceControllerTest, UsesAdapterTopKAndTemperature) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  config.mutable_sampling_params()->set_top_k(4);
  config.mutable_sampling_params()->set_temperature(1.5);
  Initialize({.config = config});

  auto session = test_controller_->CreateSession(kFeature, base::DoNothing(),
                                                 logger_.GetWeakPtr(), nullptr,
                                                 SessionConfigParams{});
  EXPECT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  const std::string expected_response =
      "Input: execute:foo\nTopK: 4, Temp: 1.5\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_THAT(response_.streamed(), ElementsAre(expected_response));
}

TEST_F(OnDeviceModelServiceControllerTest, UsesSessionTopKAndTemperature) {
  // Session sampling params should have precedence over feature ones.
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  config.mutable_sampling_params()->set_top_k(4);
  config.mutable_sampling_params()->set_temperature(1.5);
  Initialize({.config = config});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      SessionConfigParams{.sampling_params = SamplingParams{
                              .top_k = 3,
                              .temperature = 2,
                          }});
  EXPECT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  const std::string expected_response =
      "Input: execute:foo\nTopK: 3, Temp: 2\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_THAT(response_.streamed(), ElementsAre(expected_response));
}

// Validate that token interval 0 suppresses partial output.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval0) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {features::kOptimizationGuideOnDeviceModel,
           {{"on_device_model_retract_repeats", "false"}}},
          {features::kTextSafetyClassifier,
           {{"on_device_text_safety_token_interval", "0"}}},
      },
      {});
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"token1", " token2", " token3", " token4"});
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1 token2 token3 token4"};
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
}

// Validate that token interval 1 evaluates all partial output.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval1) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {features::kOptimizationGuideOnDeviceModel,
           {{"on_device_model_retract_repeats", "false"}}},
          {features::kTextSafetyClassifier,
           {{"on_device_text_safety_token_interval", "1"}}},
      },
      {});
  Initialize();
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"token1", " token2", " token3", " token4"});
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1",
      "token1 token2",
      "token1 token2 token3",
      "token1 token2 token3 token4",
  };
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
}

// Validate that token interval 3 only evaluates every third and final chunk.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval3) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {features::kOptimizationGuideOnDeviceModel,
           {{"on_device_model_retract_repeats", "false"}}},
          {features::kTextSafetyClassifier,
           {{"on_device_text_safety_token_interval", "3"}}},
      },
      {});
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result({"token1", " token2", " token3", " token4",
                                     " token5", " token6", " token7"});
  session->ExecuteModel(PageUrlRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1 token2 token3",
      "token1 token2 token3 token4 token5 token6",
      "token1 token2 token3 token4 token5 token6 token7",
  };
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.streamed(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, TestAvailabilityObserver) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kModelAdaptationCompose, {}},
       {features::internal::kOnDeviceModelTestFeature,
        {{"enable_adaptation", "false"}}}},
      {});

  auto config_compose = SimpleComposeConfig();
  config_compose.set_can_skip_text_safety(true);
  auto config_test = SimpleComposeConfig();
  config_test.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  config_test.set_can_skip_text_safety(true);

  Initialize({.config = config_compose,
              .config2 = config_test,
              .model_component_ready = false});

  FakeOnDeviceModelAvailabilityObserver availability_observer_compose(
      ModelBasedCapabilityKey::kCompose),
      availability_observer_test(ModelBasedCapabilityKey::kTest);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kCompose, &availability_observer_compose);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kTest, &availability_observer_test);

  on_device_component_state_manager_.get()->OnStartup();
  task_environment_.RunUntilIdle();
  on_device_component_state_manager_.SetReady(base_model_asset_.path());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_test.reason_);

  FakeAdaptationAsset adaptation_asset({
      .config = config_compose,
      .weight = 1015,
  });
  test_controller_->MaybeUpdateModelAdaptation(adaptation_asset.feature(),
                                               adaptation_asset.metadata());
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
        {{"on_device_retract_unsafe_content", "true"},
         {"on_device_text_safety_token_interval",
          base::NumberToString(GetParam())}}}},
      {});

  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " unsafe stuff not processed",
  });
  session->ExecuteModel(UserInputRequest("foo"), response_.callback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_.value());
  EXPECT_EQ(*response_.value(),
            "some text some more repeating text some more repeating text");

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.log_entry()
                  ->log_ai_data_request()
                  ->model_execution_info()
                  .on_device_model_execution_info()
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
  Initialize({.validation_config = WillPassValidationConfig()});
  task_environment_.RunUntilIdle();
  // Service should be immediately shut down.
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());

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
  proto::OnDeviceModelValidationConfig validation_config;

  base::HistogramTester histogram_tester;
  Initialize({.validation_config = validation_config});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                              logger_.GetWeakPtr(), nullptr,
                                              /*config_params=*/std::nullopt));

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
  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = WillFailValidationConfig()});
    task_environment_.RunUntilIdle();

    EXPECT_FALSE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kValidationFailed, 1);
  }

  {
    fake_settings_.set_execute_result({"goodbye"});
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  EXPECT_TRUE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                              logger_.GetWeakPtr(), nullptr,
                                              /*config_params=*/std::nullopt));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationBlocksSessionPendingCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "30s"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});
  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = WillPassValidationConfig()});
    task_environment_.RunUntilIdle();

    EXPECT_FALSE(test_controller_->CreateSession(
        kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
        /*config_params=*/std::nullopt));

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kValidationPending, 1);
  }

  {
    base::HistogramTester histogram_tester;
    task_environment_.FastForwardBy(base::Seconds(30) + base::Milliseconds(1));
    task_environment_.RunUntilIdle();
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  EXPECT_TRUE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                              logger_.GetWeakPtr(), nullptr,
                                              /*config_params=*/std::nullopt));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationNewModelVersion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});

  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = WillPassValidationConfig()});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  EXPECT_TRUE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                              logger_.GetWeakPtr(), nullptr,
                                              /*config_params=*/std::nullopt));
  // Kill the service since we are simulating a startup.
  test_controller_->CrashService();

  fake_settings_.set_execute_result({"goodbye"});
  {
    base::HistogramTester histogram_tester;

    on_device_component_state_manager_.get()->OnStartup();
    task_environment_.RunUntilIdle();
    on_device_component_state_manager_.SetReady(base_model_asset_.path(),
                                                "0.0.2");
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  EXPECT_FALSE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                               logger_.GetWeakPtr(), nullptr,
                                               /*config_params=*/std::nullopt));
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

  Initialize({.validation_config = WillPassValidationConfig()});
  task_environment_.RunUntilIdle();

  // Write an empty validation config and send a new model update.
  auto default_config = SimpleComposeConfig();
  default_config.set_can_skip_text_safety(true);
  base_model_asset_.Write(default_config);

  on_device_component_state_manager_.SetReady(base_model_asset_.path(),
                                              "0.0.2");
  task_environment_.RunUntilIdle();

  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));

  // Full validation should never run.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelValidationResultOnValidationStarted",
      OnDeviceModelValidationResult::kUnknown, 1);

  EXPECT_TRUE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                              logger_.GetWeakPtr(), nullptr,
                                              /*config_params=*/std::nullopt));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDoesNotRepeat) {
  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = WillPassValidationConfig()});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationRepeatsOnFailure) {
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  prompt->set_prompt("hello");
  prompt->set_expected_output("goodbye");

  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = validation_config});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  {
    fake_settings_.set_execute_result({"goodbye"});
    base::HistogramTester histogram_tester;
    RecreateServiceController();
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
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  prompt->set_prompt("hello");
  prompt->set_expected_output("goodbye");

  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = validation_config});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
  }

  // After a new version, we should re-check.
  pref_service_.SetString(
      model_execution::prefs::localstate::kOnDeviceModelChromeVersion,
      "OLD_VERSION");
  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kOnDeviceModelValidation);

  base::HistogramTester histogram_tester;
  Initialize({.validation_config = WillPassValidationConfig()});
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

  base::HistogramTester histogram_tester;
  Initialize({.validation_config = WillPassValidationConfig()});
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

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationInterrupted) {
  fake_settings_.set_execute_delay(base::Seconds(30));

  base::HistogramTester histogram_tester;
  Initialize({.validation_config = WillPassValidationConfig()});
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  EXPECT_TRUE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                              logger_.GetWeakPtr(), nullptr,
                                              /*config_params=*/std::nullopt));

  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kInterrupted, 1);

  // Session was created so the service should still be connected.
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());

  // After idle timeout, service should be killed.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationFails) {
  base::HistogramTester histogram_tester;
  Initialize({.validation_config = WillFailValidationConfig()});
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kNonMatchingOutput, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationFailsOnCrash) {
  fake_settings_.set_execute_delay(base::Seconds(10));

  base::HistogramTester histogram_tester;
  Initialize({.validation_config = WillPassValidationConfig()});
  task_environment_.RunUntilIdle();

  test_controller_->CrashService();
  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kServiceCrash, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       PerformanceCheckDoesNotInterruptModelValidation) {
  fake_settings_.set_execute_delay(base::Seconds(10));

  base::HistogramTester histogram_tester;
  Initialize({.validation_config = WillPassValidationConfig()});
  task_environment_.RunUntilIdle();

  base::test::TestFuture<
      std::optional<on_device_model::mojom::PerformanceClass>>
      result_future;
  test_controller_->GetEstimatedPerformanceClass(result_future.GetCallback());
  EXPECT_EQ(on_device_model::mojom::PerformanceClass::kVeryHigh,
            *result_future.Get());
  task_environment_.RunUntilIdle();

  // Performance check sh;ould not shut down service.
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationDoesNotInterruptPerformanceCheck) {
  fake_settings_.set_estimated_performance_delay(base::Seconds(10));
  fake_settings_.set_execute_delay(base::Seconds(1));

  base::HistogramTester histogram_tester;
  Initialize({.validation_config = WillPassValidationConfig()});
  task_environment_.RunUntilIdle();

  base::test::TestFuture<
      std::optional<on_device_model::mojom::PerformanceClass>>
      result_future;
  test_controller_->GetEstimatedPerformanceClass(result_future.GetCallback());

  task_environment_.FastForwardBy(base::Seconds(1) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  // Still connected since the performance estimator is running.
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);

  EXPECT_FALSE(result_future.IsReady());
  EXPECT_EQ(on_device_model::mojom::PerformanceClass::kVeryHigh,
            *result_future.Get());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());
}

}  // namespace optimization_guide
