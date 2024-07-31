// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "optimization_guide_model_execution_error.h"
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

}  // namespace

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

class ModelExecutionFetcherTest : public testing::Test {
 public:
  ModelExecutionFetcherTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    model_execution_fetcher_ = std::make_unique<ModelExecutionFetcher>(
        shared_url_loader_factory_, GURL(kOptimizationGuideServiceUrl),
        /*optimization_guide_logger=*/nullptr);
  }
  ModelExecutionFetcherTest(const ModelExecutionFetcherTest&) = delete;
  ModelExecutionFetcherTest& operator=(const ModelExecutionFetcherTest&) =
      delete;

  ~ModelExecutionFetcherTest() override = default;

  void ExecuteModel(ModelBasedCapabilityKey feature,
                    const google::protobuf::MessageLite& request_metadata) {
    model_execution_fetcher_->ExecuteModel(
        feature, identity_test_env_.identity_manager(), request_metadata,
        base::BindOnce(&ModelExecutionFetcherTest::OnModelExecutionReceived,
                       base::Unretained(this)));
    RunUntilIdle();
  }

  void VerifyHasPendingFetchRequest() {
    EXPECT_GE(test_url_loader_factory_.NumPending(), 1);
    auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
    EXPECT_EQ(pending_request->request.method, "POST");
    std::string key_value;
    EXPECT_TRUE(net::GetValueForKeyInQuery(pending_request->request.url, "key",
                                           &key_value));
    last_authorization_request_header_.clear();
    if (std::optional<std::string> header =
            pending_request->request.headers.GetHeader(
                net::HttpRequestHeaders::kAuthorization);
        header) {
      last_authorization_request_header_ = *header;
    }

    EXPECT_EQ(pending_request->request.request_body->elements()->size(), 1u);
    auto& element =
        pending_request->request.request_body->elements_mutable()->front();
    DCHECK(element.type() == network::DataElement::Tag::kBytes);
    auto request_body = element.As<network::DataElementBytes>().AsStringPiece();
    last_execute_request_.reset();
    proto::ExecuteRequest execute_request;
    if (execute_request.ParseFromString(
            static_cast<std::string>(request_body))) {
      last_execute_request_ = execute_request;
    }
  }

  bool SimulateResponse(const std::string& content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        kOptimizationGuideServiceUrl, content, http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  bool SimulateSuccessfulResponse(
      const proto::ExecuteResponse& execute_response) {
    std::string serialized_response;
    execute_response.SerializeToString(&serialized_response);
    return SimulateResponse(serialized_response, net::HTTP_OK);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 protected:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void OnModelExecutionReceived(
      base::expected<const proto::ExecuteResponse,
                     OptimizationGuideModelExecutionError> response) {
    last_execute_response_.reset();
    if (response.has_value()) {
      last_execute_response_ = *response;
    } else {
      last_execute_response_ = base::unexpected(response.error());
    }
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<ModelExecutionFetcher> model_execution_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::HistogramTester histogram_tester_;

  std::optional<proto::ExecuteRequest> last_execute_request_;
  std::optional<base::expected<proto::ExecuteResponse,
                               OptimizationGuideModelExecutionError>>
      last_execute_response_;
  std::string last_authorization_request_header_;
};

TEST_F(ModelExecutionFetcherTest, TestSuccessfulResponse) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  ExecuteModel(ModelBasedCapabilityKey::kWallpaperSearch,
               BuildTestMessage("foo request"));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  VerifyHasPendingFetchRequest();

  SimulateSuccessfulResponse(
      BuildTestExecuteResponse(BuildTestMessage("foo response")));

  EXPECT_EQ(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH,
      last_execute_request_->feature());
  EXPECT_EQ("type.googleapis.com/base.test.TestMessage",
            last_execute_request_->request_metadata().type_url());
  EXPECT_EQ("type.googleapis.com/base.test.TestMessage",
            last_execute_response_->value().response_metadata().type_url());
  EXPECT_EQ("foo request", ParsedAnyMetadata<TestMessage>(
                               last_execute_request_->request_metadata())
                               ->test());
  EXPECT_EQ("foo response",
            ParsedAnyMetadata<TestMessage>(
                last_execute_response_->value().response_metadata())
                ->test());

  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.FetchLatency.WallpaperSearch",
      1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.NetErrorCode", -net::OK, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.Status", 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "WallpaperSearch",
      FetcherRequestStatus::kSuccess, 1);
}

TEST_F(ModelExecutionFetcherTest, TestNetErrorResponse) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  ExecuteModel(ModelBasedCapabilityKey::kWallpaperSearch,
               BuildTestMessage("foo request"));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  VerifyHasPendingFetchRequest();

  SimulateResponse("foo response", net::HTTP_NOT_FOUND);

  // Make sure histograms are recorded correctly on bad response.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.Status", net::HTTP_NOT_FOUND, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.NetErrorCode", 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "WallpaperSearch",
      FetcherRequestStatus::kResponseError, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.FetchLatency.WallpaperSearch",
      0);
  EXPECT_EQ(ModelExecutionError::kGenericFailure,
            last_execute_response_->error().error());
}

TEST_F(ModelExecutionFetcherTest, TestBadResponse) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  ExecuteModel(ModelBasedCapabilityKey::kWallpaperSearch,
               BuildTestMessage("foo request"));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  VerifyHasPendingFetchRequest();

  SimulateResponse("bad response", net::HTTP_OK);

  // Make sure histograms are recorded correctly on bad response.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.Status", net::HTTP_OK, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.NetErrorCode", 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "WallpaperSearch",
      FetcherRequestStatus::kResponseError, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.FetchLatency.WallpaperSearch",
      0);
  EXPECT_EQ(ModelExecutionError::kGenericFailure,
            last_execute_response_->error().error());
}

TEST_F(ModelExecutionFetcherTest, TestRequestCanceled) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  ExecuteModel(ModelBasedCapabilityKey::kWallpaperSearch,
               BuildTestMessage("foo request"));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  VerifyHasPendingFetchRequest();

  model_execution_fetcher_.reset();
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "WallpaperSearch",
      FetcherRequestStatus::kRequestCanceled, 1);
  EXPECT_EQ(ModelExecutionError::kCancelled,
            last_execute_response_->error().error());
}

TEST_F(ModelExecutionFetcherTest, TestMultipleParallelRequests) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);
  ExecuteModel(ModelBasedCapabilityKey::kWallpaperSearch,
               BuildTestMessage("foo request"));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  ExecuteModel(ModelBasedCapabilityKey::kTabOrganization,
               BuildTestMessage("foo request"));

  // The second request should fail immediately.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "TabOrganization",
      FetcherRequestStatus::kFetcherBusy, 1);
  EXPECT_EQ(ModelExecutionError::kGenericFailure,
            last_execute_response_->error().error());

  VerifyHasPendingFetchRequest();
  SimulateSuccessfulResponse(
      BuildTestExecuteResponse(BuildTestMessage("foo response")));

  // Only the first request should succeed.
  histogram_tester_.ExpectBucketCount(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "WallpaperSearch",
      FetcherRequestStatus::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.Status", net::HTTP_OK, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.FetchLatency.WallpaperSearch",
      1);
}

TEST_F(ModelExecutionFetcherTest, TestSuccessfulResponseWithLogin) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);

  ExecuteModel(ModelBasedCapabilityKey::kWallpaperSearch,
               BuildTestMessage("foo request"));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
  VerifyHasPendingFetchRequest();

  SimulateSuccessfulResponse(
      BuildTestExecuteResponse(BuildTestMessage("foo response")));

  EXPECT_EQ(
      proto::ModelExecutionFeature::MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH,
      last_execute_request_->feature());
  EXPECT_EQ("type.googleapis.com/base.test.TestMessage",
            last_execute_request_->request_metadata().type_url());
  EXPECT_EQ("type.googleapis.com/base.test.TestMessage",
            last_execute_response_->value().response_metadata().type_url());
  EXPECT_EQ("foo request", ParsedAnyMetadata<TestMessage>(
                               last_execute_request_->request_metadata())
                               ->test());
  EXPECT_EQ("foo response",
            ParsedAnyMetadata<TestMessage>(
                last_execute_response_->value().response_metadata())
                ->test());

  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.FetchLatency.WallpaperSearch",
      1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.NetErrorCode", -net::OK, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecutionFetcher.Status", 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "WallpaperSearch",
      FetcherRequestStatus::kSuccess, 1);
}

TEST_F(ModelExecutionFetcherTest, TestAccessTokenFailureWithLogin) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "test_email", signin::ConsentLevel::kSignin);

  ExecuteModel(ModelBasedCapabilityKey::kWallpaperSearch,
               BuildTestMessage("foo request"));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));

  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "WallpaperSearch",
      FetcherRequestStatus::kUserNotSignedIn, 1);
  EXPECT_EQ(ModelExecutionError::kPermissionDenied,
            last_execute_response_->error().error());
}

TEST_F(ModelExecutionFetcherTest, TestNoUserSignIn) {
  ExecuteModel(ModelBasedCapabilityKey::kWallpaperSearch,
               BuildTestMessage("foo request"));

  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus."
      "WallpaperSearch",
      FetcherRequestStatus::kUserNotSignedIn, 1);
  EXPECT_EQ(ModelExecutionError::kPermissionDenied,
            last_execute_response_->error().error());
}

}  // namespace optimization_guide
