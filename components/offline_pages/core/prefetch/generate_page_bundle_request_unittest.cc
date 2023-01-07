// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"

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
using testing::Contains;
using testing::DoAll;
using testing::Eq;
using testing::Not;
using testing::SaveArg;

namespace offline_pages {

namespace {
const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;
const char kTestURL[] = "http://example.com";
const char kTestURL2[] = "http://example.com/2";
const char kTestUserAgent[] = "Test User Agent";
const char kTestGCMID[] = "Test GCM ID";
const int kTestMaxBundleSize = 100000;
}  // namespace

// All tests cases here only validate the request data and check for general
// http response. The tests for the Operation proto data returned in the http
// response are covered in PrefetchRequestOperationResponseTest.
class GeneratePageBundleRequestTest : public PrefetchRequestTestBase {
 public:
  std::unique_ptr<GeneratePageBundleRequest> CreateRequest(
      const std::string& testing_header_value,
      PrefetchRequestFinishedCallback callback) {
    std::vector<std::string> page_urls = {kTestURL, kTestURL2};
    return std::make_unique<GeneratePageBundleRequest>(
        kTestUserAgent, kTestGCMID, kTestMaxBundleSize, page_urls, kTestChannel,
        testing_header_value, shared_url_loader_factory(), std::move(callback));
  }
};

TEST_F(GeneratePageBundleRequestTest, RequestData) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::unique_ptr<GeneratePageBundleRequest> request(
      CreateRequest("", callback.Get()));

  EXPECT_EQ(2UL, request->requested_urls().size());
  EXPECT_THAT(request->requested_urls(), Contains(kTestURL));
  EXPECT_THAT(request->requested_urls(), Contains(kTestURL2));

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  DCHECK(pending_request);
  GURL request_url = pending_request->request.url;
  EXPECT_TRUE(request_url.SchemeIs(url::kHttpsScheme));
  EXPECT_TRUE(base::StartsWith(request_url.query(), "key",
                               base::CompareCase::SENSITIVE));

  std::string upload_content_type;
  pending_request->request.headers.GetHeader(
      net::HttpRequestHeaders::kContentType, &upload_content_type);
  EXPECT_FALSE(upload_content_type.empty());
  EXPECT_FALSE(network::GetUploadData(pending_request->request).empty());

  // Experiment header should not be set.
  EXPECT_EQ("", GetExperiementHeaderValue(pending_request));

  proto::GeneratePageBundleRequest bundle_data;
  ASSERT_TRUE(bundle_data.ParseFromString(
      network::GetUploadData(pending_request->request)));
  EXPECT_EQ(kTestUserAgent, bundle_data.user_agent());
  EXPECT_EQ(proto::FORMAT_MHTML, bundle_data.output_format());
  EXPECT_EQ(kTestMaxBundleSize, bundle_data.max_bundle_size_bytes());
  EXPECT_EQ(kTestGCMID, bundle_data.gcm_registration_id());
  ASSERT_EQ(2, bundle_data.pages_size());
  EXPECT_EQ(kTestURL, bundle_data.pages(0).url());
  EXPECT_EQ(proto::NO_TRANSFORMATION, bundle_data.pages(0).transformation());
  EXPECT_EQ(kTestURL2, bundle_data.pages(1).url());
  EXPECT_EQ(proto::NO_TRANSFORMATION, bundle_data.pages(1).transformation());
}

TEST_F(GeneratePageBundleRequestTest, ExperimentHeaderInRequestData) {
  // Add the experiment option in the field trial.
  SetUpExperimentOption();

  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::unique_ptr<GeneratePageBundleRequest> request(
      CreateRequest("", callback.Get()));
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  DCHECK(pending_request);

  // Experiment header should be set.
  EXPECT_EQ(kExperimentValueSetInFieldTrial,
            GetExperiementHeaderValue(pending_request));
}

TEST_F(GeneratePageBundleRequestTest, NoTestingHeaderInRequestData) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  // Make a request without the header.
  std::unique_ptr<GeneratePageBundleRequest> request(
      CreateRequest("", callback.Get()));

  std::string testing_header;
  bool has_header = GetPendingRequest()->request.headers.GetHeader(
      "X-Offline-Prefetch-Testing", &testing_header);
  EXPECT_FALSE(has_header);
}

TEST_F(GeneratePageBundleRequestTest, TestingHeaderInRequestData) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  // Make a request with the header.
  std::unique_ptr<GeneratePageBundleRequest> request(
      CreateRequest("ForceEnable", callback.Get()));
  std::string testing_header;
  bool has_header = GetPendingRequest()->request.headers.GetHeader(
      "X-Offline-Prefetch-Testing", &testing_header);
  EXPECT_TRUE(has_header);
  EXPECT_EQ("ForceEnable", testing_header);
}

TEST_F(GeneratePageBundleRequestTest, EmptyResponse) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::unique_ptr<GeneratePageBundleRequest> request(
      CreateRequest("", callback.Get()));

  PrefetchRequestStatus status;
  std::string operation_name;
  std::vector<RenderPageInfo> pages;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&operation_name),
                      SaveArg<2>(&pages)));
  RespondWithData("");
  RunUntilIdle();

  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff, status);
  EXPECT_TRUE(operation_name.empty());
  EXPECT_TRUE(pages.empty());
}

TEST_F(GeneratePageBundleRequestTest, InvalidResponse) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::unique_ptr<GeneratePageBundleRequest> request(
      CreateRequest("", callback.Get()));

  PrefetchRequestStatus status;
  std::string operation_name;
  std::vector<RenderPageInfo> pages;
  EXPECT_CALL(callback, Run(_, _, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&operation_name),
                      SaveArg<2>(&pages)));
  RespondWithData("Some invalid data");
  RunUntilIdle();

  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff, status);
  EXPECT_TRUE(operation_name.empty());
  EXPECT_TRUE(pages.empty());
}

}  // namespace offline_pages
