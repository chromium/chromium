// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/get_operation_request.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/proto/offline_pages.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace offline_pages {

namespace {
const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;

// Operations include part of the path in the operation name.
const char kTestOperationName[] = "operations/test-operation-1234";

// This is hard coded so we can express the known URL structure for operations.
const char kServerPathForTestOperation[] = "/v1/operations/test-operation-1234";

}  // namespace

// All tests cases here only validate the request data and check for general
// http response. The tests for the Operation proto data returned in the http
// response are covered in PrefetchRequestOperationResponseTest.
class GetOperationRequestTest : public PrefetchRequestTestBase {
 public:
  std::unique_ptr<GetOperationRequest> CreateRequest(
      PrefetchRequestFinishedCallback callback) {
    return std::make_unique<GetOperationRequest>(
        kTestOperationName, kTestChannel, shared_url_loader_factory(),
        std::move(callback));
  }
};

TEST_F(GetOperationRequestTest, RequestData) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::unique_ptr<GetOperationRequest> request(CreateRequest(callback.Get()));

  auto* pending_request = GetPendingRequest();
  DCHECK(pending_request);
  GURL request_url = pending_request->request.url;
  EXPECT_TRUE(request_url.SchemeIs(url::kHttpsScheme));
  EXPECT_EQ(kServerPathForTestOperation, request_url.path());
  EXPECT_TRUE(base::StartsWith(request_url.query(), "key",
                               base::CompareCase::SENSITIVE));

  net::HttpRequestHeaders headers = pending_request->request.headers;
  std::string content_type_header;
  headers.GetHeader(net::HttpRequestHeaders::kContentType,
                    &content_type_header);
  EXPECT_EQ("application/x-protobuf", content_type_header);

  // Experiment header should not be set.
  EXPECT_EQ("", GetExperiementHeaderValue(pending_request));

  std::string upload_content_type;
  pending_request->request.headers.GetHeader(
      net::HttpRequestHeaders::kContentType, &upload_content_type);
  EXPECT_FALSE(upload_content_type.empty());
  EXPECT_TRUE(network::GetUploadData(pending_request->request).empty());
}

TEST_F(GetOperationRequestTest, ExperimentHeaderInRequestData) {
  // Add the experiment option in the field trial.
  SetUpExperimentOption();

  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::unique_ptr<GetOperationRequest> request(CreateRequest(callback.Get()));
  auto* pending_request = GetPendingRequest();
  DCHECK(pending_request);

  // Experiment header should be set.
  EXPECT_EQ(kExperimentValueSetInFieldTrial,
            GetExperiementHeaderValue(pending_request));
}

TEST_F(GetOperationRequestTest, EmptyResponse) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::unique_ptr<GetOperationRequest> request(CreateRequest(callback.Get()));

  PrefetchRequestStatus status;
  std::string operation_name;
  std::vector<RenderPageInfo> pages;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&operation_name),
                      SaveArg<2>(&pages)));
  RespondWithData("");
  RunUntilIdle();

  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff, status);
  EXPECT_EQ(std::string(kTestOperationName), operation_name);
  EXPECT_TRUE(pages.empty());
}

TEST_F(GetOperationRequestTest, InvalidResponse) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::unique_ptr<GetOperationRequest> request(CreateRequest(callback.Get()));

  PrefetchRequestStatus status;
  std::string operation_name;
  std::vector<RenderPageInfo> pages;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&operation_name),
                      SaveArg<2>(&pages)));
  RespondWithData("Some invalid data");
  RunUntilIdle();

  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff, status);
  EXPECT_EQ(std::string(kTestOperationName), operation_name);
  EXPECT_TRUE(pages.empty());
}

}  // namespace offline_pages
