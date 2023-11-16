// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include <memory>

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

TestMessage BuildTestMessage(const std::string& test_message_str) {
  TestMessage test_message;
  test_message.set_test(test_message_str);
  return test_message;
}

proto::ExecuteResponse BuildTestExecuteResponse(const TestMessage& message) {
  proto::ExecuteResponse execute_response;
  proto::Any* any_metadata = execute_response.mutable_response_metadata();
  any_metadata->set_type_url("type.googleapis.com/" + message.GetTypeName());
  message.SerializeToString(any_metadata->mutable_value());
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
        BuildTestExecuteResponse(BuildTestMessage("foo response"));
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
  TestMessage test_message;
  test_message.set_test("some test");
  base::RunLoop run_loop;
  model_execution_manager()->ExecuteModel(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test_message,
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
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithUserSignIn) {
  TestMessage test_message;
  test_message.set_test("some test");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  model_execution_manager()->ExecuteModel(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE, test_message,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            EXPECT_TRUE(result.has_value());
            EXPECT_EQ("foo response",
                      ParsedAnyMetadata<TestMessage>(result.value())->test());
            EXPECT_EQ(log_entry.get(), nullptr);
            run_loop->Quit();
          },
          &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  EXPECT_TRUE(SimulateSuccessfulResponse());
  run_loop.Run();
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithPassthroughSession) {
  TestMessage test_message;
  test_message.set_test("some test");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  session->ExecuteModel(
      test_message,
      base::BindRepeating(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelStreamingExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            EXPECT_TRUE(result.has_value());
            EXPECT_EQ("foo response",
                      ParsedAnyMetadata<TestMessage>(result->response)->test());
            EXPECT_TRUE(result->is_complete);
            EXPECT_EQ(log_entry.get(), nullptr);
            run_loop->Quit();
          },
          &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  EXPECT_TRUE(SimulateSuccessfulResponse());
  run_loop.Run();
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithPassthroughSessionAddContext) {
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      proto::MODEL_EXECUTION_FEATURE_COMPOSE);
  // Message is added through AddContext().
  TestMessage test_message;
  test_message.set_test("some test");
  session->AddContext(test_message);
  // ExecuteModel() uses empty message.
  session->ExecuteModel(
      TestMessage(),
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
  TestMessage test_message;
  test_message.set_test("first test");
  session->AddContext(test_message);

  test_message.set_test("second test");
  session->AddContext(test_message);
  // ExecuteModel() uses empty message.
  session->ExecuteModel(
      TestMessage(),
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
  TestMessage test_message;
  test_message.set_test("some test");
  session->AddContext(test_message);
  // ExecuteModel() adds a different message.
  test_message.set_test("other test");
  session->ExecuteModel(
      test_message,
      base::BindRepeating(
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

}  // namespace

}  // namespace optimization_guide
