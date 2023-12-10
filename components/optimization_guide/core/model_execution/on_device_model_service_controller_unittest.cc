// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <memory>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
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

constexpr proto::ModelExecutionFeature kFeature =
    proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE;

class FakeOnDeviceSession : public base::SupportsWeakPtr<FakeOnDeviceSession>,
                            public on_device_model::mojom::Session {
 public:
  // on_device_model::mojom::Session:
  void AddContext(on_device_model::mojom::InputOptionsPtr input,
                  mojo::PendingRemote<on_device_model::mojom::ContextClient>
                      client) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeOnDeviceSession::AddContextInternal, AsWeakPtr(),
                       std::move(input), std::move(client)));
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
        base::BindOnce(&FakeOnDeviceSession::ExecuteImpl, AsWeakPtr(),
                       std::move(input), std::move(response)),
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
      remote->OnResponse("Context: " + context + "\n");
    }
    remote->OnResponse("Input: " + input->text + "\n");
    remote->OnComplete(on_device_model::mojom::ResponseStatus::kOk);
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
  explicit FakeOnDeviceModelServiceController(
      std::unique_ptr<OnDeviceModelAccessController> access_controller)
      : OnDeviceModelServiceController(std::move(access_controller)) {}

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
    g_execute_delay = base::TimeDelta();
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kOptimizationGuideOnDeviceModel,
        {{"on_device_model_min_tokens_for_context", "10"},
         {"on_device_model_max_tokens_for_context", "22"},
         {"on_device_model_context_token_chunk_size", "4"}});
    prefs::RegisterLocalStatePrefs(pref_service_.registry());
    RecreateServiceController();
  }

  ExecuteRemoteFn CreateExecuteRemoteFn() {
    return base::BindLambdaForTesting(
        [=](proto::ModelExecutionFeature feature,
            const google::protobuf::MessageLite& m,
            std::unique_ptr<proto::LogAiDataRequest> l,
            OptimizationGuideModelExecutionResultStreamingCallback c) {
          remote_execute_called_ = true;
          last_remote_message_ = base::WrapUnique(m.New());
          last_remote_message_->CheckTypeAndMergeFrom(m);
          log_ai_data_request_passed_to_remote_ = std::move(l);
        });
  }

  void RecreateServiceController() {
    test_controller_ = nullptr;
    access_controller_ = nullptr;

    auto access_controller =
        std::make_unique<OnDeviceModelAccessController>(pref_service_);
    access_controller_ = access_controller.get();
    test_controller_ = base::MakeRefCounted<FakeOnDeviceModelServiceController>(
        std::move(access_controller));
    proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_feature(kFeature);
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

    auto config_interpreter =
        std::make_unique<OnDeviceModelExecutionConfigInterpreter>();
    auto* config_interpreter_raw = config_interpreter.get();
    test_controller_->Init(base::FilePath::FromASCII("/foo"),
                           std::move(config_interpreter));
    config_interpreter_raw->OverrideFeatureConfigForTesting(config);
  }

  void AddContext(OptimizationGuideModelExecutor::Session& session,
                  std::string_view input) {
    proto::ComposeRequest request;
    request.mutable_generate_params()->set_user_input(std::string(input));
    session.AddContext(request);
  }

  void ExecuteModel(OptimizationGuideModelExecutor::Session& session,
                    std::string_view input) {
    proto::ComposeRequest request;
    request.mutable_page_metadata()->set_page_url(std::string(input));
    session.ExecuteModel(
        request,
        base::BindRepeating(&OnDeviceModelServiceControllerTest::OnResponse,
                            base::Unretained(this)));
  }

 protected:
  void OnResponse(OptimizationGuideModelStreamingExecutionResult result,
                  std::unique_ptr<ModelQualityLogEntry> log_entry) {
    log_entry_received_ = std::move(log_entry);
    if (!result.has_value()) {
      response_error_ = result.error().error();
      return;
    }
    auto response =
        ParsedAnyMetadata<proto::ComposeResponse>(result.value().response);
    if (result.value().is_complete) {
      response_received_ = response->output();
    } else {
      streamed_responses_.push_back(response->output());
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<FakeOnDeviceModelServiceController> test_controller_;
  // Owned by FakeOnDeviceModelServiceController.
  raw_ptr<OnDeviceModelAccessController> access_controller_ = nullptr;
  std::vector<std::string> streamed_responses_;
  std::optional<std::string> response_received_;
  std::unique_ptr<ModelQualityLogEntry> log_entry_received_;
  std::optional<OptimizationGuideModelExecutionError::ModelExecutionError>
      response_error_;
  base::test::ScopedFeatureList feature_list_;
  bool remote_execute_called_ = false;
  std::unique_ptr<google::protobuf::MessageLite> last_remote_message_;
  std::unique_ptr<proto::LogAiDataRequest>
      log_ai_data_request_passed_to_remote_;
  OptimizationGuideLogger logger_;
};

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionSuccess) {
  base::HistogramTester histogram_tester;

  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
  EXPECT_TRUE(session);
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  const std::string expected_response = "Input: execute:foo\n";
  EXPECT_EQ(*response_received_, expected_response);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response));
  EXPECT_TRUE(log_entry_received_);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionWithContext) {
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
  EXPECT_TRUE(session);

  AddContext(*session, "this is long context");
  // ExecuteModel() directly after AddContext() should only load first chunk.
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  std::vector<std::string> expected_responses = ConcatResponses({
      "Context: ctx:this i off:0 max:10\n",
      "Input: execute:this is long contextfoo\n",
  });
  EXPECT_EQ(*response_received_, expected_responses.back());
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, SessionFailsForInvalidFeature) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(test_controller_->CreateSession(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION,
      base::DoNothing(), &logger_));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
      "TabOrganization",
      OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionNoMinContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_min_tokens_for_context", "0"},
       {"on_device_model_max_tokens_for_context", "22"},
       {"on_device_model_context_token_chunk_size", "4"}});

  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_fallback_to_server_on_disconnect", "false"}});
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnExecute) {
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  // Start a session.
  test_controller_->set_load_model_result(LoadModelResult::kGpuBlocked);
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
  EXPECT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();

  {
    base::HistogramTester histogram_tester;

    // Because the model returned kGpuBlocked, no more sessions should start.
    EXPECT_FALSE(
        test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_));

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kGpuBlocked, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, DontRecreateSessionIfGpuBlocked) {
  test_controller_->set_load_model_result(LoadModelResult::kGpuBlocked);
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
  ASSERT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();
  test_controller_->clear_did_launch_service();

  // Adding context should not trigger launching the service again.
  AddContext(*session, "baz");
  EXPECT_FALSE(test_controller_->did_launch_service());
}

TEST_F(OnDeviceModelServiceControllerTest, StopsConnectingAfterMultipleDrops) {
  // Start a session.
  test_controller_->set_drop_connection_request(true);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    auto session =
        test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
    EXPECT_TRUE(session) << i;
    task_environment_.RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    auto session =
        test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
    EXPECT_FALSE(session);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kTooManyRecentCrashes, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, AlternatingDisconnectSucceeds) {
  // Start a session.
  for (int i = 0; i < 10; ++i) {
    test_controller_->set_drop_connection_request(i % 2 == 1);
    auto session =
        test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
    EXPECT_TRUE(session) << i;
    task_environment_.RunUntilIdle();
  }
}

TEST_F(OnDeviceModelServiceControllerTest,
       MultipleDisconnectsThenVersionChangeRetries) {
  // Create enough sessions that fail to trigger no longer creating a session.
  test_controller_->set_drop_connection_request(true);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    auto session =
        test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
    EXPECT_TRUE(session) << i;
    task_environment_.RunUntilIdle();
  }
  EXPECT_FALSE(
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_));

  // Change the pref to a different value and recreate the service.
  access_controller_ = nullptr;
  test_controller_.reset();
  pref_service_.SetString(prefs::localstate::kOnDeviceModelChromeVersion,
                          "BOGUS VERSION");
  RecreateServiceController();

  // A new session should be started because the version changed.
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
  EXPECT_TRUE(session);
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextDisconnectExecute) {
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  auto session1 =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
  EXPECT_TRUE(session1);
  AddContext(*session1, "foo");
  task_environment_.RunUntilIdle();

  // Start another session.
  auto session2 =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  test_controller_->set_load_model_result(LoadModelResult::kGpuBlocked);
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), &logger_);
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
  access_controller_ = nullptr;
  test_controller_ = nullptr;

  auto access_controller =
      std::make_unique<OnDeviceModelAccessController>(pref_service_);
  access_controller_ = access_controller.get();
  test_controller_ = base::MakeRefCounted<FakeOnDeviceModelServiceController>(
      std::move(access_controller));

  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_feature(kFeature);
  auto config_interpreter =
      std::make_unique<OnDeviceModelExecutionConfigInterpreter>();
  auto* config_interpreter_raw = config_interpreter.get();
  test_controller_->Init(base::FilePath::FromASCII("/foo"),
                         std::move(config_interpreter));
  config_interpreter_raw->OverrideFeatureConfigForTesting(config);

  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), &logger_);
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
  access_controller_ = nullptr;
  test_controller_ = nullptr;

  auto access_controller =
      std::make_unique<OnDeviceModelAccessController>(pref_service_);
  access_controller_ = access_controller.get();
  test_controller_ = base::MakeRefCounted<FakeOnDeviceModelServiceController>(
      std::move(access_controller));

  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_feature(kFeature);
  auto config_interpreter =
      std::make_unique<OnDeviceModelExecutionConfigInterpreter>();
  auto* config_interpreter_raw = config_interpreter.get();
  test_controller_->Init(base::FilePath::FromASCII("/foo"),
                         std::move(config_interpreter));
  config_interpreter_raw->OverrideFeatureConfigForTesting(config);

  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), &logger_);
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
  g_execute_delay = features::GetOnDeviceModelTimeForInitialResponse() * 2;

  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), &logger_);
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
}

TEST_F(OnDeviceModelServiceControllerTest,
       FallbackToServerOnDisconnectWhileWaitingForExecute) {
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), &logger_);
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
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
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
  auto session =
      test_controller_->CreateSession(kFeature, base::DoNothing(), &logger_);
  ASSERT_TRUE(session);
  ExecuteModel(*session, "foo");
  session.reset();
  EXPECT_TRUE(test_controller_->IsConnectedForTesting());
  // Fast forward by the amount of time that triggers a disconnect.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  // As there are no sessions and no traffice for GetOnDeviceModelIdleTimeout()
  // the connection should be dropped.
  EXPECT_FALSE(test_controller_->IsConnectedForTesting());
}

TEST_F(OnDeviceModelServiceControllerTest, UseServerWithRepeatedDelays) {
  g_execute_delay = features::GetOnDeviceModelTimeForInitialResponse() * 2;

  // Create a bunch of sessions that all timeout.
  for (int i = 0; i < features::GetOnDeviceModelTimeoutCountBeforeDisable();
       ++i) {
    auto session = test_controller_->CreateSession(
        kFeature, CreateExecuteRemoteFn(), &logger_);
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
  EXPECT_EQ(nullptr, test_controller_->CreateSession(
                         kFeature, base::DoNothing(), &logger_));
}

}  // namespace optimization_guide
