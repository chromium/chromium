// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl_test_api.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/features.h"
#include "google_apis/common/api_key_request_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

constexpr char kTestApiUrl[] = "https://api.googleapis.com/test";
constexpr char kTestApiUrl2[] = "https://api.googleapis.com/test2";
constexpr char kTestApiBody[] = "{\"test_body\": true}";
constexpr char kTestFakeSuccessResponse[] = "{\"status\": \"ok\"}";
constexpr char kTestCandidateId[] = "12345678-1234-4678-a234-567812345678";

std::unique_ptr<network::ResourceRequest> CreateRequest(
    const std::string& url) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL(url);
  request->method = "POST";
  return request;
}

class AnnotationIndexClientImplTest : public testing::Test {
 public:
  AnnotationIndexClientImplTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        client_(AnnotationIndexClientImplTestApi::CreateManagerForApiKey(
            test_shared_loader_factory_,
            "dummykey")) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kMultistepFilter,
        {{kMultistepFilterIndexServerApiBaseUrl.name, kTestApiUrl}});
  }

  ~AnnotationIndexClientImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<AnnotationIndexClientImpl> client_;
};

TEST_F(AnnotationIndexClientImplTest, ExecuteRequest_Success) {
  base::test::TestFuture<std::optional<std::string>> future;

  test_api(*client_).ExecuteRequest(
      CreateRequest(kTestApiUrl), std::string(kTestApiBody),
      TRAFFIC_ANNOTATION_FOR_TESTS, future.GetCallback());
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(google_apis::test_util::GetAPIKeyFromRequest(request->request),
            "dummykey");

  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, kTestFakeSuccessResponse);

  std::optional<std::string> result = future.Take();

  ASSERT_TRUE(result);
  EXPECT_EQ(*result, kTestFakeSuccessResponse);
}

TEST_F(AnnotationIndexClientImplTest, ExecuteRequest_MultipleRequests) {
  base::test::TestFuture<std::optional<std::string>> future1;
  base::test::TestFuture<std::optional<std::string>> future2;

  test_api(*client_).ExecuteRequest(
      CreateRequest(kTestApiUrl), std::string(kTestApiBody),
      TRAFFIC_ANNOTATION_FOR_TESTS, future1.GetCallback());
  test_api(*client_).ExecuteRequest(
      CreateRequest(kTestApiUrl2), std::string(kTestApiBody),
      TRAFFIC_ANNOTATION_FOR_TESTS, future2.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);

  network::TestURLLoaderFactory::PendingRequest* request1 =
      test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request1, kTestFakeSuccessResponse);

  network::TestURLLoaderFactory::PendingRequest* request2 =
      test_url_loader_factory_.GetPendingRequest(1);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request2, kTestFakeSuccessResponse);

  std::optional<std::string> result1 = future1.Take();
  ASSERT_TRUE(result1);
  EXPECT_EQ(*result1, kTestFakeSuccessResponse);

  std::optional<std::string> result2 = future2.Take();
  ASSERT_TRUE(result2);
  EXPECT_EQ(*result2, kTestFakeSuccessResponse);
}

TEST_F(AnnotationIndexClientImplTest, ExecuteRequest_NetworkError) {
  base::test::TestFuture<std::optional<std::string>> future;

  test_api(*client_).ExecuteRequest(
      CreateRequest(kTestApiUrl), std::string(kTestApiBody),
      TRAFFIC_ANNOTATION_FOR_TESTS, future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  std::optional<std::string> result = future.Take();

  EXPECT_FALSE(result.has_value());
}

TEST_F(AnnotationIndexClientImplTest, ExecuteRequest_HttpError) {
  base::test::TestFuture<std::optional<std::string>> future;

  test_api(*client_).ExecuteRequest(
      CreateRequest(kTestApiUrl), std::string(kTestApiBody),
      TRAFFIC_ANNOTATION_FOR_TESTS, future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 1);

  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, network::CreateURLResponseHead(net::HTTP_NOT_FOUND), "Not Found",
      network::URLLoaderCompletionStatus(net::OK));

  std::optional<std::string> result = future.Take();

  EXPECT_FALSE(result.has_value());
}

TEST_F(AnnotationIndexClientImplTest, GetSupportedTaskTypesForDomain_Success) {
  GetSupportedTasksResponse proto_response;
  proto_response.add_supported_tasks()->set_task_type("TASK1");
  proto_response.add_supported_tasks()->set_task_type("TASK2");

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain("example.com", future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, proto_response.SerializeAsString());

  std::optional<std::vector<std::string>> result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->size(), 2u);
  EXPECT_EQ((*result)[0], "TASK1");
  EXPECT_EQ((*result)[1], "TASK2");
}

TEST_F(AnnotationIndexClientImplTest, GetFilterSuggestionCandidates_Success) {
  GetTaskExecutionStrategiesResponse proto_response;
  TaskExecutionStrategy* strategy = proto_response.add_execution_strategies();
  strategy->set_candidate_id(kTestCandidateId);
  AppliedFilterUIString* filter1 = strategy->add_applied_filters();
  filter1->set_key("PRICE_MIN");
  filter1->set_label("Min Price");
  strategy->mutable_execution()->mutable_url_navigation()->set_navigation_url(
      "https://travel.com/flights?min=100");

  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;

  std::vector<FilterAnnotation> annotations;
  client_->GetFilterSuggestionCandidates(GURL("https://example.com/test"),
                                         annotations, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, proto_response.SerializeAsString());

  std::optional<std::vector<FilterSuggestionCandidate>> result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].filter_annotation_id.AsLowercaseString(),
            kTestCandidateId);
  EXPECT_EQ((*result)[0].navigation_url.spec(),
            "https://travel.com/flights?min=100");
}

TEST_F(AnnotationIndexClientImplTest, ExtractFilterAnnotation_Success) {
  ExtractTaskAttributesResponse proto_response;
  proto_response.set_domain("example.com");
  proto_response.set_task_type("SEARCH_FLIGHTS");
  TaskAttribute* attr = proto_response.add_task_attributes();
  attr->set_key("PRICE_MIN");
  attr->set_value("100");

  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL("https://example.com/path?q=1"),
                                   future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      request, proto_response.SerializeAsString());

  std::optional<FilterAnnotation> result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->task_type, "SEARCH_FLIGHTS");
  EXPECT_EQ(result->source_domain, "example.com");
  ASSERT_EQ(result->attributes.size(), 1u);
  EXPECT_EQ(result->attributes[0].key, "PRICE_MIN");
  EXPECT_EQ(result->attributes[0].value, "100");
}

}  // namespace

}  // namespace multistep_filter
