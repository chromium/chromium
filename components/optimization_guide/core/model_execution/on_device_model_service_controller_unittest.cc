// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/model_execution/on_device_model_stream_receiver.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

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
    mojo::Remote<on_device_model::mojom::StreamingResponder> remote(
        std::move(response));
    for (const std::string& context : context_) {
      remote->OnResponse("Context: " + context + "\n");
    }
    remote->OnResponse("Input: " + input->text + "\n");
    remote->OnComplete();
  }

 private:
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
    receivers_.Add(std::make_unique<FakeOnDeviceSession>(), std::move(session));
  }

 private:
  mojo::UniqueReceiverSet<on_device_model::mojom::Session> receivers_;
};

class FakeOnDeviceModelService
    : public on_device_model::mojom::OnDeviceModelService {
 public:
  explicit FakeOnDeviceModelService(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
          receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // on_device_model::mojom::OnDeviceModelService:
  void LoadModel(
      on_device_model::mojom::LoadModelParamsPtr params,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback) override {
    auto test_model = std::make_unique<FakeOnDeviceModel>();
    model_receivers_.Add(std::move(test_model), std::move(model));
    std::move(callback).Run(on_device_model::mojom::LoadModelResult::kSuccess);
  }
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override {
    std::move(callback).Run(
        on_device_model::mojom::PerformanceClass::kVeryHigh);
  }

  mojo::Receiver<on_device_model::mojom::OnDeviceModelService> receiver_;
  mojo::UniqueReceiverSet<on_device_model::mojom::OnDeviceModel>
      model_receivers_;
};

class FakeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 public:
  void LaunchService() override {
    service_remote_.reset();
    service_ = std::make_unique<FakeOnDeviceModelService>(
        service_remote_.BindNewPipeAndPassReceiver());
  }

 private:
  std::unique_ptr<FakeOnDeviceModelService> service_;
};

class OnDeviceModelServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kOptimizationGuideOnDeviceModel,
        {{"on_device_model_min_tokens_for_context", "10"},
         {"on_device_model_max_tokens_for_context", "22"},
         {"on_device_model_context_token_chunk_size", "4"}});
    proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_feature(kFeature);
    auto& input_config = *config.mutable_input_config();
    input_config.set_request_base_name(proto::ComposeRequest().GetTypeName());

    // Execute call prefixes with execute:.
    auto& substitution = *input_config.add_execute_substitutions();
    substitution.set_string_template("execute:%s%s");
    substitution.add_substitutions()
        ->add_candidates()
        ->mutable_proto_field()
        ->add_proto_descriptors()
        ->set_tag_number(2);
    auto* proto_field = substitution.add_substitutions()
                            ->add_candidates()
                            ->mutable_proto_field();
    proto_field->add_proto_descriptors()->set_tag_number(3);
    proto_field->add_proto_descriptors()->set_tag_number(1);

    // Context call prefixes with context:.
    auto& context_substitution =
        *input_config.add_input_context_substitutions();
    context_substitution.set_string_template("ctx:%s");
    context_substitution.add_substitutions()
        ->add_candidates()
        ->mutable_proto_field()
        ->add_proto_descriptors()
        ->set_tag_number(2);

    auto& output_config = *config.mutable_output_config();
    output_config.set_proto_type(proto::ComposeResponse().GetTypeName());
    output_config.mutable_proto_field()
        ->add_proto_descriptors()
        ->set_tag_number(1);

    auto config_interpreter =
        std::make_unique<OnDeviceModelExecutionConfigInterpreter>();
    auto* config_interpreter_raw = config_interpreter.get();
    test_controller_.Init(base::FilePath::FromASCII("/foo"),
                          std::move(config_interpreter));
    config_interpreter_raw->OverrideFeatureConfigForTesting(config);
  }

  void AddContext(OptimizationGuideModelExecutor::Session& session,
                  std::string_view input) {
    proto::ComposeRequest request;
    request.set_user_input(std::string(input));
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

  base::test::TaskEnvironment task_environment_;
  FakeOnDeviceModelServiceController test_controller_;
  std::vector<std::string> streamed_responses_;
  std::optional<std::string> response_received_;
  std::optional<OptimizationGuideModelExecutionError::ModelExecutionError>
      response_error_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionSuccess) {
  auto session = test_controller_.StartSession(kFeature);
  EXPECT_TRUE(session);
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  const std::string expected_response = "Input: execute:foo\n";
  EXPECT_EQ(*response_received_, expected_response);
  EXPECT_THAT(streamed_responses_, ElementsAre(expected_response));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionWithContext) {
  auto session = test_controller_.StartSession(kFeature);
  EXPECT_TRUE(session);
  AddContext(*session, "foo");
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
  auto session = test_controller_.StartSession(kFeature);
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
  auto session = test_controller_.StartSession(kFeature);
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
  auto session = test_controller_.StartSession(kFeature);
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
  EXPECT_FALSE(test_controller_.StartSession(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED));
}

TEST_F(OnDeviceModelServiceControllerTest, ModelExecutionNoMinContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_min_tokens_for_context", "0"},
       {"on_device_model_max_tokens_for_context", "22"},
       {"on_device_model_context_token_chunk_size", "4"}});

  auto session = test_controller_.StartSession(kFeature);
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

TEST_F(OnDeviceModelServiceControllerTest, SessionDisconnectCalled) {
  auto session = test_controller_.StartSession(kFeature);
  EXPECT_TRUE(session);
  // Make sure the service has launched.
  task_environment_.RunUntilIdle();

  // Wait for disconnect and restart service.
  base::RunLoop run_loop;
  session->SetDisconnectHandler(run_loop.QuitClosure());
  test_controller_.LaunchService();
  run_loop.Run();
}

TEST_F(OnDeviceModelServiceControllerTest, ReturnsErrorOnServiceDisconnect) {
  auto session = test_controller_.StartSession(kFeature);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();

  test_controller_.LaunchService();
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_error_);
  EXPECT_EQ(
      *response_error_,
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnAddContext) {
  auto session = test_controller_.StartSession(kFeature);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();

  ExecuteModel(*session, "foo");
  AddContext(*session, "bar");
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_error_);
  EXPECT_EQ(
      *response_error_,
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnExecute) {
  auto session = test_controller_.StartSession(kFeature);
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

TEST_F(OnDeviceModelServiceControllerTest, SessionRecoversAfterDisconnect) {
  auto session = test_controller_.StartSession(kFeature);
  EXPECT_TRUE(session);
  // Make sure the service has launched.
  task_environment_.RunUntilIdle();

  // Wait for disconnect and restart service.
  base::RunLoop run_loop;
  session->SetDisconnectHandler(run_loop.QuitClosure());
  test_controller_.LaunchService();
  run_loop.Run();

  // New session should still work.
  session = test_controller_.StartSession(kFeature);
  ExecuteModel(*session, "foo");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  EXPECT_EQ(*response_received_, "Input: execute:foo\n");
}

}  // namespace optimization_guide
