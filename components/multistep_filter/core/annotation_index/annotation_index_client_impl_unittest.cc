// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl_test_api.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_test_utils.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/switches.h"
#include "google_apis/common/api_key_request_test_util.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

constexpr char kTestApiUrl[] = "https://api.googleapis.com/test/";
constexpr char kTestNonGoogleApiUrl[] = "https://api.example.com/test/";
constexpr char kTestSwitchApiUrl[] = "https://switch.example.com/api/";
constexpr char kTestInvalidUrl[] = "invalid_url";
constexpr char kDummyApiKey[] = "dummykey";
constexpr char kTestUrl[] = "https://example.com/test";
constexpr char kTestExtractUrl[] = "https://example.com/path?q=1";
constexpr char kTestSuggestionUrl[] = "https://travel.com/flights?min=100";
constexpr char kTestCandidateId[] = "12345678-1234-5678-1234-567812345678";
constexpr char kTestDomain[] = "example.com";
constexpr char kTestDomain1[] = "example1.com";
constexpr char kTestDomain2[] = "example2.com";
constexpr char kTask1[] = "TASK1";
constexpr char kTask2[] = "TASK2";
constexpr char kTestTaskType[] = "SEARCH_FLIGHTS";
constexpr char kTestAttributeKey[] = "PRICE_MIN";
constexpr char kTestAttributeValue[] = "100";
constexpr char kTestAttributeLabel[] = "Min Price";

// Extracts the string payload from a URLLoader request body.
std::string GetStringFromDataElements(
    const std::vector<network::DataElement>* data_elements) {
  std::string result;
  for (const network::DataElement& e : *data_elements) {
    if (e.type() == network::DataElement::Tag::kBytes) {
      result.append(e.As<network::DataElementBytes>().AsStringPiece());
    }
  }
  return result;
}

// Helper to deserialize the uploaded proto from a pending request.
template <typename RequestProto>
bool GetRequestProtoFromPendingRequest(
    const network::TestURLLoaderFactory::PendingRequest* request,
    RequestProto* out_proto) {
  if (!request || !request->request.request_body) {
    return false;
  }
  std::string body_content =
      GetStringFromDataElements(request->request.request_body->elements());
  return out_proto->ParseFromString(body_content);
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

  void TearDown() override {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kMultistepFilterIndexServerApiBaseUrl);
  }

 protected:
  void OverrideBaseUrlWithSwitch(const std::string& base_url) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMultistepFilterIndexServerApiBaseUrl, base_url);
  }

  void SimulateHttpError(network::TestURLLoaderFactory::PendingRequest* request,
                         net::HttpStatusCode status) {
    test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
        request, network::CreateURLResponseHead(status), "Server Error",
        network::URLLoaderCompletionStatus(net::OK));
  }

  void SimulateNetworkError(
      network::TestURLLoaderFactory::PendingRequest* request,
      int error_code = net::ERR_FAILED) {
    test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
        request, network::mojom::URLResponseHead::New(), std::string(),
        network::URLLoaderCompletionStatus(error_code));
  }

  void SimulateInvalidResponse(
      network::TestURLLoaderFactory::PendingRequest* request) {
    test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
        request, "not a valid proto");
  }

  void SimulateTimeout(network::TestURLLoaderFactory::PendingRequest* request) {
    SimulateNetworkError(request, net::ERR_TIMED_OUT);
  }

  void SimulateEmptyResponse(
      network::TestURLLoaderFactory::PendingRequest* request) {
    test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
        request, "");
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<AnnotationIndexClientImpl> client_;
};

TEST_F(AnnotationIndexClientImplTest,
       GetFilterSuggestionCandidates_Success_ReturnsCandidates) {
  GetTaskExecutionStrategiesResponse proto_response =
      CreateTaskExecutionStrategiesResponse(
          GURL(kTestSuggestionUrl), {{kTestAttributeKey, kTestAttributeLabel}});
  proto_response.mutable_execution_strategies(0)->set_candidate_id(
      kTestCandidateId);
  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  std::vector<FilterAnnotation> annotations;

  client_->GetFilterSuggestionCandidates(GURL(kTestUrl), annotations,
                                         future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(),
            std::string(kTestApiUrl) + "GetTaskExecutionStrategies");
  EXPECT_EQ(
      google_apis::test_util::GetAPIKeyFromRequest(pending_request->request),
      kDummyApiKey);
  GetTaskExecutionStrategiesRequest request_proto;
  ASSERT_TRUE(
      GetRequestProtoFromPendingRequest(pending_request, &request_proto));
  EXPECT_EQ(request_proto.current_url(), kTestUrl);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      pending_request, proto_response.SerializeAsString());
  std::optional<std::vector<FilterSuggestionCandidate>> result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ((*result)[0].filter_annotation_id.AsLowercaseString(),
            kTestCandidateId);
  EXPECT_EQ((*result)[0].navigation_url.spec(), kTestSuggestionUrl);
}

TEST_F(AnnotationIndexClientImplTest,
       GetFilterSuggestionCandidates_HttpError_ReturnsNullopt) {
  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  std::vector<FilterAnnotation> annotations;

  client_->GetFilterSuggestionCandidates(GURL(kTestUrl), annotations,
                                         future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateHttpError(test_url_loader_factory_.GetPendingRequest(0),
                    net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       GetFilterSuggestionCandidates_NetworkError_ReturnsNullopt) {
  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  std::vector<FilterAnnotation> annotations;

  client_->GetFilterSuggestionCandidates(GURL(kTestUrl), annotations,
                                         future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateNetworkError(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       GetFilterSuggestionCandidates_InvalidResponse_ReturnsNullopt) {
  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  std::vector<FilterAnnotation> annotations;

  client_->GetFilterSuggestionCandidates(GURL(kTestUrl), annotations,
                                         future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateInvalidResponse(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       GetFilterSuggestionCandidates_EmptyResponse_ReturnsEmptyVector) {
  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  std::vector<FilterAnnotation> annotations;

  client_->GetFilterSuggestionCandidates(GURL(kTestUrl), annotations,
                                         future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateEmptyResponse(test_url_loader_factory_.GetPendingRequest(0));
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(AnnotationIndexClientImplTest,
       GetFilterSuggestionCandidates_Timeout_ReturnsNullopt) {
  base::test::TestFuture<std::optional<std::vector<FilterSuggestionCandidate>>>
      future;
  std::vector<FilterAnnotation> annotations;

  client_->GetFilterSuggestionCandidates(GURL(kTestUrl), annotations,
                                         future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateTimeout(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       GetSupportedTaskTypesForDomain_Success_ReturnsTaskTypes) {
  GetSupportedTasksResponse proto_response =
      CreateSupportedTasksResponse({kTask1, kTask2});
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(),
            std::string(kTestApiUrl) + "GetSupportedTasks");
  EXPECT_EQ(
      google_apis::test_util::GetAPIKeyFromRequest(pending_request->request),
      kDummyApiKey);
  GetSupportedTasksRequest request_proto;
  ASSERT_TRUE(
      GetRequestProtoFromPendingRequest(pending_request, &request_proto));
  EXPECT_EQ(request_proto.domain(), kTestDomain);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      pending_request, proto_response.SerializeAsString());
  std::optional<std::vector<std::string>> result = future.Take();
  ASSERT_TRUE(result);
  ASSERT_EQ(result->size(), 2u);
  EXPECT_EQ((*result)[0], kTask1);
  EXPECT_EQ((*result)[1], kTask2);
}

TEST_F(AnnotationIndexClientImplTest,
       GetSupportedTaskTypesForDomain_HttpError_ReturnsNullopt) {
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateHttpError(test_url_loader_factory_.GetPendingRequest(0),
                    net::HTTP_NOT_FOUND);
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       GetSupportedTaskTypesForDomain_NetworkError_ReturnsNullopt) {
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateNetworkError(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       GetSupportedTaskTypesForDomain_InvalidResponse_ReturnsNullopt) {
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateInvalidResponse(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       GetSupportedTaskTypesForDomain_EmptyResponse_ReturnsEmptyVector) {
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateEmptyResponse(test_url_loader_factory_.GetPendingRequest(0));
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(AnnotationIndexClientImplTest,
       GetSupportedTaskTypesForDomain_Timeout_ReturnsNullopt) {
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateTimeout(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       ExtractFilterAnnotation_Success_ReturnsAnnotation) {
  ExtractTaskAttributesResponse proto_response =
      CreateExtractTaskAttributesResponse(
          kTestDomain, kTestTaskType,
          {{kTestAttributeKey, kTestAttributeValue}});
  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL(kTestExtractUrl), future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url.spec(),
            std::string(kTestApiUrl) + "ExtractTaskAttributes");
  EXPECT_EQ(
      google_apis::test_util::GetAPIKeyFromRequest(pending_request->request),
      kDummyApiKey);
  ExtractTaskAttributesRequest request_proto;
  ASSERT_TRUE(
      GetRequestProtoFromPendingRequest(pending_request, &request_proto));
  EXPECT_EQ(request_proto.source().raw_url(), kTestExtractUrl);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      pending_request, proto_response.SerializeAsString());
  std::optional<FilterAnnotation> result = future.Take();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->task_type, kTestTaskType);
  EXPECT_EQ(result->source_domain, kTestDomain);
  ASSERT_EQ(result->attributes.size(), 1u);
  EXPECT_EQ(result->attributes[0].key, kTestAttributeKey);
  EXPECT_EQ(result->attributes[0].value, kTestAttributeValue);
}

TEST_F(AnnotationIndexClientImplTest,
       ExtractFilterAnnotation_HttpError_ReturnsNullopt) {
  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL(kTestExtractUrl), future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateHttpError(test_url_loader_factory_.GetPendingRequest(0),
                    net::HTTP_INTERNAL_SERVER_ERROR);
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       ExtractFilterAnnotation_NetworkError_ReturnsNullopt) {
  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL(kTestExtractUrl), future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateNetworkError(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       ExtractFilterAnnotation_InvalidResponse_ReturnsNullopt) {
  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL(kTestExtractUrl), future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateInvalidResponse(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       ExtractFilterAnnotation_EmptyResponse_ReturnsNullopt) {
  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL(kTestExtractUrl), future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateEmptyResponse(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest,
       ExtractFilterAnnotation_Timeout_ReturnsNullopt) {
  base::test::TestFuture<std::optional<FilterAnnotation>> future;

  client_->ExtractFilterAnnotation(GURL(kTestExtractUrl), future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateTimeout(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest, ApiKeyIncludedInRequest) {
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(
      google_apis::test_util::GetAPIKeyFromRequest(pending_request->request),
      kDummyApiKey);
}

TEST_F(AnnotationIndexClientImplTest, NoApiKeyForNonGoogleDomains) {
  OverrideBaseUrlWithSwitch(kTestNonGoogleApiUrl);
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_FALSE(google_apis::test_util::HasAPIKey(pending_request->request));
}

TEST_F(AnnotationIndexClientImplTest, BaseUrlOverriddenBySwitch) {
  OverrideBaseUrlWithSwitch(kTestSwitchApiUrl);
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_TRUE(
      pending_request->request.url.spec().starts_with(kTestSwitchApiUrl));
}

TEST_F(AnnotationIndexClientImplTest, InvalidBaseUrlFailsQuickly) {
  OverrideBaseUrlWithSwitch(kTestInvalidUrl);
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_FALSE(future.Take().has_value());
}

TEST_F(AnnotationIndexClientImplTest, HandlesConcurrentRequests) {
  GetSupportedTasksResponse proto_response1 =
      CreateSupportedTasksResponse({kTask1});
  GetSupportedTasksResponse proto_response2 =
      CreateSupportedTasksResponse({kTask2});
  base::test::TestFuture<std::optional<std::vector<std::string>>> future1;
  base::test::TestFuture<std::optional<std::vector<std::string>>> future2;

  client_->GetSupportedTaskTypesForDomain(kTestDomain1, future1.GetCallback());
  client_->GetSupportedTaskTypesForDomain(kTestDomain2, future2.GetCallback());

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 2);
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      test_url_loader_factory_.GetPendingRequest(0),
      proto_response1.SerializeAsString());
  test_url_loader_factory_.SimulateResponseWithoutRemovingFromPendingList(
      test_url_loader_factory_.GetPendingRequest(1),
      proto_response2.SerializeAsString());
  auto result1 = future1.Take();
  ASSERT_TRUE(result1);
  ASSERT_EQ(result1->size(), 1u);
  EXPECT_EQ((*result1)[0], kTask1);
  auto result2 = future2.Take();
  ASSERT_TRUE(result2);
  ASSERT_EQ(result2->size(), 1u);
  EXPECT_EQ((*result2)[0], kTask2);
}

TEST_F(AnnotationIndexClientImplTest, LoaderCleanedUpAfterCompletion) {
  base::test::TestFuture<std::optional<std::vector<std::string>>> future;

  client_->GetSupportedTaskTypesForDomain(kTestDomain, future.GetCallback());

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  SimulateEmptyResponse(test_url_loader_factory_.GetPendingRequest(0));
  EXPECT_TRUE(future.Take().has_value());
}

}  // namespace

}  // namespace multistep_filter
