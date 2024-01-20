// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using ::base::test::TestMessage;
using ::testing::HasSubstr;

proto::ExecuteResponse BuildComposeResponse(const std::string& output) {
  proto::ComposeResponse compose_response;
  compose_response.set_output(output);
  proto::ExecuteResponse execute_response;
  proto::Any* any_metadata = execute_response.mutable_response_metadata();
  any_metadata->set_type_url("type.googleapis.com/" +
                             compose_response.GetTypeName());
  execute_response.set_server_execution_id("test_id");
  compose_response.SerializeToString(any_metadata->mutable_value());
  return execute_response;
}

class ModelExecutionManagerTest : public testing::Test {
 public:
  ModelExecutionManagerTest() = default;
  ~ModelExecutionManagerTest() override = default;

  void SetUp() override {
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    model_execution_manager_ = std::make_unique<ModelExecutionManager>(
        url_loader_factory_, identity_test_env_.identity_manager(),
        /*on_device_model_service_controller_=*/nullptr,
        &optimization_guide_logger_);
  }

  bool SimulateResponse(const std::string& content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        kOptimizationGuideServiceModelExecutionDefaultURL, content, http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  bool SimulateSuccessfulResponse() {
    std::string serialized_response;
    proto::ExecuteResponse execute_response =
        BuildComposeResponse("foo response");
    execute_response.SerializeToString(&serialized_response);
    return SimulateResponse(serialized_response, net::HTTP_OK);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  ModelExecutionManager* model_execution_manager() {
    return model_execution_manager_.get();
  }

  void CheckPendingRequestMessage(const std::string& message) {
    EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);
    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    auto body_bytes = pending_request->request.request_body->elements()
                          ->at(0)
                          .As<network::DataElementBytes>()
                          .AsStringPiece();
    EXPECT_THAT(body_bytes, HasSubstr(message));
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  OptimizationGuideLogger optimization_guide_logger_;
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;
};

TEST_F(ModelExecutionManagerTest, ExecuteModelEmptyAccessToken) {
  base::HistogramTester histogram_tester;
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  model_execution_manager()->ExecuteModel(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request,
      /*log_ai_data_request=*/nullptr,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(log_entry.get(), nullptr);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithUserSignIn) {
  base::HistogramTester histogram_tester;
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  model_execution_manager()->ExecuteModel(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request,
      /*log_ai_data_request=*/nullptr,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            EXPECT_TRUE(result.has_value());
            auto response =
                ParsedAnyMetadata<proto::ComposeResponse>(result.value());
            EXPECT_EQ("foo response", response->output());
            EXPECT_NE(log_entry, nullptr);
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_compose()
                            ->has_request_data());
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_compose()
                            ->has_response_data());
            EXPECT_EQ(log_entry->log_ai_data_request()
                          ->mutable_model_execution_info()
                          ->server_execution_id(),
                      "test_id");
            run_loop->Quit();
          },
          &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  EXPECT_TRUE(SimulateSuccessfulResponse());
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithServerError) {
  base::HistogramTester histogram_tester;

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result,
                      std::unique_ptr<ModelQualityLogEntry> log_entry) {
                     EXPECT_FALSE(result.has_value());
                     EXPECT_EQ(OptimizationGuideModelExecutionError::
                                   ModelExecutionError::kRetryableError,
                               result.error().error());
                     EXPECT_EQ(log_entry, nullptr);
                     run_loop->Quit();
                   },
                   &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  std::string serialized_response;
  proto::ExecuteResponse execute_response;
  execute_response.mutable_error_response()->set_error_state(
      proto::ErrorState::ERROR_STATE_INTERNAL_SERVER_ERROR_RETRY);
  execute_response.SerializeToString(&serialized_response);
  EXPECT_TRUE(SimulateResponse(serialized_response, net::HTTP_OK));

  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.ServerError.Compose",
      OptimizationGuideModelExecutionError::ModelExecutionError::
          kRetryableError,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithServerErrorAllowedForLogging) {
  base::HistogramTester histogram_tester;

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result,
                      std::unique_ptr<ModelQualityLogEntry> log_entry) {
                     EXPECT_FALSE(result.has_value());
                     EXPECT_EQ(OptimizationGuideModelExecutionError::
                                   ModelExecutionError::kUnsupportedLanguage,
                               result.error().error());
                     EXPECT_NE(log_entry, nullptr);
                     // Check that correct error state is recordered.
                     EXPECT_EQ(
                         proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE,
                         log_entry->log_ai_data_request()
                             ->mutable_model_execution_info()
                             ->mutable_error_response()
                             ->error_state());
                     run_loop->Quit();
                   },
                   &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  std::string serialized_response;
  proto::ExecuteResponse execute_response;
  execute_response.mutable_error_response()->set_error_state(
      proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE);
  execute_response.SerializeToString(&serialized_response);
  EXPECT_TRUE(SimulateResponse(serialized_response, net::HTTP_OK));

  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.ServerError.Compose",
      OptimizationGuideModelExecutionError::ModelExecutionError::
          kUnsupportedLanguage,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithPassthroughSession) {
  base::HistogramTester histogram_tester;

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result,
                      std::unique_ptr<ModelQualityLogEntry> log_entry) {
                     EXPECT_TRUE(result.has_value());
                     EXPECT_EQ("foo response",
                               ParsedAnyMetadata<proto::ComposeResponse>(
                                   result->response)
                                   ->output());
                     EXPECT_TRUE(result->is_complete);
                     EXPECT_NE(log_entry, nullptr);
                     EXPECT_TRUE(log_entry->log_ai_data_request()
                                     ->mutable_compose()
                                     ->has_request_data());
                     EXPECT_TRUE(log_entry->log_ai_data_request()
                                     ->mutable_compose()
                                     ->has_response_data());
                     run_loop->Quit();
                   },
                   &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  EXPECT_TRUE(SimulateSuccessfulResponse());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.Compose",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.Compose",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
}

TEST_F(ModelExecutionManagerTest, LogsContextToExecutionTimeHistogram) {
  base::HistogramTester histogram_tester;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  auto execute_model = [&] {
    base::RunLoop run_loop;
    proto::ComposeRequest request;
    request.mutable_generate_params()->set_user_input("some test");
    session->ExecuteModel(
        request, base::BindRepeating(
                     [](base::RunLoop* run_loop,
                        OptimizationGuideModelStreamingExecutionResult result,
                        std::unique_ptr<ModelQualityLogEntry> log_entry) {
                       run_loop->Quit();
                     },
                     &run_loop));
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            "access_token", base::Time::Max());
    CheckPendingRequestMessage("some test");
    EXPECT_TRUE(SimulateSuccessfulResponse());
    run_loop.Run();
  };

  constexpr char kHistogramName[] =
      "OptimizationGuide.ModelExecution.ContextStartToExecutionTime.Compose";

  // Execute without context should not log.
  execute_model();
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Just adding context should not log.
  proto::ComposeRequest context;
  context.mutable_generate_params()->set_user_input("context");
  session->AddContext(context);
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // First execute call after context should log.
  execute_model();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // Next execute call should not log.
  execute_model();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // Add context again and execute should log.
  session->AddContext(context);
  execute_model();
  histogram_tester.ExpectTotalCount(kHistogramName, 2);
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithPassthroughSessionAddContext) {
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  // Message is added through AddContext().
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("some test");
  session->AddContext(request);
  // ExecuteModel() uses empty message.
  session->ExecuteModel(
      proto::ComposeRequest(),
      base::BindRepeating(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelStreamingExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            run_loop->Quit();
          },
          &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  CheckPendingRequestMessage("some test");
  EXPECT_TRUE(SimulateSuccessfulResponse());
  run_loop.Run();
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithPassthroughSessionMultipleAddContext) {
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("first test");
  session->AddContext(request);
  request.mutable_generate_params()->set_user_input("second test");
  session->AddContext(request);
  // ExecuteModel() uses empty message.
  session->ExecuteModel(
      proto::ComposeRequest(),
      base::BindRepeating(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelStreamingExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            run_loop->Quit();
          },
          &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  CheckPendingRequestMessage("second test");
  EXPECT_TRUE(SimulateSuccessfulResponse());
  run_loop.Run();
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithPassthroughSessionExecuteOverridesAddContext) {
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  // First message is added through AddContext().
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("test_message");
  session->AddContext(request);
  // ExecuteModel() adds a different message.
  request.mutable_generate_params()->set_user_input("other test");
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result,
                      std::unique_ptr<ModelQualityLogEntry> log_entry) {
                     run_loop->Quit();
                   },
                   &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  CheckPendingRequestMessage("other test");
  EXPECT_TRUE(SimulateSuccessfulResponse());
  run_loop.Run();
}

TEST_F(ModelExecutionManagerTest, TestMultipleParallelRequests) {
  base::HistogramTester histogram_tester;
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop_old, run_loop_new;

  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);

  model_execution_manager()->ExecuteModel(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request,
      /*log_ai_data_request=*/nullptr,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(OptimizationGuideModelExecutionError::
                          ModelExecutionError::kCancelled,
                      result.error().error());
            run_loop->Quit();
          },
          &run_loop_old));

  model_execution_manager()->ExecuteModel(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, request,
      /*log_ai_data_request=*/nullptr,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            EXPECT_TRUE(result.has_value());
            auto response =
                ParsedAnyMetadata<proto::ComposeResponse>(result.value());
            EXPECT_EQ("foo response", response->output());
            EXPECT_NE(log_entry, nullptr);
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_compose()
                            ->has_request_data());
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_compose()
                            ->has_response_data());
            EXPECT_EQ(log_entry->log_ai_data_request()
                          ->mutable_model_execution_info()
                          ->server_execution_id(),
                      "test_id");
            run_loop->Quit();
          },
          &run_loop_new));

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  test_url_loader_factory()->EraseResponse(
      GURL(kOptimizationGuideServiceModelExecutionDefaultURL));
  EXPECT_TRUE(SimulateSuccessfulResponse());
  run_loop_new.Run();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.Result.Compose", 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

}  // namespace

}  // namespace optimization_guide
