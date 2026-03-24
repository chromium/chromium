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
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl_test_api.h"
#include "components/multistep_filter/core/annotation_index/proto/annotation_index.pb.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
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
            "dummykey")) {}

  ~AnnotationIndexClientImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
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

}  // namespace

}  // namespace multistep_filter
