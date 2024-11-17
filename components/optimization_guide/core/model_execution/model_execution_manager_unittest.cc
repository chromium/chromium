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
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
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
  any_metadata->set_type_url("type.googleapis.com/" +
                             compose_response.GetTypeName());
  execute_response.set_server_execution_id("test_id");
  compose_response.SerializeToString(any_metadata->mutable_value());
  return execute_response;
}

class FakeServiceController : public OnDeviceModelServiceController {
 public:
  FakeServiceController()
      : OnDeviceModelServiceController(nullptr, nullptr, base::DoNothing()) {}

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
        NOTREACHED();
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

  // Sets up most of the fields except `model_execution_manager_` and
  // `component_manager_`, which are left to the test cases to set up.
  void SetUp() override {
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    model_execution::prefs::RegisterLocalStatePrefs(local_state_->registry());
    service_controller_ = base::MakeRefCounted<FakeServiceController>();
  }

  void CreateModelExecutionManager() {
    model_execution_manager_ = std::make_unique<ModelExecutionManager>(
        url_loader_factory_, local_state_.get(),
        identity_test_env_.identity_manager(), service_controller_,
        &model_provider_,
        component_manager_ ? component_manager_->get()->GetWeakPtr() : nullptr,
        &optimization_guide_logger_, nullptr);
  }

  void CreateComponentManager(bool should_observe) {
    component_manager_ =
        std::make_unique<TestOnDeviceModelComponentStateManager>(
            local_state_.get());
    component_manager_->get()->OnStartup();
    task_environment_.FastForwardBy(base::Seconds(1));
    if (should_observe) {
      component_manager_->get()->AddObserver(model_execution_manager_.get());
    }
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

  void SetModelComponentReady() {
    component_manager_->SetReady(base::FilePath());
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  PrefService* local_state() { return local_state_.get(); }

  void Reset() { model_execution_manager_ = nullptr; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  signin::IdentityTestEnvironment identity_test_env_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<FakeServiceController> service_controller_;
  std::unique_ptr<TestOnDeviceModelComponentStateManager> component_manager_;
  FakeModelProvider model_provider_;
  OptimizationGuideLogger optimization_guide_logger_;
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;
};

TEST_F(ModelExecutionManagerTest, ExecuteModelEmptyAccessToken) {
  CreateModelExecutionManager();
  base::HistogramTester histogram_tester;
  ResponseHolder response_holder;
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
  CreateModelExecutionManager();
  base::HistogramTester histogram_tester;
  ResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, UserInputRequest("a user typed this"),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, response_holder.GetCallback());
  EXPECT_TRUE(SimulateSuccessfulResponse());
  EXPECT_TRUE(response_holder.GetFinalStatus());
  EXPECT_EQ("foo response", response_holder.value());
  EXPECT_NE(response_holder.log_entry(), nullptr);
  EXPECT_TRUE(response_holder.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_request());
  EXPECT_TRUE(response_holder.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_response());
  EXPECT_EQ(response_holder.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .execution_id(),
            "test_id");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelWithServerError) {
  CreateModelExecutionManager();
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
  EXPECT_EQ(response_holder.log_entry(), nullptr);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.ServerError.Compose",
      OptimizationGuideModelExecutionError::ModelExecutionError::kDisabled, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

TEST_F(ModelExecutionManagerTest,
       ExecuteModelWithServerErrorAllowedForLogging) {
  CreateModelExecutionManager();
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
  EXPECT_NE(response_holder.log_entry(), nullptr);
  // Check that the correct error state and error enum are
  // recorded:
  auto model_execution_info = response_holder.log_entry()
                                  ->log_ai_data_request()
                                  ->model_execution_info();
  EXPECT_EQ(proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE,
            model_execution_info.error_response().error_state());
  EXPECT_EQ(7u,  // ModelExecutionError::kUnsupportedLanguage
            model_execution_info.model_execution_error_enum());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.ServerError.Compose",
      OptimizationGuideModelExecutionError::ModelExecutionError::
          kUnsupportedLanguage,
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", false, 1);
}

TEST_F(ModelExecutionManagerTest, ExecuteModelExecutionModeSetOnDeviceOnly) {
  CreateModelExecutionManager();
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
  CreateModelExecutionManager();
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
  EXPECT_NE(response_holder.log_entry(), nullptr);
  EXPECT_TRUE(response_holder.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_request());
  EXPECT_TRUE(response_holder.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_response());

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
  CreateModelExecutionManager();
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
  EXPECT_NE(response_holder.log_entry(), nullptr);
  EXPECT_TRUE(response_holder.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_request());
  EXPECT_TRUE(response_holder.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_response());

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
  CreateModelExecutionManager();
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
  EXPECT_NE(response_holder.log_entry(), nullptr);
  EXPECT_TRUE(response_holder.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_request());
  EXPECT_TRUE(response_holder.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_response());

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
  CreateModelExecutionManager();
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
  CreateModelExecutionManager();
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
  CreateModelExecutionManager();
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
  CreateModelExecutionManager();
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
  CreateModelExecutionManager();
  base::HistogramTester histogram_tester;
  ResponseHolder response_holder1, response_holder2;

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
  EXPECT_EQ("foo response", response_holder2.value());
  EXPECT_NE(response_holder2.log_entry(), nullptr);
  EXPECT_TRUE(response_holder2.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_request());
  EXPECT_TRUE(response_holder2.log_entry()
                  ->log_ai_data_request()
                  ->mutable_compose()
                  ->has_response());
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

TEST_F(ModelExecutionManagerTest, DoesNotRegisterTextSafetyIfNotEnabled) {
  CreateModelExecutionManager();
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(ModelExecutionManagerSafetyEnabledTest,
       RegistersTextSafetyModelWithOverrideModel) {
  // Effectively, when an override is set, the model component will be ready
  // before ModelExecutionManager can be added as an observer. Here we simulate
  // that by simply setting up the component without adding
  // ModelExecutionManager as an observer.
  CreateComponentManager(/*should_observe=*/false);
  SetModelComponentReady();
  CreateModelExecutionManager();

  EXPECT_TRUE(model_provider()->was_registered());
}

TEST_F(ModelExecutionManagerSafetyEnabledTest,
       RegistersTextSafetyModelIfEnabled) {
  CreateModelExecutionManager();
  EXPECT_FALSE(model_provider()->was_registered());

  // Text safety model should only be registered after the base model is ready.
  local_state()->SetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass,
      base::to_underlying(OnDeviceModelPerformanceClass::kHigh));
  CreateComponentManager(/*should_observe=*/true);
  SetModelComponentReady();

  EXPECT_TRUE(model_provider()->was_registered());
}
#endif

TEST_F(ModelExecutionManagerSafetyEnabledTest,
       DoesNotNotifyServiceControllerWrongTarget) {
  CreateModelExecutionManager();
  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder().SetVersion(123).Build();
  model_execution_manager()->OnModelUpdated(
      proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, *model_info);

  EXPECT_FALSE(service_controller()->received_safety_info());
}

TEST_F(ModelExecutionManagerSafetyEnabledTest, NotifiesServiceController) {
  CreateModelExecutionManager();
  std::unique_ptr<ModelInfo> model_info =
      TestModelInfoBuilder().SetVersion(123).Build();
  model_execution_manager()->OnModelUpdated(
      proto::OPTIMIZATION_TARGET_TEXT_SAFETY, *model_info);

  EXPECT_TRUE(service_controller()->received_safety_info());
}

TEST_F(ModelExecutionManagerSafetyEnabledTest, UpdateLanguageDetection) {
  CreateModelExecutionManager();
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
  CreateModelExecutionManager();
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
