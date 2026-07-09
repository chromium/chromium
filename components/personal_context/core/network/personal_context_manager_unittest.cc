// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/network/personal_context_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/network/personal_context_fetcher.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace personal_context {

namespace {

using ::base::test::TestMessage;

class RemoteResponseHolder {
 public:
  RemoteResponseHolder() = default;
  ~RemoteResponseHolder() = default;

  FetchContextCallback GetCallback() {
    CHECK(!weak_ptr_factory_.HasWeakPtrs());  // Shouldn't be reused.
    return base::BindOnce(&RemoteResponseHolder::OnResponse,
                          weak_ptr_factory_.GetWeakPtr());
  }

  bool GetFinalStatus() { return future_.Get(); }

  template <typename T>
  T GetOutput() const {
    T result;
    result.ParseFromString(result_->response.value().value());
    return result;
  }

  ContextMemoryError::ExecutionError error() const {
    return result_->response.error().error();
  }

  std::string server_request_id() const { return result_->server_request_id; }

 private:
  void OnResponse(FetchContextResult result) {
    result_.emplace(std::move(result));
    future_.SetValue(result_->response.has_value());
  }

  base::test::TestFuture<bool> future_;
  std::optional<FetchContextResult> result_;
  base::WeakPtrFactory<RemoteResponseHolder> weak_ptr_factory_{this};
};

class PiiResponseHolder {
 public:
  PiiResponseHolder() = default;
  ~PiiResponseHolder() = default;

  FetchPiiContextCallback GetCallback() {
    CHECK(!weak_ptr_factory_.HasWeakPtrs());
    return base::BindOnce(&PiiResponseHolder::OnResponse,
                          weak_ptr_factory_.GetWeakPtr());
  }

  bool GetFinalStatus() { return future_.Get(); }

  const proto::FetchPiiEntitiesResponse& response() const {
    CHECK(result_->response.has_value());
    return result_->response.value();
  }

  ContextMemoryError::ExecutionError error() const {
    CHECK(!result_->response.has_value());
    return result_->response.error().error();
  }

 private:
  void OnResponse(FetchPiiEntitiesResult result) {
    result_.emplace(std::move(result));
    future_.SetValue(result_->response.has_value());
  }

  base::test::TestFuture<bool> future_;
  std::optional<FetchPiiEntitiesResult> result_;
  base::WeakPtrFactory<PiiResponseHolder> weak_ptr_factory_{this};
};

proto::FetchContextResponse BuildFetchContextResponse(std::string_view output) {
  proto::FetchContextResponse fetch_response;
  proto::Any* any_metadata = fetch_response.mutable_response_metadata();
  any_metadata->set_type_url("type.googleapis.com/test.Message");
  any_metadata->set_value(output);
  return fetch_response;
}

class PersonalContextManagerTest : public testing::Test {
 public:
  PersonalContextManagerTest() = default;
  ~PersonalContextManagerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kPersonalContext);
    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    personal_context_manager_ = std::make_unique<PersonalContextManager>(
        url_loader_factory_, identity_test_env_.identity_manager());
  }

  bool SimulateResponse(std::string_view content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        "https://contextmemoryservice.pa.googleapis.com/v1:fetchContext",
        std::string(content), http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  bool SimulateSuccessfulResponse() {
    TestMessage test_message;
    test_message.set_test("foo response");
    std::string serialized_message;
    test_message.SerializeToString(&serialized_message);

    std::string serialized_response;
    proto::FetchContextResponse fetch_response =
        BuildFetchContextResponse(serialized_message);
    fetch_response.set_server_request_id("test_id");
    fetch_response.SerializeToString(&serialized_response);
    return SimulateResponse(serialized_response, net::HTTP_OK);
  }

  bool SimulatePiiResponse(std::string_view content,
                           net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        "https://contextmemoryservice.pa.googleapis.com/v1:fetchPiiEntities",
        std::string(content), http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  bool SimulateSuccessfulPiiResponse() {
    proto::FetchPiiEntitiesResponse pii_response;
    pii_response.set_server_request_id("test_id");
    std::string serialized_response;
    pii_response.SerializeToString(&serialized_response);
    return SimulatePiiResponse(serialized_response, net::HTTP_OK);
  }

  void SetAutomaticIssueOfAccessTokens() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test_email", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  PersonalContextManager* personal_context_manager() {
    return personal_context_manager_.get();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<PersonalContextManager> personal_context_manager_;
};

TEST_F(PersonalContextManagerTest, FetchContextEmptyAccessToken) {
  base::HistogramTester histogram_tester;
  RemoteResponseHolder response_holder;
  personal_context_manager()->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, TestMessage(),
      /*timeout=*/std::nullopt, response_holder.GetCallback());
  EXPECT_FALSE(response_holder.GetFinalStatus());
  EXPECT_EQ(ContextMemoryError::ExecutionError::kPermissionDenied,
            response_holder.error());

  // Check that the result histogram records failure.
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchContext.Result.AmbientAutofill", /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_F(PersonalContextManagerTest, FetchContextWithUserSignIn) {
  base::HistogramTester histogram_tester;
  RemoteResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  personal_context_manager()->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, TestMessage(),
      /*timeout=*/std::nullopt, response_holder.GetCallback());

  EXPECT_TRUE(SimulateSuccessfulResponse());
  EXPECT_TRUE(response_holder.GetFinalStatus());
  EXPECT_EQ("foo response", response_holder.GetOutput<TestMessage>().test());
  EXPECT_EQ("test_id", response_holder.server_request_id());

  // Check that the result histogram records success.
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchContext.Result.AmbientAutofill", /*sample=*/true,
      /*expected_bucket_count=*/1);

  // Check that the latency was recorded.
  histogram_tester.ExpectTotalCount(
      "PersonalContext.FetchContext.Latency.AmbientAutofill",
      /*expected_count=*/1);
}

// Tests that when a new request is issued and the total number of active
// requests would exceed the maximum for this feature, the oldest request is
// cancelled.
TEST_F(PersonalContextManagerTest, MultipleParallelRequestsLimit) {
  base::HistogramTester histogram_tester;
  RemoteResponseHolder response_holder1, response_holder2;

  SetAutomaticIssueOfAccessTokens();

  personal_context_manager()->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, TestMessage(),
      /*timeout=*/std::nullopt, response_holder1.GetCallback());

  personal_context_manager()->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, TestMessage(),
      /*timeout=*/std::nullopt, response_holder2.GetCallback());

  test_url_loader_factory()->EraseResponse(
      GURL("https://contextmemoryservice.pa.googleapis.com/v1:fetchContext"));
  EXPECT_TRUE(SimulateSuccessfulResponse());

  EXPECT_TRUE(response_holder2.GetFinalStatus());

  EXPECT_FALSE(response_holder1.GetFinalStatus());
  EXPECT_EQ(ContextMemoryError::ExecutionError::kCancelled,
            response_holder1.error());

  // The cancelled request should record a failure and client cancellation
  // error. The second (successful) request should record success.
  histogram_tester.ExpectTotalCount(
      "PersonalContext.FetchContext.Result.AtMemory",
      /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      "PersonalContext.FetchContext.Result.AtMemory", /*sample=*/false,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "PersonalContext.FetchContext.Result.AtMemory", /*sample=*/true,
      /*expected_count=*/1);

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchContext.ErrorStatus.AtMemory",
      ContextMemoryError::ExecutionError::kCancelled,
      /*expected_bucket_count=*/1);
}

TEST_F(PersonalContextManagerTest, FetchPiiEntitiesEmptyAccessToken) {
  base::HistogramTester histogram_tester;
  PiiResponseHolder response_holder;
  proto::FetchPiiEntitiesRequest request;
  request.set_feature(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);
  personal_context_manager()->FetchPiiEntities(
      request, /*timeout=*/std::nullopt, response_holder.GetCallback());
  EXPECT_FALSE(response_holder.GetFinalStatus());
  EXPECT_EQ(ContextMemoryError::ExecutionError::kPermissionDenied,
            response_holder.error());

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchPiiEntities.Result.AmbientAutofill",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchPiiEntities.ErrorStatus.AmbientAutofill",
      ContextMemoryError::ExecutionError::kPermissionDenied,
      /*expected_bucket_count=*/1);
}

TEST_F(PersonalContextManagerTest, FetchPiiEntitiesWithUserSignIn) {
  base::HistogramTester histogram_tester;
  PiiResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  proto::FetchPiiEntitiesRequest request;
  request.set_feature(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);
  personal_context_manager()->FetchPiiEntities(
      request, /*timeout=*/std::nullopt, response_holder.GetCallback());
  EXPECT_TRUE(SimulateSuccessfulPiiResponse());
  EXPECT_TRUE(response_holder.GetFinalStatus());
  EXPECT_EQ("test_id", response_holder.response().server_request_id());

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchPiiEntities.Result.AmbientAutofill",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.FetchPiiEntities.Latency.AmbientAutofill",
      /*expected_count=*/1);
}

TEST_F(PersonalContextManagerTest, FetchPiiEntitiesServerError) {
  base::HistogramTester histogram_tester;
  PiiResponseHolder response_holder;
  SetAutomaticIssueOfAccessTokens();
  proto::FetchPiiEntitiesRequest request;
  request.set_feature(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);
  personal_context_manager()->FetchPiiEntities(
      request, /*timeout=*/std::nullopt, response_holder.GetCallback());

  EXPECT_TRUE(SimulatePiiResponse("error", net::HTTP_INTERNAL_SERVER_ERROR));
  EXPECT_FALSE(response_holder.GetFinalStatus());

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchPiiEntities.Result.AmbientAutofill",
      /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchPiiEntities.ErrorStatus.AmbientAutofill",
      ContextMemoryError::ExecutionError::kGenericFailure,
      /*expected_bucket_count=*/1);
}

TEST_F(PersonalContextManagerTest,
       FetchPiiEntitiesMultipleParallelRequestsLimit) {
  base::HistogramTester histogram_tester;
  PiiResponseHolder response_holder1, response_holder2;

  SetAutomaticIssueOfAccessTokens();

  proto::FetchPiiEntitiesRequest request;
  request.set_feature(proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL);

  personal_context_manager()->FetchPiiEntities(
      request, /*timeout=*/std::nullopt, response_holder1.GetCallback());

  personal_context_manager()->FetchPiiEntities(
      request, /*timeout=*/std::nullopt, response_holder2.GetCallback());

  test_url_loader_factory()->EraseResponse(
      GURL("https://example.com/v1:fetchPiiEntities"));
  EXPECT_TRUE(SimulateSuccessfulPiiResponse());

  EXPECT_TRUE(response_holder2.GetFinalStatus());

  EXPECT_FALSE(response_holder1.GetFinalStatus());
  EXPECT_EQ(ContextMemoryError::ExecutionError::kCancelled,
            response_holder1.error());

  histogram_tester.ExpectTotalCount(
      "PersonalContext.FetchPiiEntities.Result.AmbientAutofill",
      /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      "PersonalContext.FetchPiiEntities.Result.AmbientAutofill",
      /*sample=*/false,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "PersonalContext.FetchPiiEntities.Result.AmbientAutofill",
      /*sample=*/true,
      /*expected_count=*/1);

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.FetchPiiEntities.ErrorStatus.AmbientAutofill",
      ContextMemoryError::ExecutionError::kCancelled,
      /*expected_bucket_count=*/1);
}

}  // namespace

}  // namespace personal_context
