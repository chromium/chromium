// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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
  any_metadata->set_type_url(
      base::StrCat({"type.googleapis.com/", compose_response.GetTypeName()}));
  execute_response.set_server_execution_id("test_id");
  compose_response.SerializeToString(any_metadata->mutable_value());
  return execute_response;
}

class ModelExecutionManagerTest : public testing::Test {
 public:
  ModelExecutionManagerTest() = default;
  ~ModelExecutionManagerTest() override = default;

  // Sets up most of the fields except `model_execution_manager_` and
  // `component_manager_`, which are left to the test cases to set up.
  void SetUp() override {
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    service_controller_ = base::MakeRefCounted<OnDeviceModelServiceController>(
        nullptr, nullptr, base::DoNothing());
    model_execution_manager_ = std::make_unique<ModelExecutionManager>(
        url_loader_factory_, identity_test_env_.identity_manager(),
        service_controller_, &optimization_guide_logger_, nullptr);
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

  void SetAutomaticIssueOfAccessTokens() {
    identity_test_env()->MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  ModelExecutionManager* model_execution_manager() {
    return model_execution_manager_.get();
  }

  OnDeviceModelServiceController* service_controller() {
    return service_controller_.get();
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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<OnDeviceModelServiceController> service_controller_;
  OptimizationGuideLogger optimization_guide_logger_;
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;
};

TEST_F(ModelExecutionManagerTest, ExecuteModelEmptyAccessToken) {
  base::HistogramTester histogram_tester;
  RemoteResponseHolder response_holder;
  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, UserInputRequest("a user typed this"),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, response_holder.GetCallback());
  EXPECT_FALSE(response_holder.GetFinalStatus());
  ASSERT_NE(response_holder.log_entry(), nullptr);
  EXPECT_EQ(3u,  // ModelExecutionError::kPermissionDenied
            response_holder.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .model_execution_error_enum());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithUserSignIn) {
  base::HistogramTester histogram_tester;
  RemoteResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, UserInputRequest("a user typed this"),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, response_holder.GetCallback());
  EXPECT_TRUE(SimulateSuccessfulResponse());
  EXPECT_TRUE(response_holder.GetFinalStatus());
  EXPECT_EQ("foo response", response_holder.GetComposeOutput());
  EXPECT_NE(response_holder.log_entry(), nullptr);
  EXPECT_EQ(response_holder.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .execution_id(),
            "test_id");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithServerError) {
  base::HistogramTester histogram_tester;

  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  session->ExecuteModel(UserInputRequest("a user typed this"),
                        response_holder.GetStreamingCallback());

  std::string serialized_response;
  proto::ExecuteResponse execute_response;
  execute_response.mutable_error_response()->set_error_state(
      proto::ErrorState::ERROR_STATE_DISABLED);
  execute_response.SerializeToString(&serialized_response);
  EXPECT_TRUE(SimulateResponse(serialized_response, net::HTTP_OK));

  EXPECT_FALSE(response_holder.GetFinalStatus());
  EXPECT_EQ(
      OptimizationGuideModelExecutionError::ModelExecutionError::kDisabled,
      response_holder.error());
  EXPECT_EQ(response_holder.model_execution_info(), nullptr);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.ServerError.Compose",
      OptimizationGuideModelExecutionError::ModelExecutionError::kDisabled, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithServerErrorAllowedForLogging) {
  base::HistogramTester histogram_tester;

  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  session->ExecuteModel(UserInputRequest("a user typed this"),
                        response_holder.GetStreamingCallback());

  std::string serialized_response;
  proto::ExecuteResponse execute_response;
  execute_response.mutable_error_response()->set_error_state(
      proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE);
  execute_response.SerializeToString(&serialized_response);
  EXPECT_TRUE(SimulateResponse(serialized_response, net::HTTP_OK));

  EXPECT_FALSE(response_holder.GetFinalStatus());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kUnsupportedLanguage,
            response_holder.error());
  EXPECT_NE(response_holder.model_execution_info(), nullptr);
  // Check that the correct error state and error enum are
  // recorded:
  auto* model_execution_info = response_holder.model_execution_info();
  EXPECT_EQ(proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE,
            model_execution_info->error_response().error_state());
  EXPECT_EQ(7u,  // ModelExecutionError::kUnsupportedLanguage
            model_execution_info->model_execution_error_enum());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.ServerError.Compose",
      OptimizationGuideModelExecutionError::ModelExecutionError::
          kUnsupportedLanguage,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelExecutionModeSetOnDeviceOnly) {
  base::HistogramTester histogram_tester;

  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose,
      SessionConfigParams{
          .execution_mode = SessionConfigParams::ExecutionMode::kOnDeviceOnly});
  ASSERT_FALSE(session);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.Compose", 0);
  // Should test for on-device eligibility.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelExecutionModeSetToServerOnly) {
  base::HistogramTester histogram_tester;

  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose,
      SessionConfigParams{.execution_mode =
                              SessionConfigParams::ExecutionMode::kServerOnly});
  session->ExecuteModel(UserInputRequest("a user typed this"),
                        response_holder.GetStreamingCallback());
  EXPECT_TRUE(SimulateSuccessfulResponse());

  EXPECT_TRUE(response_holder.GetFinalStatus());
  EXPECT_EQ("foo response", response_holder.value());
  EXPECT_NE(response_holder.model_execution_info(), nullptr);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.Compose",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.Compose",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
  // Should not even test for on-device eligibility.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      0);
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelExecutionModeExplicitlySetToDefault) {
  base::HistogramTester histogram_tester;

  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose,
      SessionConfigParams{.execution_mode =
                              SessionConfigParams::ExecutionMode::kDefault});
  session->ExecuteModel(UserInputRequest("a user typed this"),
                        response_holder.GetStreamingCallback());
  EXPECT_TRUE(SimulateSuccessfulResponse());

  EXPECT_TRUE(response_holder.GetFinalStatus());
  EXPECT_EQ("foo response", response_holder.value());
  EXPECT_NE(response_holder.model_execution_info(), nullptr);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.Compose",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.SessionUsedRemoteExecution.Compose",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
  // Should test for on-device eligibility.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithPassthroughSession) {
  base::HistogramTester histogram_tester;

  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  session->ExecuteModel(UserInputRequest("a user typed this"),
                        response_holder.GetStreamingCallback());
  EXPECT_TRUE(SimulateSuccessfulResponse());

  EXPECT_TRUE(response_holder.GetFinalStatus());
  EXPECT_EQ("foo response", response_holder.value());
  EXPECT_NE(response_holder.model_execution_info(), nullptr);

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
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  auto execute_model = [&] {
    ResponseHolder response_holder;
    session->ExecuteModel(UserInputRequest("some test"),
                          response_holder.GetStreamingCallback());
    CheckPendingRequestMessage("some test");
    EXPECT_TRUE(SimulateSuccessfulResponse());
    EXPECT_TRUE(response_holder.GetFinalStatus());
  };

  constexpr char kHistogramName[] =
      "OptimizationGuide.ModelExecution.ContextStartToExecutionTime.Compose";

  // Execute without context should not log.
  execute_model();
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Just adding context should not log.
  session->AddContext(UserInputRequest("context"));
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // First execute call after context should log.
  execute_model();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // Next execute call should not log.
  execute_model();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // Add context again and execute should log.
  session->AddContext(UserInputRequest("context"));
  execute_model();
  histogram_tester.ExpectTotalCount(kHistogramName, 2);
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithPassthroughSessionAddContext) {
  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  // Message is added through AddContext().
  session->AddContext(UserInputRequest("some test"));
  // ExecuteModel() uses empty message.
  session->ExecuteModel(proto::ComposeRequest(),
                        response_holder.GetStreamingCallback());
  CheckPendingRequestMessage("some test");
  EXPECT_TRUE(SimulateSuccessfulResponse());
  EXPECT_TRUE(response_holder.GetFinalStatus());
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithPassthroughSessionMultipleAddContext) {
  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  session->AddContext(UserInputRequest("first test"));
  session->AddContext(UserInputRequest("second test"));
  // ExecuteModel() uses empty message.
  session->ExecuteModel(proto::ComposeRequest(),
                        response_holder.GetStreamingCallback());
  CheckPendingRequestMessage("second test");
  EXPECT_TRUE(SimulateSuccessfulResponse());
  EXPECT_TRUE(response_holder.GetFinalStatus());
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithPassthroughSessionExecuteOverridesAddContext) {
  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  // First message is added through AddContext().
  session->AddContext(UserInputRequest("test message"));
  // ExecuteModel() adds a different message.
  session->ExecuteModel(UserInputRequest("other test"),
                        response_holder.GetStreamingCallback());
  CheckPendingRequestMessage("other test");
  EXPECT_TRUE(SimulateSuccessfulResponse());
  EXPECT_TRUE(response_holder.GetFinalStatus());
}

TEST_F(ModelExecutionManagerTest, TestMultipleParallelRequests) {
  base::HistogramTester histogram_tester;
  RemoteResponseHolder response_holder1, response_holder2;

  SetAutomaticIssueOfAccessTokens();

  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, UserInputRequest("a user typed this"),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, response_holder1.GetCallback());

  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, UserInputRequest("a user typed this"),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, response_holder2.GetCallback());

  test_url_loader_factory()->EraseResponse(
      GURL(kOptimizationGuideServiceModelExecutionDefaultURL));
  EXPECT_TRUE(SimulateSuccessfulResponse());

  EXPECT_TRUE(response_holder2.GetFinalStatus());
  EXPECT_EQ("foo response", response_holder2.GetComposeOutput());
  EXPECT_NE(response_holder2.log_entry(), nullptr);
  EXPECT_EQ(response_holder2.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .execution_id(),
            "test_id");

  EXPECT_FALSE(response_holder1.GetFinalStatus());
  EXPECT_EQ(
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled,
      response_holder1.error());
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.Result.Compose", 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

}  // namespace

}  // namespace optimization_guide
