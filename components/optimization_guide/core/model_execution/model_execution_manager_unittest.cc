// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"
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

using ::base::test::EqualsProto;
using ::base::test::TestMessage;
using ::testing::HasSubstr;

class MockModelExecutionFetcher : public ModelExecutionFetcher {
 public:
  MOCK_METHOD(void,
              ExecuteModel,
              (ModelBasedCapabilityKey feature,
               signin::IdentityManager* identity_manager,
               const google::protobuf::MessageLite& request_metadata,
               std::optional<base::TimeDelta> timeout,
               ModelExecuteResponseCallback callback),
              (override));
};

class MockDelegate : public ModelExecutionManager::Delegate {
 public:
  MOCK_METHOD(std::unique_ptr<ModelExecutionFetcher>,
              CreateLegionFetcher,
              (),
              (override));
};

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
    model_execution_manager_ = std::make_unique<ModelExecutionManager>(
        url_loader_factory_, identity_test_env_.identity_manager(),
        /*delegate=*/nullptr, &optimization_guide_logger_, nullptr);
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

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  OptimizationGuideLogger optimization_guide_logger_;
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;
};

TEST_F(ModelExecutionManagerTest, ExecuteModelEmptyAccessToken) {
  base::HistogramTester histogram_tester;
  RemoteResponseHolder response_holder;
  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, UserInputRequest("a user typed this"),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, ModelExecutionServiceType::kDefault,
      response_holder.GetCallback());
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
      /*log_ai_data_request=*/nullptr, ModelExecutionServiceType::kDefault,
      response_holder.GetCallback());
  EXPECT_TRUE(SimulateSuccessfulResponse());
  EXPECT_TRUE(response_holder.GetFinalStatus());
  EXPECT_EQ("foo response",
            response_holder.GetOutput<proto::ComposeResponse>().output());
  EXPECT_NE(response_holder.log_entry(), nullptr);
  EXPECT_EQ(response_holder.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .execution_id(),
            "test_id");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Compose", true, 1);
}

// Tests that when a new request is issued and the total number of active
// requests would exceed the maximum for this feature, the oldest request is
// cancelled.
// Note that kCompose is limited to 1 active request at a time.
TEST_F(ModelExecutionManagerTest, MultipleParallelRequestsLimit) {
  base::HistogramTester histogram_tester;
  RemoteResponseHolder response_holder1, response_holder2;

  SetAutomaticIssueOfAccessTokens();

  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, UserInputRequest("a user typed this"),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, ModelExecutionServiceType::kDefault,
      response_holder1.GetCallback());

  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, UserInputRequest("a user typed this"),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, ModelExecutionServiceType::kDefault,
      response_holder2.GetCallback());

  test_url_loader_factory()->EraseResponse(
      GURL(kOptimizationGuideServiceModelExecutionDefaultURL));
  EXPECT_TRUE(SimulateSuccessfulResponse());

  EXPECT_TRUE(response_holder2.GetFinalStatus());
  EXPECT_EQ("foo response",
            response_holder2.GetOutput<proto::ComposeResponse>().output());
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

// Tests that multiple parallel model executions are possible for features that
// support it (like kFormsClassification).
TEST_F(ModelExecutionManagerTest, MultipleParallelRequests) {
  RemoteResponseHolder response_holder1, response_holder2;
  SetAutomaticIssueOfAccessTokens();

  // Trigger two parallel model executions.
  proto::AutofillAiTypeRequest request;
  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kFormsClassifications, request,
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, ModelExecutionServiceType::kDefault,
      response_holder1.GetCallback());
  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kFormsClassifications, request,
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, ModelExecutionServiceType::kDefault,
      response_holder2.GetCallback());

  // Simulate a successful response for both executions.
  proto::AutofillAiTypeResponse response;
  proto::ExecuteResponse execute_response;
  proto::Any* any_metadata = execute_response.mutable_response_metadata();
  any_metadata->set_type_url("type.googleapis.com/AutofillAiTypeResponse");
  response.SerializeToString(any_metadata->mutable_value());
  std::string serialized_response;
  execute_response.SerializeToString(&serialized_response);
  ASSERT_TRUE(SimulateResponse(serialized_response, net::HTTP_OK));
  ASSERT_TRUE(SimulateResponse(serialized_response, net::HTTP_OK));

  // Expect that both model executions succeeded.
  ASSERT_TRUE(response_holder1.GetFinalStatus());
  EXPECT_THAT(response_holder1.GetOutput<proto::AutofillAiTypeResponse>(),
              EqualsProto(response));
  ASSERT_TRUE(response_holder2.GetFinalStatus());
  EXPECT_THAT(response_holder2.GetOutput<proto::AutofillAiTypeResponse>(),
              EqualsProto(response));
}

class ModelExecutionManagerDelegateTest : public ModelExecutionManagerTest {
 public:
  void SetUp() override {
    ModelExecutionManagerTest::SetUp();
    auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
    delegate_ = delegate.get();
    model_execution_manager_ = std::make_unique<ModelExecutionManager>(
        url_loader_factory_, identity_test_env_.identity_manager(),
        std::move(delegate), &optimization_guide_logger_, nullptr);
  }

  MockDelegate* delegate() { return delegate_; }

 private:
  raw_ptr<MockDelegate> delegate_;
};

TEST_F(ModelExecutionManagerDelegateTest, UsesDelegateToCreateFetcher) {
  RemoteResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  auto fetcher = std::make_unique<MockModelExecutionFetcher>();
  EXPECT_CALL(*fetcher, ExecuteModel);
  EXPECT_CALL(*delegate(), CreateLegionFetcher)
      .WillOnce(testing::Return(testing::ByMove(std::move(fetcher))));

  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kZeroStateSuggestions, TestMessage(),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, ModelExecutionServiceType::kLegion,
      response_holder.GetCallback());
}

TEST_F(ModelExecutionManagerDelegateTest, CreatesDefaultFetcher) {
  RemoteResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  EXPECT_CALL(*delegate(), CreateLegionFetcher).Times(0);

  model_execution_manager()->ExecuteModel(
      ModelBasedCapabilityKey::kCompose, TestMessage(),
      /*timeout=*/std::nullopt,
      /*log_ai_data_request=*/nullptr, ModelExecutionServiceType::kDefault,
      response_holder.GetCallback());
}

}  // namespace

}  // namespace optimization_guide
