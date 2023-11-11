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

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

namespace optimization_guide {

constexpr proto::ModelExecutionFeature kFeature =
    proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_COMPOSE;

class FakeOnDeviceModel : public on_device_model::mojom::OnDeviceModel,
                          public on_device_model::mojom::Session {
 public:
  // on_device_model::mojom::OnDeviceModel:
  void StartSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) override {
    receivers_.Add(this, std::move(session));
  }

  // on_device_model::mojom::Session:
  void AddContext(on_device_model::mojom::InputOptionsPtr input,
                  mojo::PendingRemote<on_device_model::mojom::ContextClient>
                      client) override {
    context_.push_back(input->text);
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
  std::vector<std::string> context_;
  mojo::ReceiverSet<on_device_model::mojom::Session> receivers_;
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
    std::move(callback).Run(std::nullopt);
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
    proto::OnDeviceModelExecutionFeatureConfig config;
    config.set_feature(kFeature);
    auto& input_config = *config.mutable_input_config();
    input_config.set_request_base_name(proto::ComposeRequest().GetTypeName());

    // Execute call prefixes with execute:.
    auto& substitution = *input_config.add_execute_substitutions();
    substitution.set_expected_num_args(1);
    substitution.set_string_template("execute:%s");
    substitution.add_args()
        ->mutable_proto_field()
        ->add_proto_descriptors()
        ->set_tag_number(2);

    // Context call prefixes with context:.
    auto& context_substitution =
        *input_config.add_input_context_substitutions();
    context_substitution.set_expected_num_args(1);
    context_substitution.set_string_template("context:%s");
    context_substitution.add_args()
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
    request.set_user_input(std::string(input));
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
  base::test::ScopedFeatureList feature_list_{
      features::kOptimizationGuideOnDeviceModel};
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
  AddContext(*session, "bar");
  ExecuteModel(*session, "baz");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_received_);
  const std::vector<std::string> expected_responses = {
      "Context: context:foo\n",
      "Context: context:foo\nContext: context:bar\n",
      "Context: context:foo\nContext: context:bar\nInput: execute:baz\n",
  };
  EXPECT_EQ(*response_received_, expected_responses[2]);
  EXPECT_THAT(streamed_responses_, ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, SessionFailsForInvalidFeature) {
  EXPECT_FALSE(test_controller_.StartSession(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_UNSPECIFIED));
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
  EXPECT_EQ(*response_error_, OptimizationGuideModelExecutionError::
                                  ModelExecutionError::kGenericFailure);
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
