// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using base::test::TestMessage;

namespace {

constexpr char kOptimizationGuideServiceUrl[] =
    "https://optimization-guide-server.com/?key=foo_key";

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
        kOptimizationGuideServiceUrl, content, http_status,
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
  base::test::TestMessage test_message;
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
  run_loop.RunUntilIdle();
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithUserSignIn) {
  base::test::TestMessage test_message;
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
  SimulateSuccessfulResponse();
  run_loop.RunUntilIdle();
}

}  // namespace

}  // namespace optimization_guide
