// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <memory>
#include <optional>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_test_utils.h"
#include "components/optimization_guide/core/model_execution/test_on_device_model_component.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

using on_device_model::mojom::LoadModelResult;
using ExecuteModelResult = SessionImpl::ExecuteModelResult;

namespace {

constexpr int64_t kModelAdatationVersion = 1;

// Sets a threshold that will rejct text containing "unsafe"  when used with
// FakeOnDeviceModel::ClassifyTextSafety..
proto::SafetyCategoryThreshold ForbidUnsafe() {
  proto::SafetyCategoryThreshold result;
  result.set_output_index(0);  // FakeOnDeviceModel's "SAFETY" category.
  result.set_threshold(0.5);
  return result;
}

// Sets a threshold that will reject text without "reasonable" when used with
// FakeOnDeviceModel::ClassifyTextSafety.
proto::SafetyCategoryThreshold RequireReasonable() {
  proto::SafetyCategoryThreshold result;
  result.set_output_index(1);  // FakeOnDeviceModel's "REASONABLE" category.
  result.set_threshold(0.5);
  return result;
}

class FakeOnDeviceModelAvailabilityObserver
    : public OnDeviceModelAvailabilityObserver {
 public:
  explicit FakeOnDeviceModelAvailabilityObserver(
      ModelBasedCapabilityKey expected_feature) {
    expected_feature_ = expected_feature;
  }

  void OnDeviceModelAvailablityChanged(
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

class OnDeviceModelServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
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
        base::to_underlying(OnDeviceModelPerformanceClass::kLow));
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
      WriteFeatureConfig(*params.config, params.config2,
                         params.validation_config);
    } else {
      proto::OnDeviceModelExecutionFeatureConfig default_config;
      default_config.set_can_skip_text_safety(true);
      PopulateConfigForFeature(kFeature, default_config);
      WriteFeatureConfig(default_config, std::nullopt,
                         params.validation_config);
    }

    if (params.model_component_ready) {
      on_device_component_state_manager_.get()->OnStartup();
      task_environment_.FastForwardBy(base::Seconds(1));
      on_device_component_state_manager_.SetReady(temp_dir());
    }

    RecreateServiceController();
    // Wait until the OnDeviceModelExecutionConfig has been read.
    task_environment_.RunUntilIdle();
  }

  ExecuteRemoteFn CreateExecuteRemoteFn() {
    return base::BindLambdaForTesting(
        [=](ModelBasedCapabilityKey feature,
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

  void SetFeatureTextSafetyConfiguration(
      std::unique_ptr<proto::FeatureTextSafetyConfiguration> feature_config) {
    feature_config->set_feature(ToModelExecutionFeatureProto(kFeature));
    proto::TextSafetyModelMetadata model_metadata;
    model_metadata.mutable_feature_text_safety_configurations()->AddAllocated(
        feature_config.release());
    proto::Any any;
    any.set_type_url(
        "type.googleapis.com/optimization_guide.proto.TextSafetyModelMetadata");
    model_metadata.SerializeToString(any.mutable_value());
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(
                {temp_dir().Append(kTsDataFile),
                 temp_dir().Append(base::FilePath(kTsSpModelFile))})
            .SetModelMetadata(any)
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);
  }

  // Add a substitution for ComposeRequest::page_metadata.page_url
  void AddPageUrlSubstitution(proto::SubstitutedString* substitution) {
    auto* proto_field2 = substitution->add_substitutions()
                             ->add_candidates()
                             ->mutable_proto_field();
    proto_field2->add_proto_descriptors()->set_tag_number(3);
    proto_field2->add_proto_descriptors()->set_tag_number(1);
  }

  // Add a substitution for StringValue::value
  void AddStringValueSubstitution(proto::SubstitutedString* substitution) {
    auto* proto_field2 = substitution->add_substitutions()
                             ->add_candidates()
                             ->mutable_proto_field();
    proto_field2->add_proto_descriptors()->set_tag_number(1);
  }

  void PopulateConfigForFeature(
      ModelBasedCapabilityKey feature,
      proto::OnDeviceModelExecutionFeatureConfig& config) {
    config.set_feature(ToModelExecutionFeatureProto(feature));
    auto& input_config = *config.mutable_input_config();
    input_config.set_request_base_name(proto::ComposeRequest().GetTypeName());

    // Execute call prefixes with execute:.
    auto& substitution = *input_config.add_execute_substitutions();
    substitution.set_string_template("execute:%s%s");
    auto* proto_field1 = substitution.add_substitutions()
                             ->add_candidates()
                             ->mutable_proto_field();
    proto_field1->add_proto_descriptors()->set_tag_number(7);
    proto_field1->add_proto_descriptors()->set_tag_number(1);
    auto* proto_field2 = substitution.add_substitutions()
                             ->add_candidates()
                             ->mutable_proto_field();
    proto_field2->add_proto_descriptors()->set_tag_number(3);
    proto_field2->add_proto_descriptors()->set_tag_number(1);

    // Context call prefixes with context:.
    auto& context_substitution =
        *input_config.add_input_context_substitutions();
    context_substitution.set_string_template("ctx:%s");
    auto* context_proto_field = context_substitution.add_substitutions()
                                    ->add_candidates()
                                    ->mutable_proto_field();
    context_proto_field->add_proto_descriptors()->set_tag_number(7);
    context_proto_field->add_proto_descriptors()->set_tag_number(1);

    auto& output_config = *config.mutable_output_config();
    output_config.set_proto_type(proto::ComposeResponse().GetTypeName());
    output_config.mutable_proto_field()
        ->add_proto_descriptors()
        ->set_tag_number(1);
  }

  proto::RedactRule& PopulateConfigForFeatureWithRedactRule(
      proto::OnDeviceModelExecutionFeatureConfig& config,
      const std::string& regex,
      proto::RedactBehavior behavior =
          proto::RedactBehavior::REDACT_IF_ONLY_IN_OUTPUT) {
    PopulateConfigForFeature(kFeature, config);
    auto& output_config = *config.mutable_output_config();
    auto& redact_rules = *output_config.mutable_redact_rules();
    auto& field = *redact_rules.add_fields_to_check();
    field.add_proto_descriptors()->set_tag_number(7);
    field.add_proto_descriptors()->set_tag_number(1);
    auto& redact_rule = *redact_rules.add_rules();
    redact_rule.set_regex(regex);
    redact_rule.set_behavior(behavior);
    return redact_rule;
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

  void WriteExecutionConfig(const proto::OnDeviceModelExecutionConfig& config) {
    CHECK(base::WriteFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                          config.SerializeAsString()));
  }

  void WriteFeatureConfig(
      const proto::OnDeviceModelExecutionFeatureConfig& config,
      std::optional<proto::OnDeviceModelExecutionFeatureConfig> config2 =
          std::nullopt,
      std::optional<proto::OnDeviceModelValidationConfig> validation_config =
          std::nullopt) {
    proto::OnDeviceModelExecutionConfig execution_config;
    *execution_config.add_feature_configs() = config;
    if (config2) {
      *execution_config.add_feature_configs() = *config2;
    }
    if (validation_config) {
      *execution_config.mutable_validation_config() = *validation_config;
    }
    WriteExecutionConfig(execution_config);
  }

  void AddContext(OptimizationGuideModelExecutor::Session& session,
                  std::string_view input) {
    proto::ComposeRequest request;
    request.mutable_generate_params()->set_user_input(std::string(input));
    session.AddContext(request);
  }

  // Calls Execute() after setting `input` as the page-url.
  void ExecuteModel(OptimizationGuideModelExecutor::Session& session,
                    std::string_view input) {
    proto::ComposeRequest request;
    request.mutable_page_metadata()->set_page_url(std::string(input));
    session.ExecuteModel(
        request,
        base::BindRepeating(&OnDeviceModelServiceControllerTest::OnResponse,
                            base::Unretained(this)));
  }

  // Calls Execute() after setting `input` as the user_input.
  void ExecuteModelUsingInput(OptimizationGuideModelExecutor::Session& session,
                              std::string_view input) {
    proto::ComposeRequest request;
    request.mutable_generate_params()->set_user_input(std::string(input));
    session.ExecuteModel(
        request,
        base::BindRepeating(&OnDeviceModelServiceControllerTest::OnResponse,
                            base::Unretained(this)));
  }

  void ExecuteModelWithRewrite(
      OptimizationGuideModelExecutor::Session& session) {
    proto::ComposeRequest request;
    auto& rewrite_params = *request.mutable_rewrite_params();
    rewrite_params.set_previous_response("bar");
    rewrite_params.set_tone(proto::COMPOSE_FORMAL);
    session.ExecuteModel(
        request,
        base::BindRepeating(&OnDeviceModelServiceControllerTest::OnResponse,
                            base::Unretained(this)));
  }

  std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationController>&
  GetModelAdaptationControllers() const {
    return test_controller_->model_adaptation_controllers_;
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 protected:
  void OnResponse(OptimizationGuideModelStreamingExecutionResult result) {
    log_entry_received_ = std::move(result.log_entry);
    if (log_entry_received_) {
      // Make sure that an execution ID is always generated if we return a log
      // entry.
      ASSERT_FALSE(log_entry_received_->log_ai_data_request()
                       ->model_execution_info()
                       .execution_id()
                       .empty());
      EXPECT_TRUE(base::StartsWith(log_entry_received_->log_ai_data_request()
                                       ->model_execution_info()
                                       .execution_id(),
                                   "on-device"));
    }
    if (!result.response.has_value()) {
      response_error_ = result.response.error().error();
      return;
    }
    provided_by_on_device_ = result.provided_by_on_device;
    auto response =
        ParsedAnyMetadata<proto::ComposeResponse>(result.response->response);
    if (result.response->is_complete) {
      response_received_ = response->output();
    } else {
      streamed_responses_.push_back(response->output());
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple pref_service_;
  FakeOnDeviceServiceSettings fake_settings_;
  TestOnDeviceModelComponentStateManager on_device_component_state_manager_{
      &pref_service_};
  scoped_refptr<FakeOnDeviceModelServiceController> test_controller_;
  // Owned by FakeOnDeviceModelServiceController.
  raw_ptr<OnDeviceModelAccessController> access_controller_ = nullptr;
  std::vector<std::string> streamed_responses_;
  std::optional<std::string> response_received_;
  std::optional<bool> provided_by_on_device_;
  std::unique_ptr<ModelQualityLogEntry> log_entry_received_;
  std::optional<OptimizationGuideModelExecutionError::ModelExecutionError>
      response_error_;
  base::test::ScopedFeatureList feature_list_;
  bool remote_execute_called_ = false;
  std::unique_ptr<google::protobuf::MessageLite> last_remote_message_;
  std::unique_ptr<proto::LogAiDataRequest>
      log_ai_data_request_passed_to_remote_;
  OptimizationGuideModelExecutionResultCallback last_remote_ts_callback_;
  OptimizationGuideLogger logger_;
};

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionSuccess) {
  Initialize();

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  const std::string expected_response = "Input: execute:foo\n";
  EXPECT_EQ(*response_received_, expected_response);
  EXPECT_TRUE(*provided_by_on_device_);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response));
  EXPECT_TRUE(log_entry_received_);
  const auto logged_on_device_model_execution_info =
      log_entry_received_->log_ai_data_request()
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

  proto::OnDeviceModelExecutionFeatureConfig config_compose, config_test;
  config_compose.set_can_skip_text_safety(true);
  config_test.set_can_skip_text_safety(true);
  PopulateConfigForFeature(ModelBasedCapabilityKey::kCompose, config_compose);
  PopulateConfigForFeature(ModelBasedCapabilityKey::kTest, config_test);

  Initialize({.config = config_compose, .config2 = config_test});

  FakeOnDeviceModelAvailabilityObserver availability_observer_compose(
      ModelBasedCapabilityKey::kCompose),
      availability_observer_test(ModelBasedCapabilityKey::kTest);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kCompose, &availability_observer_compose);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kTest, &availability_observer_test);

  test_controller_->MaybeUpdateModelAdaptation(
      ModelBasedCapabilityKey::kCompose,
      OnDeviceModelAdaptationMetadata::New(
          on_device_model::AdaptationAssetPaths(), kModelAdatationVersion,
          /*adapter=*/nullptr));
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_compose.reason_);
  EXPECT_FALSE(availability_observer_test.reason_);

  test_controller_->MaybeUpdateModelAdaptation(
      ModelBasedCapabilityKey::kTest,
      OnDeviceModelAdaptationMetadata::New(
          on_device_model::AdaptationAssetPaths(), kModelAdatationVersion,
          /*adapter=*/nullptr));
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_test.reason_);

  auto session_compose = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  task_environment_.RunUntilIdle();
  auto session_test = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, base::DoNothing(), logger_.GetWeakPtr(),
      nullptr,
      /*config_params=*/std::nullopt);

  EXPECT_EQ(2u, GetModelAdaptationControllers().size());

  ExecuteModel(*session_compose, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_EQ(*response_received_, "Adaptation model: 1\nInput: execute:foo\n");
  EXPECT_TRUE(*provided_by_on_device_);

  ExecuteModel(*session_test, "bar");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_EQ(*response_received_, "Adaptation model: 2\nInput: execute:bar\n");
  EXPECT_TRUE(*provided_by_on_device_);

  EXPECT_TRUE(log_entry_received_);
  const auto logged_on_device_model_execution_info =
      log_entry_received_->log_ai_data_request()
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
  EXPECT_EQ(model_version.model_adaptation_version(), 1);

  session_compose.reset();
  session_test.reset();

  // Fast forward by the amount of time that triggers an idle disconnect. The
  // model adaptations will be reset. But the base model remote will still be
  // connected.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  EXPECT_TRUE(GetModelAdaptationControllers().empty());
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());

  // Fast forward by another idle timeout. The base model remote will be reset.
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
    AddContext(*session, "foo");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kUsingOnDevice, 1);
  }
  task_environment_.RunUntilIdle();

  AddContext(*session, "bar");
  ExecuteModel(*session, "baz");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  const std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:bar off:0 max:10\n",
      "Input: execute:barbaz\n",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelExecutionLoadsSingleContextChunk) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  AddContext(*session, "context");
  task_environment_.RunUntilIdle();

  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:contex off:0 max:10\n",
      "Context: t off:10 max:4\n",
      "Input: execute:contextfoo\n",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelExecutionLoadsLongContextInChunks) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  AddContext(*session, "this is long context");
  task_environment_.RunUntilIdle();

  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:this i off:0 max:10\n",
      "Context: s lo off:10 max:4\n",
      "Context: ng c off:14 max:4\n",
      "Context: onte off:18 max:4\n",
      "Input: execute:this is long contextfoo\n",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelExecutionCancelsOptionalContext) {
  Initialize();
  fake_settings_.set_execute_delay(base::Seconds(10));
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  AddContext(*session, "this is long context");
  // ExecuteModel() directly after AddContext() should only load first chunk.
  ExecuteModel(*session, "foo");

  // Give time to make sure we don't process the optional context.
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_received_);
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:this i off:0 max:10\n",
      "Input: execute:this is long contextfoo\n",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionModelNotAvailable) {
  Initialize({.model_component_ready = false});

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_FALSE(session);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kModelNotAvailable, 1);
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
  on_device_component_state_manager_.SetReady(temp_dir());
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
  WriteExecutionConfig({});
  on_device_component_state_manager_.SetReady(temp_dir());
  task_environment_.RunUntilIdle();

  // Verify the existing session still works.
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(response_received_);
  const std::string expected_response = "Input: execute:foo\n";
  EXPECT_EQ(*response_received_, expected_response);
  EXPECT_TRUE(*provided_by_on_device_);
}

TEST_F(OnDeviceModelServiceControllerTest, SessionBeforeAndAfterModelUpdate) {
  Initialize();

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  AddContext(*session, "context");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1ull, test_controller_->on_device_model_receiver_count());

  // Simulates a model update. This should close the model remote.
  // Write a new empty execution config to check that the config is reloaded.
  WriteExecutionConfig({});
  on_device_component_state_manager_.SetReady(temp_dir());
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
  Initialize();

  // Safety model info is valid but no metadata.
  {
    base::HistogramTester histogram_tester;

    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(
                {temp_dir().Append(kTsDataFile),
                 temp_dir().Append(base::FilePath(kTsSpModelFile))})
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
            .SetAdditionalFiles(
                {temp_dir().Append(kTsDataFile),
                 temp_dir().Append(base::FilePath(kTsSpModelFile))})
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
            .SetAdditionalFiles(
                {temp_dir().Append(kTsDataFile),
                 temp_dir().Append(base::FilePath(kTsSpModelFile))})
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
            .SetAdditionalFiles(
                {temp_dir().Append(kTsDataFile),
                 temp_dir().Append(base::FilePath(kTsSpModelFile))})
            .SetModelMetadata(any)
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, SessionRequiresSafetyModel) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
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

    proto::TextSafetyModelMetadata model_metadata;
    model_metadata.add_feature_text_safety_configurations()->set_feature(
        proto::MODEL_EXECUTION_FEATURE_TEST);
    proto::Any any;
    any.set_type_url(
        "type.googleapis.com/optimization_guide.proto.TextSafetyModelMetadata");
    model_metadata.SerializeToString(any.mutable_value());
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(
                {temp_dir().Append(kTsDataFile),
                 temp_dir().Append(base::FilePath(kTsSpModelFile))})
            .SetModelMetadata(any)
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);
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

    proto::TextSafetyModelMetadata model_metadata;
    model_metadata.add_feature_text_safety_configurations()->set_feature(
        ToModelExecutionFeatureProto(kFeature));
    proto::Any any;
    any.set_type_url(
        "type.googleapis.com/optimization_guide.proto.TextSafetyModelMetadata");
    model_metadata.SerializeToString(any.mutable_value());
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(
                {temp_dir().Append(kTsDataFile),
                 temp_dir().Append(base::FilePath(kTsSpModelFile))})
            .SetModelMetadata(any)
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);
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

    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetModelFilePath(temp_dir().Append(FILE_PATH_LITERAL("garbage")))
            .Build();
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

    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("en");
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

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

    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("en");
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);

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

TEST(SafetyConfigTest, MissingScoreIsUnsafe) {
  auto safety_config =
      std::make_unique<proto::FeatureTextSafetyConfiguration>();
  safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
  auto* threshold = safety_config->add_safety_category_thresholds();
  threshold->set_output_index(1);
  threshold->set_threshold(0.5);
  SafetyConfig cfg(*safety_config);

  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->class_scores = {0.1};  // Only 1 score, but expects 2.
  EXPECT_TRUE(cfg.IsUnsafeText(safety_info));
}

TEST(SafetyConfigTest, SafeWithRequiredScores) {
  auto safety_config =
      std::make_unique<proto::FeatureTextSafetyConfiguration>();
  safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
  auto* threshold = safety_config->add_safety_category_thresholds();
  threshold->set_output_index(1);
  threshold->set_threshold(0.5);
  SafetyConfig cfg(*safety_config);

  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->class_scores = {0.1, 0.1};  // Has score with index = 1.
  EXPECT_FALSE(cfg.IsUnsafeText(safety_info));
}

TEST_F(OnDeviceModelServiceControllerTest, DefaultOutputSafetyPasses) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  // Should fail the default raw output check.
  fake_settings_.set_execute_result({"unsafe_output"});
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_received_);
  ASSERT_TRUE(response_error_);
  EXPECT_EQ(
      *response_error_,
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  // Make sure T&S logged.
  ASSERT_TRUE(log_entry_received_);
  const auto logged_on_device_model_execution_info =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info();
  const auto num_execution_infos =
      logged_on_device_model_execution_info.execution_infos_size();
  EXPECT_GE(num_execution_infos, 2);
  auto ts_log = logged_on_device_model_execution_info.execution_infos(
      num_execution_infos - 1);
  EXPECT_EQ(ts_log.request().text_safety_model_request().text(),
            "unsafe_output");
  EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
              ElementsAre(0.8, 0.8));
  EXPECT_TRUE(ts_log.response().text_safety_model_response().is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, DefaultOutputSafetyFails) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result({"reasonable_output"});
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  // Make sure T&S logged.
  ASSERT_TRUE(log_entry_received_);
  const auto logged_on_device_model_execution_info =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info();
  const auto num_execution_infos =
      logged_on_device_model_execution_info.execution_infos_size();
  EXPECT_GE(num_execution_infos, 2);
  auto ts_log = logged_on_device_model_execution_info.execution_infos(
      num_execution_infos - 1);
  EXPECT_EQ(ts_log.request().text_safety_model_request().text(),
            "reasonable_output");
  EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
              ElementsAre(0.2, 0.2));
  EXPECT_FALSE(ts_log.response().text_safety_model_response().is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, SafetyModelUsedButNoRetract) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "false"}});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  // Should fail the configured checks, but not not be retracted.
  fake_settings_.set_execute_result({"unsafe_output"});

  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);

  // Make sure T&S logged.
  ASSERT_TRUE(log_entry_received_);
  const auto logged_on_device_model_execution_info =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info();
  EXPECT_GE(logged_on_device_model_execution_info.execution_infos_size(), 2);
  auto ts_log = logged_on_device_model_execution_info.execution_infos(
      logged_on_device_model_execution_info.execution_infos_size() - 1);
  EXPECT_EQ(ts_log.request().text_safety_model_request().text(),
            "unsafe_output");
  EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
              ElementsAre(0.8, 0.8));
  EXPECT_TRUE(ts_log.response().text_safety_model_response().is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, RequestCheckPassesWithSafeUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "safe_url");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);

  // Make sure check was logged.
  ASSERT_TRUE(log_entry_received_);
  const auto& logged_execution_infos =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
  ASSERT_GE(logged_execution_infos.size(), 2);
  const auto& check_log = logged_execution_infos[1];
  EXPECT_EQ(check_log.request().text_safety_model_request().text(),
            "url: safe_url");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.2, 0.8));
  EXPECT_FALSE(response_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, RequestCheckFailsWithUnsafeUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "unsafe_url");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_received_);
  EXPECT_TRUE(response_error_);

  // Make sure check was logged.
  ASSERT_TRUE(log_entry_received_);
  const auto& logged_execution_infos =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
  ASSERT_EQ(logged_execution_infos.size(), 2);
  const auto& check_log = logged_execution_infos[1];
  EXPECT_EQ(check_log.request().text_safety_model_request().text(),
            "url: unsafe_url");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.8, 0.8));
  EXPECT_TRUE(response_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, RequestCheckIgnoredInDarkMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "false"}});
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "unsafe_url");
  task_environment_.RunUntilIdle();
  // Should still succeed, because on_device_retract_unsafe_content is false.
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);

  // Make sure check was logged.
  ASSERT_TRUE(log_entry_received_);
  const auto& logged_execution_infos =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
  ASSERT_GE(logged_execution_infos.size(), 2);
  const auto& check_log = logged_execution_infos[1];
  EXPECT_EQ(check_log.request().text_safety_model_request().text(),
            "url: unsafe_url");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.8, 0.8));
  EXPECT_TRUE(response_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest,
       RequestCheckFailsWithSafeUrlWithFallbackThreshold) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    // Omitted check thresholds, should fallback to default.
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "safe_url");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_received_);
  EXPECT_TRUE(response_error_);

  // Make sure check was logged.
  ASSERT_TRUE(log_entry_received_);
  const auto& logged_execution_infos =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
  ASSERT_EQ(logged_execution_infos.size(), 2);
  const auto& check_log = logged_execution_infos[1];
  EXPECT_EQ(check_log.request().text_safety_model_request().text(),
            "url: safe_url");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.2, 0.8));
  EXPECT_TRUE(response_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest,
       RequestCheckFailsWithUnmetRequiredLanguage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "safe_url");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_received_);
  EXPECT_TRUE(response_error_);
}

TEST_F(OnDeviceModelServiceControllerTest,
       RequestCheckFailsWithUnmetRequiredLanguageButIgnored) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  Initialize();

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    check->set_ignore_language_result(true);
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "safe_url");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);
}

TEST_F(OnDeviceModelServiceControllerTest,
       RequestCheckPassesWithMetRequiredLanguage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  Initialize();

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "safe_url in esperanto");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);
}

TEST_F(OnDeviceModelServiceControllerTest,
       RequestCheckPassesWithLanguageOnlyFilter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  Initialize();

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    check->set_check_language_only(true);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "unsafe_url in esperanto");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);
}

TEST_F(OnDeviceModelServiceControllerTest,
       RequestCheckFailsWithLanguageOnlyFilter) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    check->set_check_language_only(true);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  // This should pass the default raw output safety check
  fake_settings_.set_execute_result(
      {"reasonable but unsafe output in esperanto"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "safe_url in english");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_received_);
  EXPECT_TRUE(response_error_);

  // Make sure check was logged.
  ASSERT_TRUE(log_entry_received_);
  const auto& logged_execution_infos =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
  ASSERT_EQ(logged_execution_infos.size(), 2);
  const auto& check_log = logged_execution_infos[1];
  EXPECT_EQ(check_log.request().text_safety_model_request().text(),
            "url: safe_url in english");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_FALSE(response_log.is_unsafe());
  EXPECT_EQ(response_log.language_code(), "");
  EXPECT_EQ(response_log.language_confidence(), 0.0);
}

TEST_F(OnDeviceModelServiceControllerTest,
       RawOutputCheckPassesWithMetRequiredLanguage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->mutable_raw_output_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("safe_text in esperanto: %s");
    AddStringValueSubstitution(input_template);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  // This should be used in the raw output check.
  fake_settings_.set_execute_result({"reasonable_output"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "some_url");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);

  // Make sure check was logged.
  ASSERT_TRUE(log_entry_received_);
  const auto& logged_execution_infos =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
  ASSERT_EQ(logged_execution_infos.size(), 2);
  const auto& check_log = logged_execution_infos[1];
  EXPECT_EQ(check_log.request().text_safety_model_request().text(),
            "safe_text in esperanto: reasonable_output");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.2, 0.2));
  EXPECT_FALSE(response_log.is_unsafe());
  EXPECT_EQ(response_log.language_code(), "eo");
  EXPECT_EQ(response_log.language_confidence(), 1.0);
}

TEST_F(OnDeviceModelServiceControllerTest, RawOutputCheckFailsWithUnsafeText) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    auto* check = safety_config->mutable_raw_output_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("unsafe_text in esperanto: %s");
    AddStringValueSubstitution(input_template);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  // This should be used in the raw output check.
  fake_settings_.set_execute_result({"reasonable_output"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "some_url");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_received_);
  EXPECT_TRUE(response_error_);

  // Make sure check was logged.
  ASSERT_TRUE(log_entry_received_);
  const auto& logged_execution_infos =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
  ASSERT_EQ(logged_execution_infos.size(), 2);
  const auto& check_log = logged_execution_infos[1];
  EXPECT_EQ(check_log.request().text_safety_model_request().text(),
            "unsafe_text in esperanto: reasonable_output");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.8, 0.2));
  EXPECT_TRUE(response_log.is_unsafe());
  EXPECT_EQ(response_log.language_code(), "eo");
  EXPECT_EQ(response_log.language_confidence(), 1.0);
}

TEST_F(OnDeviceModelServiceControllerTest,
       RawOutputCheckFailsWithSafeTextInUndeterminedLanguage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    auto* check = safety_config->mutable_raw_output_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("safe_text in unknown language: %s");
    AddStringValueSubstitution(input_template);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  // This should be used in the raw output check.
  fake_settings_.set_execute_result({"reasonable_output"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ExecuteModel(*session, "some_url");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_received_);
  EXPECT_TRUE(response_error_);

  // Make sure check was logged.
  ASSERT_TRUE(log_entry_received_);
  const auto& logged_execution_infos =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
  ASSERT_EQ(logged_execution_infos.size(), 2);
  const auto& check_log = logged_execution_infos[1];
  EXPECT_EQ(check_log.request().text_safety_model_request().text(),
            "safe_text in unknown language: reasonable_output");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.2, 0.2));
  EXPECT_FALSE(response_log.is_unsafe());
  EXPECT_EQ(response_log.language_code(), "");
  EXPECT_EQ(response_log.language_confidence(), 0.0);
}

TEST_F(OnDeviceModelServiceControllerTest, SafetyModelDarkMode) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "false"}});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config->mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));

    std::unique_ptr<optimization_guide::ModelInfo> ld_model_info =
        TestModelInfoBuilder().SetVersion(123).Build();
    test_controller_->SetLanguageDetectionModel(*ld_model_info);
  }

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  // Should fail raw output, but not retract.
  fake_settings_.set_execute_result({"unsafe_output"});
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);

  // Make sure T&S logged.
  ASSERT_TRUE(log_entry_received_);
  const auto logged_on_device_model_execution_info =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info();
  EXPECT_GE(logged_on_device_model_execution_info.execution_infos_size(), 2);
  auto ts_log = logged_on_device_model_execution_info.execution_infos(
      logged_on_device_model_execution_info.execution_infos_size() - 1);
  EXPECT_EQ(ts_log.request().text_safety_model_request().text(),
            "unsafe_output");
  EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
              ElementsAre(0.8, 0.8));
  EXPECT_TRUE(ts_log.response().text_safety_model_response().is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, SafetyModelDarkModeNoFeatureConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(true);
  Initialize({.config = config});

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "false"}});

  proto::TextSafetyModelMetadata model_metadata;
  auto* other_feature_safety_config =
      model_metadata.add_feature_text_safety_configurations();
  other_feature_safety_config->set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  other_feature_safety_config->mutable_safety_category_thresholds()->Add(
      ForbidUnsafe());
  other_feature_safety_config->mutable_safety_category_thresholds()->Add(
      RequireReasonable());
  proto::Any any;
  any.set_type_url(
      "type.googleapis.com/optimization_guide.proto.TextSafetyModelMetadata");
  model_metadata.SerializeToString(any.mutable_value());
  std::unique_ptr<optimization_guide::ModelInfo> model_info =
      TestModelInfoBuilder()
          .SetAdditionalFiles(
              {temp_dir().Append(kTsDataFile),
               temp_dir().Append(base::FilePath(kTsSpModelFile))})
          .SetModelMetadata(any)
          .Build();
  test_controller_->MaybeUpdateSafetyModel(*model_info);
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  // Would fail other feature's raw output check, but it shouldn't run.
  fake_settings_.set_execute_result({"unsafe_output"});

  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_FALSE(response_error_);

  // T&S should not be passed through or logged.
  ASSERT_TRUE(log_entry_received_);
  const auto logged_on_device_model_execution_info =
      log_entry_received_->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info();
  for (const auto& execution_info :
       logged_on_device_model_execution_info.execution_infos()) {
    EXPECT_FALSE(execution_info.request().has_text_safety_model_request());
  }
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

  AddContext(*session, "context");
  task_environment_.RunUntilIdle();

  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx: off:0 max:4\n",
      "Context: cont off:4 max:4\n",
      "Context: ext off:8 max:4\n",
      "Input: execute:contextfoo\n",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
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

  test_controller_->LaunchService();
  ExecuteModel(*session, "foo");
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kDisconnectAndCancel, 1);

  ASSERT_TRUE(response_error_);
  EXPECT_EQ(
      *response_error_,
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnAddContext) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();

  ExecuteModel(*session, "foo");
  base::HistogramTester histogram_tester;
  AddContext(*session, "bar");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kCancelled, 1);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_error_);
  EXPECT_EQ(
      *response_error_,
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
  ASSERT_FALSE(log_entry_received_);
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnExecute) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();

  ExecuteModel(*session, "foo");
  ExecuteModel(*session, "bar");
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_error_);
  EXPECT_EQ(
      *response_error_,
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
  EXPECT_TRUE(response_received_);
  EXPECT_EQ(*response_received_, "Input: execute:bar\n");
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
  AddContext(*session, "baz");
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
  AddContext(*session, "foo");
  task_environment_.RunUntilIdle();

  // Launch the service again, which triggers disconnect.
  test_controller_->LaunchService();
  task_environment_.RunUntilIdle();

  // Send some text, ensuring the context is received.
  ExecuteModel(*session, "baz");
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kUsedOnDevice, 1);
  ASSERT_TRUE(response_received_);
  const std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:foo off:0 max:10\n",
      "Input: execute:foobaz\n",
  });
  EXPECT_EQ(*response_received_, expected_responses[1]);
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->compose()
                .request()
                .page_metadata()
                .page_url(),
            "baz");
  EXPECT_EQ(
      log_entry_received_->log_ai_data_request()->compose().response().output(),
      "Context: ctx:foo off:0 max:10\nInput: execute:foobaz\n");
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextExecuteDisconnect) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  AddContext(*session, "foo");
  task_environment_.RunUntilIdle();
  // Send the text, this won't make it because the service is immediately
  // killed.
  ExecuteModel(*session, "bar");
  test_controller_->LaunchService();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(response_received_);
  ASSERT_FALSE(log_entry_received_);
}

TEST_F(OnDeviceModelServiceControllerTest, ExecuteDisconnectedSession) {
  Initialize();
  auto session1 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session1);
  AddContext(*session1, "foo");
  task_environment_.RunUntilIdle();

  // Start another session.
  auto session2 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session2);
  AddContext(*session2, "bar");
  task_environment_.RunUntilIdle();

  ExecuteModel(*session2, "2");
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(response_received_);
  const std::vector<std::string> expected_responses1 = {
      "Context: ctx:bar off:0 max:10\n",
      "Context: ctx:bar off:0 max:10\nInput: execute:bar2\n",
  };
  EXPECT_EQ(*response_received_, expected_responses1[1]);
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses1));
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->compose()
                .request()
                .page_metadata()
                .page_url(),
            "2");
  EXPECT_EQ(
      log_entry_received_->log_ai_data_request()->compose().response().output(),
      "Context: ctx:bar off:0 max:10\nInput: execute:bar2\n");
  response_received_.reset();
  streamed_responses_.clear();
  log_entry_received_.reset();

  ExecuteModel(*session1, "1");
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(response_received_);
  const std::vector<std::string> expected_responses2 = {
      "Context: ctx:foo off:0 max:10\n",
      "Context: ctx:foo off:0 max:10\nInput: execute:foo1\n",
  };
  EXPECT_EQ(*response_received_, expected_responses2[1]);
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses2));
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->compose()
                .request()
                .page_metadata()
                .page_url(),
            "1");
  EXPECT_EQ(
      log_entry_received_->log_ai_data_request()->compose().response().output(),
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
    AddContext(*session, "baz");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kUsingServer, 1);
  }
  ExecuteModel(*session, "2");
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
    AddContext(*session, "foo");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kFailedConstructingInput, 1);
  }
  task_environment_.RunUntilIdle();
  {
    base::HistogramTester histogram_tester;
    ExecuteModel(*session, "2");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
        ExecuteModelResult::kUsedServer, 1);
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
  ExecuteModel(*session, "2");
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
  ExecuteModel(*session, "2z");
  base::HistogramTester histogram_tester;
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelTimeForInitialResponse() +
      base::Milliseconds(1));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kTimedOut, 1);
  EXPECT_TRUE(streamed_responses_.empty());
  EXPECT_FALSE(response_received_);
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
  EXPECT_FALSE(provided_by_on_device_.has_value());
}

TEST_F(OnDeviceModelServiceControllerTest,
       FallbackToServerOnDisconnectWhileWaitingForExecute) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();
  test_controller_->LaunchService();
  ExecuteModel(*session, "foo");
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
  ExecuteModel(*session, "foo");
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
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  ExecuteModel(*session, "foo");
  session.reset();
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());
  // Fast forward by the amount of time that triggers a disconnect.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  // As there are no sessions and no traffic for GetOnDeviceModelIdleTimeout()
  // the connection should be dropped.
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
    ExecuteModel(*session, "2z");
    task_environment_.FastForwardBy(
        features::GetOnDeviceModelTimeForInitialResponse() +
        base::Milliseconds(1));
    EXPECT_TRUE(streamed_responses_.empty());
    EXPECT_FALSE(response_received_);
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeatureWithRedactRule(config, "bar");
  Initialize({.config = config});

  // `foo` doesn't match the redaction, so should be returned.
  auto session1 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session1);
  ExecuteModelUsingInput(*session1, "foo");
  task_environment_.RunUntilIdle();
  const std::string expected_response1 = "Input: execute:foo\n";
  EXPECT_EQ(*response_received_, expected_response1);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response1));

  // Input and output contain text matching redact, so should not be redacted.
  response_received_.reset();
  streamed_responses_.clear();
  auto session2 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session2);
  ExecuteModelUsingInput(*session2, "abarx");
  task_environment_.RunUntilIdle();
  const std::string expected_response2 = "Input: execute:abarx\n";
  EXPECT_EQ(*response_received_, expected_response2);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response2));

  // Output contains redacted text (and  input doesn't), so redact.
  fake_settings_.set_execute_result({"Input: abarx\n"});
  response_received_.reset();
  streamed_responses_.clear();
  auto session3 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session3);
  ExecuteModelUsingInput(*session3, "foo");
  task_environment_.RunUntilIdle();
  const std::string expected_response3 = "Input: a[###]x\n";
  EXPECT_EQ(*response_received_, expected_response3);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response3));
}

TEST_F(OnDeviceModelServiceControllerTest, RejectedField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeatureWithRedactRule(config, "bar",
                                         proto::RedactBehavior::REJECT);
  Initialize({.config = config});

  auto session1 = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session1);
  ExecuteModelUsingInput(*session1, "bar");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_received_);
  ASSERT_TRUE(response_error_);
  EXPECT_EQ(
      *response_error_,
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  // Although we send an error, we should be sending a log entry back so the
  // filtering can be logged.
  ASSERT_TRUE(log_entry_received_);
  EXPECT_GT(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos(0)
                .response()
                .on_device_model_service_response()
                .status(),
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
}

TEST_F(OnDeviceModelServiceControllerTest, UsePreviousResponseForRewrite) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeatureWithRedactRule(config, "bar");
  // Add a rule that identifies `previous_response` of `rewrite_params`.
  auto& output_config = *config.mutable_output_config();
  auto& redact_rules = *output_config.mutable_redact_rules();
  auto& field = *redact_rules.add_fields_to_check();
  field.add_proto_descriptors()->set_tag_number(8);
  field.add_proto_descriptors()->set_tag_number(1);
  Initialize({.config = config});

  // Force 'bar' to be returned from model.
  fake_settings_.set_execute_result({"Input: bar\n"});

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  ExecuteModelWithRewrite(*session);
  task_environment_.RunUntilIdle();
  // `bar` shouldn't be rewritten as it's in the input.
  const std::string expected_response = "Input: bar\n";
  EXPECT_EQ(*response_received_, expected_response);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response));
}

TEST_F(OnDeviceModelServiceControllerTest, ReplacementText) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeatureWithRedactRule(config, "bar")
      .set_replacement_string("[redacted]");
  Initialize({.config = config});

  // Output contains redacted text (and  input doesn't), so redact.
  fake_settings_.set_execute_result({"Input: abarx\n"});
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  ExecuteModelUsingInput(*session, "foo");
  task_environment_.RunUntilIdle();
  const std::string expected_response = "Input: a[redacted]x\n";
  EXPECT_EQ(*response_received_, expected_response);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response));
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
  ExecuteModelUsingInput(*session, "foo");
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more repeating text",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));

  ASSERT_TRUE(log_entry_received_);
  EXPECT_GT(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(log_entry_received_->log_ai_data_request()
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
  ExecuteModelUsingInput(*session, "foo");
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(response_received_);
  ASSERT_TRUE(response_error_);
  EXPECT_EQ(
      *response_error_,
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);

  ASSERT_TRUE(log_entry_received_);
  EXPECT_GT(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(log_entry_received_->log_ai_data_request()
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
  ExecuteModelUsingInput(*session, "foo");
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating",
      " text",
      " some more ",
      "repeating text",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));

  ASSERT_TRUE(log_entry_received_);
  EXPECT_GT(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(log_entry_received_->log_ai_data_request()
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
  ExecuteModelUsingInput(*session, "foo");
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));

  ASSERT_TRUE(log_entry_received_);
  EXPECT_GT(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_FALSE(log_entry_received_->log_ai_data_request()
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeature(kFeature, config);
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
  ExecuteModelUsingInput(*session, "foo");
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(streamed_responses_.empty());
  EXPECT_FALSE(response_received_);
  ASSERT_TRUE(response_error_);
  EXPECT_EQ(*response_error_, OptimizationGuideModelExecutionError::
                                  ModelExecutionError::kGenericFailure);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kFailedConstructingRemoteTextSafetyRequest, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, UseRemoteTextSafetyFallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTextSafetyRemoteFallback);

  base::HistogramTester histogram_tester;
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeature(kFeature, config);
  // Set input url proto field for text safety to just be user input.
  auto* input_url_proto_field = config.mutable_text_safety_fallback_config()
                                    ->mutable_input_url_proto_field();
  input_url_proto_field->add_proto_descriptors()->set_tag_number(7);
  input_url_proto_field->add_proto_descriptors()->set_tag_number(1);
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
  ExecuteModelUsingInput(*session, "foo");
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

  EXPECT_TRUE(streamed_responses_.empty());
  EXPECT_EQ(*response_received_, expected_responses.back());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kUsedOnDevice, 1);

  // Verify log entry.
  ASSERT_TRUE(log_entry_received_);
  // Should have 2 infos: one for text generation, one for safety fallback.
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            2);
  auto& ts_exec_info = log_entry_received_->log_ai_data_request()
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeature(kFeature, config);
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
  ExecuteModelUsingInput(*session, "foo");
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

  EXPECT_TRUE(streamed_responses_.empty());
  EXPECT_FALSE(response_received_);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kUsedOnDeviceOutputUnsafe, 1);

  // Verify log entry.
  ASSERT_TRUE(log_entry_received_);
  // Should have 2 infos: one for text generation, one for safety fallback.
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            2);
  auto& ts_exec_info = log_entry_received_->log_ai_data_request()
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeature(kFeature, config);
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
  ExecuteModelUsingInput(*session, "foo");
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

  ASSERT_TRUE(response_error_);
  EXPECT_EQ(*response_error_, OptimizationGuideModelExecutionError::
                                  ModelExecutionError::kGenericFailure);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kTextSafetyRemoteRequestFailed, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       UseRemoteTextSafetyFallbackNewRequestBeforeCallbackComesBack) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTextSafetyRemoteFallback);

  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  PopulateConfigForFeature(kFeature, config);
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
  ExecuteModelUsingInput(*session, "foo");
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

    ExecuteModelUsingInput(*session, "newquery");

    ASSERT_TRUE(response_error_);
    EXPECT_EQ(
        *response_error_,
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

TEST_F(OnDeviceModelServiceControllerTest, UsesTopKAndTemperature) {
  Initialize();
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      SessionConfigParams{.sampling_params = SamplingParams{
                              .top_k = 3,
                              .temperature = 2,
                          }});
  EXPECT_TRUE(session);
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  const std::string expected_response =
      "Input: execute:foo\nTopK: 3, Temp: 2\n";
  EXPECT_EQ(*response_received_, expected_response);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response));
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"token1", " token2", " token3", " token4"});
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1 token2 token3 token4"};
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
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
  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"token1", " token2", " token3", " token4"});
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1",
      "token1 token2",
      "token1 token2 token3",
      "token1 token2 token3 token4",
  };
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
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
  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result({"token1", " token2", " token3", " token4",
                                     " token5", " token6", " token7"});
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1 token2 token3",
      "token1 token2 token3 token4 token5 token6",
      "token1 token2 token3 token4 token5 token6 token7",
  };
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWhenOnDeviceModelAdaptationMissing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kOptimizationGuideComposeOnDeviceEval,
       features::internal::kModelAdaptationCompose},
      {});

  Initialize();

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_FALSE(session);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, TestAvailabilityObserver) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kModelAdaptationCompose, {}},
       {features::internal::kOnDeviceModelTestFeature,
        {{"enable_adaptation", "false"}}}},
      {});

  proto::OnDeviceModelExecutionFeatureConfig config_compose, config_test;
  config_compose.set_can_skip_text_safety(true);
  config_test.set_can_skip_text_safety(true);
  PopulateConfigForFeature(ModelBasedCapabilityKey::kCompose, config_compose);
  PopulateConfigForFeature(ModelBasedCapabilityKey::kTest, config_test);

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
  on_device_component_state_manager_.SetReady(temp_dir());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_test.reason_);
  EXPECT_EQ(OnDeviceModelEligibilityReason::kModelAdaptationNotAvailable,
            availability_observer_compose.reason_);

  test_controller_->MaybeUpdateModelAdaptation(
      ModelBasedCapabilityKey::kCompose,
      OnDeviceModelAdaptationMetadata::New(
          on_device_model::AdaptationAssetPaths(), kModelAdatationVersion,
          /*adapter=*/nullptr));
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

  proto::OnDeviceModelExecutionFeatureConfig config;
  PopulateConfigForFeature(kFeature, config);
  config.set_can_skip_text_safety(false);
  Initialize({.config = config});

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

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
  ExecuteModelUsingInput(*session, "foo");
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_received_);
  EXPECT_EQ(*response_received_,
            "some text some more repeating text some more repeating text");

  ASSERT_TRUE(log_entry_received_);
  EXPECT_GT(log_entry_received_->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(log_entry_received_->log_ai_data_request()
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
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  prompt->set_prompt("hello");
  prompt->set_expected_output("HELLO");

  base::HistogramTester histogram_tester;
  Initialize({.validation_config = validation_config});
  task_environment_.RunUntilIdle();
  // Service should be immediately shut down.
  EXPECT_FALSE(test_controller_->IsServiceConnected());

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
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  prompt->set_prompt("hello");
  prompt->set_expected_output("goodbye");

  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = validation_config});
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
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  prompt->set_prompt("hello");
  prompt->set_expected_output("hello");

  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = validation_config});
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
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  prompt->set_prompt("hello");
  prompt->set_expected_output("hello");

  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = validation_config});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  EXPECT_TRUE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                              logger_.GetWeakPtr(), nullptr,
                                              /*config_params=*/std::nullopt));

  fake_settings_.set_execute_result({"goodbye"});
  {
    base::HistogramTester histogram_tester;

    on_device_component_state_manager_.get()->OnStartup();
    task_environment_.RunUntilIdle();
    on_device_component_state_manager_.SetReady(temp_dir(), "0.0.2");
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  EXPECT_FALSE(test_controller_->CreateSession(kFeature, base::DoNothing(),
                                               logger_.GetWeakPtr(), nullptr,
                                               /*config_params=*/std::nullopt));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDoesNotRepeat) {
  proto::OnDeviceModelValidationConfig validation_config;
  auto* prompt = validation_config.add_validation_prompts();
  prompt->set_prompt("hello");
  prompt->set_expected_output("hello");

  {
    base::HistogramTester histogram_tester;
    Initialize({.validation_config = validation_config});
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

  // Session was created to the service should still be connected.
  EXPECT_TRUE(test_controller_->IsServiceConnected());
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

}  // namespace optimization_guide
