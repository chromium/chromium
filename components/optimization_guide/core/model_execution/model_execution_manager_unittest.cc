// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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
  any_metadata->set_type_url("type.googleapis.com/" +
                             compose_response.GetTypeName());
  execute_response.set_server_execution_id("test_id");
  compose_response.SerializeToString(any_metadata->mutable_value());
  return execute_response;
}

class FakeServiceController : public OnDeviceModelServiceController {
 public:
  FakeServiceController() : OnDeviceModelServiceController(nullptr, nullptr) {}

  void LaunchService() override {}

  void MaybeUpdateSafetyModel(
      base::optional_ref<const ModelInfo> model_info) override {
    received_safety_info_ = true;
  }

  bool received_safety_info() const { return received_safety_info_; }

  std::optional<base::FilePath> language_detection_model_path() {
    return OnDeviceModelServiceController::language_detection_model_path();
  }

 private:
  ~FakeServiceController() override = default;

  bool received_safety_info_ = false;
};

class FakeModelProvider : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    switch (optimization_target) {
      case proto::OPTIMIZATION_TARGET_TEXT_SAFETY:
        registered_for_text_safety_ = true;
        break;

      case proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION:
        registered_for_language_detection_ = true;
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  void Reset() {
    registered_for_text_safety_ = false;
    registered_for_language_detection_ = false;
  }

  bool was_registered() const {
    return registered_for_text_safety_ && registered_for_language_detection_;
  }

 private:
  bool registered_for_text_safety_ = false;
  bool registered_for_language_detection_ = false;
};

class ModelExecutionManagerTest : public testing::Test {
 public:
  ModelExecutionManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kTextSafetyClassifier,
             features::internal::kModelAdaptationCompose});
  }
  ~ModelExecutionManagerTest() override = default;

  void SetUp() override {
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    model_execution::prefs::RegisterLocalStatePrefs(local_state_->registry());
    service_controller_ = base::MakeRefCounted<FakeServiceController>();
    CreateModelExecutionManager();
  }

  void CreateModelExecutionManager() {
    model_execution_manager_ = std::make_unique<ModelExecutionManager>(
        url_loader_factory_, local_state_.get(),
        identity_test_env_.identity_manager(), service_controller_,
        &model_provider_, /*on_device_component_state_manager=*/nullptr,
        &optimization_guide_logger_, nullptr);
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

  FakeModelProvider* model_provider() { return &model_provider_; }

  FakeServiceController* service_controller() {
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

  PrefService* local_state() { return local_state_.get(); }

  void Reset() { model_execution_manager_ = nullptr; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  signin::IdentityTestEnvironment identity_test_env_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<FakeServiceController> service_controller_;
  FakeModelProvider model_provider_;
  OptimizationGuideLogger optimization_guide_logger_;
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;
};

TEST_F(ModelExecutionManagerTest, ExecuteModelEmptyAccessToken) {
  base::HistogramTester histogram_tester;
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, request,
      /*log_ai_data_request=*/nullptr,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelExecutionResult result,
             std::unique_ptr<ModelQualityLogEntry> log_entry) {
            EXPECT_FALSE(result.has_value());
            EXPECT_NE(log_entry.get(), nullptr);
            EXPECT_EQ(3u,  // ModelExecutionError::kPermissionDenied
                      log_entry->log_ai_data_request()
                          ->model_execution_info()
                          .model_execution_error_enum());
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
      ModelBasedCapabilityKey::kCompose, request,
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
                            ->has_request());
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_compose()
                            ->has_response());
            EXPECT_EQ(log_entry->log_ai_data_request()
                          ->model_execution_info()
                          .execution_id(),
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
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result) {
                     EXPECT_FALSE(result.response.has_value());
                     EXPECT_EQ(OptimizationGuideModelExecutionError::
                                   ModelExecutionError::kDisabled,
                               result.response.error().error());
                     EXPECT_EQ(result.log_entry, nullptr);
                     run_loop->Quit();
                   },
                   &run_loop));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());

  std::string serialized_response;
  proto::ExecuteResponse execute_response;
  execute_response.mutable_error_response()->set_error_state(
      proto::ErrorState::ERROR_STATE_DISABLED);
  execute_response.SerializeToString(&serialized_response);
  EXPECT_TRUE(SimulateResponse(serialized_response, net::HTTP_OK));

  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.ServerError.Compose",
      OptimizationGuideModelExecutionError::ModelExecutionError::kDisabled, 1);
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
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result) {
                     EXPECT_FALSE(result.response.has_value());
                     EXPECT_EQ(OptimizationGuideModelExecutionError::
                                   ModelExecutionError::kUnsupportedLanguage,
                               result.response.error().error());
                     EXPECT_NE(result.log_entry, nullptr);
                     // Check that the correct error state and error enum are
                     // recorded:
                     auto model_execution_info =
                         result.log_entry->log_ai_data_request()
                             ->model_execution_info();
                     EXPECT_EQ(
                         proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE,
                         model_execution_info.error_response().error_state());
                     EXPECT_EQ(
                         7u,  // ModelExecutionError::kUnsupportedLanguage
                         model_execution_info.model_execution_error_enum());
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

TEST_F(ModelExecutionManagerTest, ExecuteModelExecutionModeSetOnDeviceOnly) {
  base::HistogramTester histogram_tester;

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
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

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose,
      SessionConfigParams{.execution_mode =
                              SessionConfigParams::ExecutionMode::kServerOnly});
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result) {
                     EXPECT_TRUE(result.response.has_value());
                     EXPECT_EQ("foo response",
                               ParsedAnyMetadata<proto::ComposeResponse>(
                                   result.response->response)
                                   ->output());
                     EXPECT_TRUE(result.response->is_complete);
                     EXPECT_NE(result.log_entry, nullptr);
                     EXPECT_TRUE(result.log_entry->log_ai_data_request()
                                     ->mutable_compose()
                                     ->has_request());
                     EXPECT_TRUE(result.log_entry->log_ai_data_request()
                                     ->mutable_compose()
                                     ->has_response());
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
  // Should not even test for on-device eligibility.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      0);
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelExecutionModeExplicitlySetToDefault) {
  base::HistogramTester histogram_tester;

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose,
      SessionConfigParams{.execution_mode =
                              SessionConfigParams::ExecutionMode::kDefault});
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result) {
                     EXPECT_TRUE(result.response.has_value());
                     EXPECT_EQ("foo response",
                               ParsedAnyMetadata<proto::ComposeResponse>(
                                   result.response->response)
                                   ->output());
                     EXPECT_TRUE(result.response->is_complete);
                     EXPECT_NE(result.log_entry, nullptr);
                     EXPECT_TRUE(result.log_entry->log_ai_data_request()
                                     ->mutable_compose()
                                     ->has_request());
                     EXPECT_TRUE(result.log_entry->log_ai_data_request()
                                     ->mutable_compose()
                                     ->has_response());
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
  // Should test for on-device eligibility.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithPassthroughSession) {
  base::HistogramTester histogram_tester;

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  base::RunLoop run_loop;
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  auto session = model_execution_manager()->StartSession(
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result) {
                     EXPECT_TRUE(result.response.has_value());
                     EXPECT_EQ("foo response",
                               ParsedAnyMetadata<proto::ComposeResponse>(
                                   result.response->response)
                                   ->output());
                     EXPECT_TRUE(result.response->is_complete);
                     EXPECT_NE(result.log_entry, nullptr);
                     EXPECT_TRUE(result.log_entry->log_ai_data_request()
                                     ->mutable_compose()
                                     ->has_request());
                     EXPECT_TRUE(result.log_entry->log_ai_data_request()
                                     ->mutable_compose()
                                     ->has_response());
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
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  auto execute_model = [&] {
    base::RunLoop run_loop;
    proto::ComposeRequest request;
    request.mutable_generate_params()->set_user_input("some test");
    session->ExecuteModel(
        request, base::BindRepeating(
                     [](base::RunLoop* run_loop,
                        OptimizationGuideModelStreamingExecutionResult result) {
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
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  // Message is added through AddContext().
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("some test");
  session->AddContext(request);
  // ExecuteModel() uses empty message.
  session->ExecuteModel(
      proto::ComposeRequest(),
      base::BindRepeating(
          [](base::RunLoop* run_loop,
             OptimizationGuideModelStreamingExecutionResult result) {
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
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
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
             OptimizationGuideModelStreamingExecutionResult result) {
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
      ModelBasedCapabilityKey::kCompose, /*config_params=*/std::nullopt);
  // First message is added through AddContext().
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("test_message");
  session->AddContext(request);
  // ExecuteModel() adds a different message.
  request.mutable_generate_params()->set_user_input("other test");
  session->ExecuteModel(
      request, base::BindRepeating(
                   [](base::RunLoop* run_loop,
                      OptimizationGuideModelStreamingExecutionResult result) {
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
      ModelBasedCapabilityKey::kCompose, request,
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
      ModelBasedCapabilityKey::kCompose, request,
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
                            ->has_request());
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_compose()
                            ->has_response());
            EXPECT_EQ(log_entry->log_ai_data_request()
                          ->model_execution_info()
                          .execution_id(),
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

TEST_F(ModelExecutionManagerTest, DoesNotRegisterTextSafetyIfNotEnabled) {
  EXPECT_FALSE(model_provider()->was_registered());
}

class ModelExecutionManagerSafetyEnabledTest
    : public ModelExecutionManagerTest {
 public:
  ModelExecutionManagerSafetyEnabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kTextSafetyClassifier},
        {features::internal::kModelAdaptationCompose});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ModelExecutionManagerSafetyEnabledTest,
       RegistersTextSafetyModelIfEnabled) {
  EXPECT_TRUE(model_provider()->was_registered());
}

TEST_F(ModelExecutionManagerSafetyEnabledTest,
       DoesNotNotifyServiceControllerWrongTarget) {
  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder().SetVersion(123).Build();
  model_execution_manager()->OnModelUpdated(
      proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, *model_info);

  EXPECT_FALSE(service_controller()->received_safety_info());
}

TEST_F(ModelExecutionManagerSafetyEnabledTest, NotifiesServiceController) {
  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder().SetVersion(123).Build();
  model_execution_manager()->OnModelUpdated(
      proto::OPTIMIZATION_TARGET_TEXT_SAFETY, *model_info);

  EXPECT_TRUE(service_controller()->received_safety_info());
}

TEST_F(ModelExecutionManagerSafetyEnabledTest, UpdateLanguageDetection) {
  const base::FilePath kTestPath{FILE_PATH_LITERAL("foo")};
  std::unique_ptr<ModelInfo> model_info = TestModelInfoBuilder()
                                              .SetVersion(123)
                                              .SetModelFilePath(kTestPath)
                                              .Build();
  model_execution_manager()->OnModelUpdated(
      proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION, *model_info);
  EXPECT_EQ(kTestPath, service_controller()->language_detection_model_path());
}

TEST_F(ModelExecutionManagerSafetyEnabledTest,
       NotRegisteredWhenDisabledByEnterprisePolicy) {
  model_provider()->Reset();
  local_state()->SetInteger(
      model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      static_cast<int>(model_execution::prefs::
                           GenAILocalFoundationalModelEnterprisePolicySettings::
                               kDisallowed));
  CreateModelExecutionManager();
  EXPECT_FALSE(model_provider()->was_registered());

  // Reset manager to make sure removing observer doesn't crash.
  Reset();
}

}  // namespace

}  // namespace optimization_guide
