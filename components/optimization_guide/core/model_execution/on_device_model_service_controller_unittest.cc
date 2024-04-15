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
#include "base/types/cxx23_to_underlying.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/test_on_device_model_component.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

using on_device_model::mojom::LoadModelResult;
using ExecuteModelResult = SessionImpl::ExecuteModelResult;

namespace {

// If non-zero this amount of delay is added before the response is sent.
base::TimeDelta g_execute_delay = base::TimeDelta();

// If non-empty, used as the output from Execute().
std::vector<std::string> g_model_execute_result;

// Used as the SafetyInfo output.
on_device_model::mojom::SafetyInfoPtr g_safety_info;

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

class FakeOnDeviceSession final : public on_device_model::mojom::Session {
 public:
  // on_device_model::mojom::Session:
  void AddContext(on_device_model::mojom::InputOptionsPtr input,
                  mojo::PendingRemote<on_device_model::mojom::ContextClient>
                      client) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeOnDeviceSession::AddContextInternal,
                                  weak_factory_.GetWeakPtr(), std::move(input),
                                  std::move(client)));
  }

  void Execute(on_device_model::mojom::InputOptionsPtr input,
               mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                   response) override {
    if (g_execute_delay.is_zero()) {
      ExecuteImpl(std::move(input), std::move(response));
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeOnDeviceSession::ExecuteImpl,
                       weak_factory_.GetWeakPtr(), std::move(input),
                       std::move(response)),
        g_execute_delay);
  }

 private:
  void ExecuteImpl(
      on_device_model::mojom::InputOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
          response) {
    mojo::Remote<on_device_model::mojom::StreamingResponder> remote(
        std::move(response));
    for (const std::string& context : context_) {
      auto chunk = on_device_model::mojom::ResponseChunk::New();
      chunk->text = "Context: " + context + "\n";
      remote->OnResponse(std::move(chunk));
    }

    if (g_model_execute_result.empty()) {
      auto chunk = on_device_model::mojom::ResponseChunk::New();
      chunk->text = "Input: " + input->text + "\n";
      if (input->top_k > 1) {
        chunk->text += "TopK: " + base::NumberToString(*input->top_k) +
                       ", Temp: " + base::NumberToString(*input->temperature) +
                       "\n";
      }
      if (g_safety_info) {
        chunk->safety_info = g_safety_info->Clone();
      }
      remote->OnResponse(std::move(chunk));
    } else {
      int safety_interval = input->safety_interval.value_or(1);
      int n = 0;
      for (const auto& text : g_model_execute_result) {
        n++;
        auto chunk = on_device_model::mojom::ResponseChunk::New();
        chunk->text = text;
        if (g_safety_info && (n % safety_interval) == 0) {
          chunk->safety_info = g_safety_info->Clone();
        }
        remote->OnResponse(std::move(chunk));
      }
    }
    auto summary = on_device_model::mojom::ResponseSummary::New();
    if (g_safety_info) {
      summary->safety_info = g_safety_info->Clone();
    }
    remote->OnComplete(std::move(summary));
  }

  void AddContextInternal(
      on_device_model::mojom::InputOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::ContextClient> client) {
    std::string suffix;
    std::string context = input->text;
    if (input->token_offset) {
      context.erase(context.begin(), context.begin() + *input->token_offset);
      suffix += " off:" + base::NumberToString(*input->token_offset);
    }
    if (input->max_tokens) {
      if (input->max_tokens < context.size()) {
        context.resize(*input->max_tokens);
      }
      suffix += " max:" + base::NumberToString(*input->max_tokens);
    }
    context_.push_back(context + suffix);
    uint32_t max_tokens = input->max_tokens.value_or(input->text.size());
    uint32_t token_offset = input->token_offset.value_or(0);
    if (client) {
      mojo::Remote<on_device_model::mojom::ContextClient> remote(
          std::move(client));
      remote->OnComplete(
          std::min(static_cast<uint32_t>(input->text.size()) - token_offset,
                   max_tokens));
    }
  }

  std::vector<std::string> context_;
  base::WeakPtrFactory<FakeOnDeviceSession> weak_factory_{this};
};

class FakeOnDeviceModel : public on_device_model::mojom::OnDeviceModel {
 public:
  // on_device_model::mojom::OnDeviceModel:
  void StartSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) override {
    // Mirror what the real OnDeviceModel does, which is only allow a single
    // Session.
    receivers_.Clear();
    receivers_.Add(std::make_unique<FakeOnDeviceSession>(), std::move(session));
  }

  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override {
    auto safety_info = on_device_model::mojom::SafetyInfo::New();

    // Text is unsafe if it contains "unsafe".
    bool has_unsafe = text.find("unsafe") != std::string::npos;
    safety_info->class_scores.emplace_back(has_unsafe ? 0.8 : 0.2);

    if (text.find("esperanto") != std::string::npos) {
      safety_info->language =
          on_device_model::mojom::LanguageDetectionResult::New("eo", 1.0);
    }

    std::move(callback).Run(std::move(safety_info));
  }

  void LoadAdaptation(
      on_device_model::mojom::LoadAdaptationParamsPtr params,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadAdaptationCallback callback) override {
    std::move(callback).Run(on_device_model::mojom::LoadModelResult::kSuccess);
  }

 private:
  mojo::UniqueReceiverSet<on_device_model::mojom::Session> receivers_;
};

class FakeOnDeviceModelService
    : public on_device_model::mojom::OnDeviceModelService {
 public:
  FakeOnDeviceModelService(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
          receiver,
      LoadModelResult result,
      bool drop_connection_request)
      : receiver_(this, std::move(receiver)),
        load_model_result_(result),
        drop_connection_request_(drop_connection_request) {}

  size_t on_device_model_receiver_count() const {
    return model_receivers_.size();
  }

 private:
  // on_device_model::mojom::OnDeviceModelService:
  void LoadModel(
      on_device_model::mojom::LoadModelParamsPtr params,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback) override {
    if (drop_connection_request_) {
      std::move(callback).Run(load_model_result_);
      return;
    }
    auto test_model = std::make_unique<FakeOnDeviceModel>();
    model_receivers_.Add(std::move(test_model), std::move(model));
    std::move(callback).Run(load_model_result_);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void LoadPlatformModel(
      const base::Uuid& uuid,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback) override {
    if (drop_connection_request_) {
      std::move(callback).Run(load_model_result_);
      return;
    }
    auto test_model = std::make_unique<FakeOnDeviceModel>();
    model_receivers_.Add(std::move(test_model), std::move(model));
    std::move(callback).Run(load_model_result_);
  }
#endif
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override {
    std::move(callback).Run(
        on_device_model::mojom::PerformanceClass::kVeryHigh);
  }

  mojo::Receiver<on_device_model::mojom::OnDeviceModelService> receiver_;
  const LoadModelResult load_model_result_;
  const bool drop_connection_request_;
  mojo::UniqueReceiverSet<on_device_model::mojom::OnDeviceModel>
      model_receivers_;
};

class FakeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 public:
  FakeOnDeviceModelServiceController(
      std::unique_ptr<OnDeviceModelAccessController> access_controller,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager)
      : OnDeviceModelServiceController(
            std::move(access_controller),
            std::move(on_device_component_state_manager)) {}

  void LaunchService() override {
    did_launch_service_ = true;
    service_remote_.reset();
    service_ = std::make_unique<FakeOnDeviceModelService>(
        service_remote_.BindNewPipeAndPassReceiver(), load_model_result_,
        drop_connection_request_);
  }

  void clear_did_launch_service() { did_launch_service_ = false; }

  bool did_launch_service() const { return did_launch_service_; }

  void set_load_model_result(LoadModelResult result) {
    load_model_result_ = result;
  }

  void set_drop_connection_request(bool value) {
    drop_connection_request_ = value;
  }

  size_t on_device_model_receiver_count() const {
    return service_ ? service_->on_device_model_receiver_count() : 0;
  }

 private:
  ~FakeOnDeviceModelServiceController() override = default;

  LoadModelResult load_model_result_ = LoadModelResult::kSuccess;
  bool drop_connection_request_ = false;
  std::unique_ptr<FakeOnDeviceModelService> service_;
  bool did_launch_service_ = false;
};

class OnDeviceModelServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    g_model_execute_result.clear();
    g_safety_info.reset();
    g_execute_delay = base::TimeDelta();
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kOptimizationGuideOnDeviceModel,
          {{"on_device_model_min_tokens_for_context", "10"},
           {"on_device_model_max_tokens_for_context", "22"},
           {"on_device_model_context_token_chunk_size", "4"},
           {"on_device_model_topk", "1"},
           {"on_device_model_temperature", "0"}}},
         {features::kTextSafetyClassifier,
          {{"on_device_must_use_safety_model", "false"}}}},
        {});
    prefs::RegisterLocalStatePrefs(pref_service_.registry());

    // Fake the requirements to install the model.
    pref_service_.SetInteger(
        prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kLow));
    pref_service_.SetTime(
        prefs::localstate::kLastTimeOnDeviceEligibleFeatureWasUsed,
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
    // Whether to make the downloaded model available prior to initialization of
    // the service controller.
    bool model_component_ready = true;
  };

  void Initialize() { Initialize({}); }

  void Initialize(const InitializeParams& params) {
    if (params.config) {
      WriteFeatureConfig(*params.config);
    } else {
      proto::OnDeviceModelExecutionFeatureConfig default_config;
      PopulateConfigForFeature(default_config);
      WriteFeatureConfig(default_config);
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
      proto::OnDeviceModelExecutionFeatureConfig& config) {
    config.set_feature(ToModelExecutionFeatureProto(kFeature));
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
    PopulateConfigForFeature(config);
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
        std::move(access_controller),
        on_device_component_state_manager_.get()->GetWeakPtr());

    test_controller_->Init();
  }

  void WriteExecutionConfig(const proto::OnDeviceModelExecutionConfig& config) {
    CHECK(base::WriteFile(temp_dir().Append(kOnDeviceModelExecutionConfigFile),
                          config.SerializeAsString()));
  }

  void WriteFeatureConfig(
      const proto::OnDeviceModelExecutionFeatureConfig& config) {
    proto::OnDeviceModelExecutionConfig execution_config;
    *execution_config.add_feature_configs() = config;
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
  EXPECT_EQ(logged_on_device_model_execution_info.model_versions()
                .on_device_model_service_version()
                .component_version(),
            "0.0.1");
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
  g_execute_delay = base::Seconds(10);
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  AddContext(*session, "this is long context");
  // ExecuteModel() directly after AddContext() should only load first chunk.
  ExecuteModel(*session, "foo");

  // Give time to make sure we don't process the optional context.
  task_environment_.RunUntilIdle();
  task_environment_.FastForwardBy(g_execute_delay + base::Milliseconds(1));
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
  Initialize();
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTabOrganization, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr, /*config_params=*/std::nullopt));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
      "TabOrganization",
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
  Initialize();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_must_use_safety_model", "true"}});

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
}

TEST_F(OnDeviceModelServiceControllerTest, SafetyModelRetract) {
  Initialize();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_must_use_safety_model", "true"},
       {"on_device_retract_unsafe_content", "true"}});

  proto::TextSafetyModelMetadata model_metadata;
  auto* safety_config = model_metadata.add_feature_text_safety_configurations();
  safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
  auto* threshold1 = safety_config->add_safety_category_thresholds();
  threshold1->set_output_index(0);
  threshold1->set_threshold(0.5);
  auto* threshold2 = safety_config->add_safety_category_thresholds();
  threshold2->set_output_index(1);
  threshold2->set_threshold(0.5);
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

  // Scores never provided even on complete.
  {
    base::HistogramTester histogram_tester;
    g_safety_info.reset();
    ExecuteModel(*session, "foo");
    task_environment_.RunUntilIdle();
    EXPECT_FALSE(response_received_);
    ASSERT_TRUE(response_error_);
    EXPECT_EQ(*response_error_, OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kGenericFailure);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
        ExecuteModelResult::kResponseCompleteButNoRequiredSafetyScores, 1);
  }

  // Score exceeds threshold.
  {
    g_safety_info = on_device_model::mojom::SafetyInfo::New();
    g_safety_info->class_scores = {0.7, 0.3};
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
    EXPECT_TRUE(ts_log.request().has_text_safety_model_request());
    EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
                ElementsAre(0.7, 0.3));
    EXPECT_TRUE(ts_log.response().text_safety_model_response().is_unsafe());
  }

  // Invalid model output according to config.
  {
    g_safety_info = on_device_model::mojom::SafetyInfo::New();
    g_safety_info->class_scores = {0.3};
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
    EXPECT_TRUE(ts_log.request().has_text_safety_model_request());
    EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
                ElementsAre(0.3));
    EXPECT_TRUE(ts_log.response().text_safety_model_response().is_unsafe());
  }

  // Score below threshold. Text safety check passes.
  {
    g_safety_info = on_device_model::mojom::SafetyInfo::New();
    g_safety_info->class_scores = {0.3, 0.3};
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
    EXPECT_TRUE(ts_log.request().has_text_safety_model_request());
    EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
                ElementsAre(0.3, 0.3));
    EXPECT_FALSE(ts_log.response().text_safety_model_response().is_unsafe());
  }
}

TEST_F(OnDeviceModelServiceControllerTest, SafetyModelUsedButNoRetract) {
  Initialize();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_must_use_safety_model", "true"},
       {"on_device_retract_unsafe_content", "false"}});

  proto::TextSafetyModelMetadata model_metadata;
  auto* safety_config = model_metadata.add_feature_text_safety_configurations();
  safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
  auto* threshold1 = safety_config->add_safety_category_thresholds();
  threshold1->set_output_index(0);
  threshold1->set_threshold(0.5);
  auto* threshold2 = safety_config->add_safety_category_thresholds();
  threshold2->set_output_index(1);
  threshold2->set_threshold(0.5);
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

  // Score exceeds threshold. Would not pass but not retracting.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.7, 0.3};
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
  EXPECT_TRUE(ts_log.request().has_text_safety_model_request());
  EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
              ElementsAre(0.7, 0.3));
  EXPECT_TRUE(ts_log.response().text_safety_model_response().is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, RequestCheckPassesWithSafeUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  Initialize();

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.1);
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    auto* threshold1 = check->add_safety_category_thresholds();
    threshold1->set_output_index(0);
    threshold1->set_threshold(0.5);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as completely safe.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.0, 0.0};

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
  EXPECT_THAT(response_log.scores(), ElementsAre(0.2));
  EXPECT_FALSE(response_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, RequestCheckFailsWithUnsafeUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  Initialize();

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.1);
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    auto* threshold1 = check->add_safety_category_thresholds();
    threshold1->set_output_index(0);
    threshold1->set_threshold(0.5);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as completely safe.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.0, 0.0};

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
  EXPECT_THAT(response_log.scores(), ElementsAre(0.8));
  EXPECT_TRUE(response_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, RequestCheckIgnoredInDarkMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "false"}});
  Initialize();

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.1);
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    auto* threshold1 = check->add_safety_category_thresholds();
    threshold1->set_output_index(0);
    threshold1->set_threshold(0.5);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as completely safe.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.0, 0.0};

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
  EXPECT_THAT(response_log.scores(), ElementsAre(0.8));
  EXPECT_TRUE(response_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest,
       RequestCheckFailsWithSafeUrlWithFallbackThreshold) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});
  Initialize();

  {
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.1);
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    // Omitted check thresholds, should fallback to default.
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as completely safe.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.0, 0.0};

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
  EXPECT_THAT(response_log.scores(), ElementsAre(0.2));
  EXPECT_TRUE(response_log.is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest,
       RequestCheckFailsWithUnmetRequiredLanguage) {
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
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.1);
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    auto* threshold1 = check->add_safety_category_thresholds();
    threshold1->set_output_index(0);
    threshold1->set_threshold(0.5);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as completely safe.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.0, 0.0};
  g_safety_info->language =
      on_device_model::mojom::LanguageDetectionResult::New("eo", 1.0);

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
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.1);
    auto* check = safety_config->add_request_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("url: %s");
    AddPageUrlSubstitution(input_template);
    auto* threshold1 = check->add_safety_category_thresholds();
    threshold1->set_output_index(0);
    threshold1->set_threshold(0.5);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as completely safe.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.0, 0.0};
  g_safety_info->language =
      on_device_model::mojom::LanguageDetectionResult::New("eo", 1.0);

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
       RawOutputCheckPassesWithMetRequiredLanguage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_must_use_safety_model", "false"},
       {"on_device_retract_unsafe_content", "true"}});

  Initialize();

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.5);
    auto* check = safety_config->mutable_raw_output_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("safe_text in esperanto: %s");
    AddStringValueSubstitution(input_template);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as totally unsafe, but we expect to ignore these scores.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {1.0, 1.0};

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
            "safe_text in esperanto: Input: execute:some_url\n");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.2));
  EXPECT_FALSE(response_log.is_unsafe());
  EXPECT_EQ(response_log.language_code(), "eo");
}

TEST_F(OnDeviceModelServiceControllerTest,
       RawOutputCheckFailsWithUnsafeText) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_must_use_safety_model", "false"},
       {"on_device_retract_unsafe_content", "true"}});

  Initialize();

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.5);
    auto* check = safety_config->mutable_raw_output_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("unsafe_text in esperanto: %s");
    AddStringValueSubstitution(input_template);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as totally unsafe, but we expect to ignore these scores.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {1.0, 1.0};

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
            "unsafe_text in esperanto: Input: execute:some_url\n");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.8));
  EXPECT_TRUE(response_log.is_unsafe());
  EXPECT_EQ(response_log.language_code(), "eo");
}

TEST_F(OnDeviceModelServiceControllerTest,
       RawOutputCheckFailsWithSafeTextInUndeterminedLanguage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_must_use_safety_model", "false"},
       {"on_device_retract_unsafe_content", "true"}});

  Initialize();

  {
    // Configure a request safety check on the PageUrl.
    auto safety_config =
        std::make_unique<proto::FeatureTextSafetyConfiguration>();
    safety_config->add_allowed_languages("eo");
    auto* default_threshold = safety_config->add_safety_category_thresholds();
    default_threshold->set_output_index(0);
    default_threshold->set_threshold(0.5);
    auto* check = safety_config->mutable_raw_output_check();
    auto* input_template = check->add_input_template();
    input_template->set_string_template("safe_text in unknown language: %s");
    AddStringValueSubstitution(input_template);
    SetFeatureTextSafetyConfiguration(std::move(safety_config));
  }

  // Score output as totally unsafe, but we expect to ignore these scores.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {1.0, 1.0};

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
            "safe_text in unknown language: Input: execute:some_url\n");
  const auto& response_log = check_log.response().text_safety_model_response();
  EXPECT_THAT(response_log.scores(), ElementsAre(0.2));
  EXPECT_FALSE(response_log.is_unsafe());
  EXPECT_EQ(response_log.language_code(), "");
}

TEST_F(OnDeviceModelServiceControllerTest, SafetyModelDarkMode) {
  Initialize();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_must_use_safety_model", "false"},
       {"on_device_retract_unsafe_content", "false"}});

  proto::TextSafetyModelMetadata model_metadata;
  auto* safety_config = model_metadata.add_feature_text_safety_configurations();
  safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
  auto* threshold1 = safety_config->add_safety_category_thresholds();
  threshold1->set_output_index(0);
  threshold1->set_threshold(0.5);
  auto* threshold2 = safety_config->add_safety_category_thresholds();
  threshold2->set_output_index(1);
  threshold2->set_threshold(0.5);
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

  // Score exceeds threshold. Would not pass but not retracting.
  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.7, 0.3};
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
  EXPECT_TRUE(ts_log.request().has_text_safety_model_request());
  EXPECT_THAT(ts_log.response().text_safety_model_response().scores(),
              ElementsAre(0.7, 0.3));
  EXPECT_TRUE(ts_log.response().text_safety_model_response().is_unsafe());
}

TEST_F(OnDeviceModelServiceControllerTest, SafetyModelDarkModeNoFeatureConfig) {
  Initialize();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_must_use_safety_model", "false"},
       {"on_device_retract_unsafe_content", "false"}});

  proto::TextSafetyModelMetadata model_metadata;
  auto* other_feature_safety_config =
      model_metadata.add_feature_text_safety_configurations();
  other_feature_safety_config->set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  auto* threshold1 =
      other_feature_safety_config->add_safety_category_thresholds();
  threshold1->set_output_index(0);
  threshold1->set_threshold(0.5);
  auto* threshold2 =
      other_feature_safety_config->add_safety_category_thresholds();
  threshold2->set_output_index(1);
  threshold2->set_threshold(0.5);
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
  test_controller_->set_load_model_result(LoadModelResult::kGpuBlocked);
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
  test_controller_->set_load_model_result(LoadModelResult::kGpuBlocked);
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
  test_controller_->set_drop_connection_request(true);
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
    test_controller_->set_drop_connection_request(i % 2 == 1);
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
  test_controller_->set_drop_connection_request(true);
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
  pref_service_.SetString(prefs::localstate::kOnDeviceModelChromeVersion,
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
                .request_data()
                .page_metadata()
                .page_url(),
            "baz");
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->compose()
                .response_data()
                .output(),
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
                .request_data()
                .page_metadata()
                .page_url(),
            "2");
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->compose()
                .response_data()
                .output(),
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
                .request_data()
                .page_metadata()
                .page_url(),
            "1");
  EXPECT_EQ(log_entry_received_->log_ai_data_request()
                ->compose()
                .response_data()
                .output(),
            "Context: ctx:foo off:0 max:10\nInput: execute:foo1\n");
}

TEST_F(OnDeviceModelServiceControllerTest, CallsRemoteExecute) {
  Initialize();
  test_controller_->set_load_model_result(LoadModelResult::kGpuBlocked);
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
  g_execute_delay = features::GetOnDeviceModelTimeForInitialResponse() * 2;

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
                .request_data()
                .page_metadata()
                .page_url(),
            "2z");
  EXPECT_FALSE(
      log_ai_data_request_passed_to_remote_->compose().has_response_data());
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
      ExecuteModelResult::kDisconnectAndFallbackToServer, 1);
  EXPECT_TRUE(remote_execute_called_);
  ASSERT_TRUE(log_ai_data_request_passed_to_remote_);
  EXPECT_EQ(log_ai_data_request_passed_to_remote_->compose()
                .request_data()
                .page_metadata()
                .page_url(),
            "foo");
  EXPECT_FALSE(
      log_ai_data_request_passed_to_remote_->compose().has_response_data());
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
  g_execute_delay = features::GetOnDeviceModelTimeForInitialResponse() * 2;

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
  g_model_execute_result = {"Input: abarx\n"};
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
  PopulateConfigForFeatureWithRedactRule(config, "bar");
  // Add a rule that identifies `previous_response` of `rewrite_params`.
  auto& output_config = *config.mutable_output_config();
  auto& redact_rules = *output_config.mutable_redact_rules();
  auto& field = *redact_rules.add_fields_to_check();
  field.add_proto_descriptors()->set_tag_number(8);
  field.add_proto_descriptors()->set_tag_number(1);
  Initialize({.config = config});

  // Force 'bar' to be returned from model.
  g_model_execute_result = {"Input: bar\n"};

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
  PopulateConfigForFeatureWithRedactRule(config, "bar")
      .set_replacement_string("[redacted]");
  Initialize({.config = config});

  // Output contains redacted text (and  input doesn't), so redact.
  g_model_execute_result = {"Input: abarx\n"};
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

  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  };
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

  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  };
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
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      true, 1);
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

  g_model_execute_result = {
      "some text",   " some more repeating", " text",
      " some more ", "repeating text",       " more stuff",
  };
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

  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  };
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
  PopulateConfigForFeature(config);
  Initialize({.config = config});

  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  };
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
  PopulateConfigForFeature(config);
  // Set input url proto field for text safety to just be user input.
  auto* input_url_proto_field = config.mutable_text_safety_fallback_config()
                                    ->mutable_input_url_proto_field();
  input_url_proto_field->add_proto_descriptors()->set_tag_number(7);
  input_url_proto_field->add_proto_descriptors()->set_tag_number(1);
  Initialize({.config = config});

  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  };
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
  PopulateConfigForFeature(config);
  // Create an empty ts fallback config which is valid and will call the
  // fallback.
  config.mutable_text_safety_fallback_config();
  Initialize({.config = config});

  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  };
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
  PopulateConfigForFeature(config);
  // Create an empty ts fallback config which is valid and will call the
  // fallback.
  config.mutable_text_safety_fallback_config();
  Initialize({.config = config});

  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  };
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
  PopulateConfigForFeature(config);
  // Create an empty ts fallback config which is valid and will call the
  // fallback.
  config.mutable_text_safety_fallback_config();
  Initialize({.config = config});

  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  };
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
      std::move(access_controller),
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
        {{"on_device_must_use_safety_model", "true"},
         {"on_device_retract_unsafe_content", "true"},
         {"on_device_text_safety_token_interval",
          base::NumberToString(GetParam())}}}},
      {});

  Initialize();

  proto::TextSafetyModelMetadata model_metadata;
  auto* safety_config = model_metadata.add_feature_text_safety_configurations();
  safety_config->set_feature(ToModelExecutionFeatureProto(kFeature));
  auto* threshold1 = safety_config->add_safety_category_thresholds();
  threshold1->set_output_index(0);
  threshold1->set_threshold(0.5);
  auto* threshold2 = safety_config->add_safety_category_thresholds();
  threshold2->set_output_index(1);
  threshold2->set_threshold(0.5);
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

  g_safety_info = on_device_model::mojom::SafetyInfo::New();
  g_safety_info->class_scores = {0.3, 0.3};
  g_model_execute_result = {
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  };
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

}  // namespace optimization_guide
