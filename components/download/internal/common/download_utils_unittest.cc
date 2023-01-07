// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/download_features.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace download {
namespace {

TEST(DownloadUtilsTest, HandleServerResponse200) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            HandleSuccessfulServerResponse(*headers.get(), nullptr,
                                           /*fetch_error_body=*/false));
}

TEST(DownloadUtilsTest, HandleServerResponse200_RangeRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDownloadRange);

  // Arbitrary range request must expect HTTP 206 as a successful response.
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 200 OK"));
  DownloadSaveInfo save_info;
  save_info.range_request_to = 100;
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            HandleSuccessfulServerResponse(*headers.get(), &save_info,
                                           /*fetch_error_body=*/false));
}

TEST(DownloadUtilsTest, HandleServerResponse206_RangeRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDownloadRange);

  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders("HTTP/1.1 206 Partial Content"));
  headers->AddHeader("Content-Range", "bytes 105-125/500");
  DownloadSaveInfo save_info;
  save_info.range_request_from = 100;
  EXPECT_TRUE(save_info.IsArbitraryRangeRequest());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            HandleSuccessfulServerResponse(*headers.get(), &save_info,
                                           /*fetch_error_body=*/false));
}

void VerifyRangeHeader(DownloadUrlParameters* params,
                       const std::string& expected_range_header) {
  auto resource_request = CreateResourceRequest(params);
  std::string header_value;
  ASSERT_TRUE(resource_request->headers.GetHeader(
      net::HttpRequestHeaders::kRange, &header_value));
  ASSERT_FALSE(
      resource_request->headers.HasHeader(net::HttpRequestHeaders::kIfRange));
  EXPECT_EQ(expected_range_header, header_value);
}

TEST(DownloadUtilsTest, CreateResourceRequest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDownloadRange);
  auto params = std::make_unique<DownloadUrlParameters>(
      GURL(), TRAFFIC_ANNOTATION_FOR_TESTS);
  params->set_use_if_range(false);
  params->set_range_request_offset(100, kInvalidRange);
  VerifyRangeHeader(params.get(), "bytes=100-");
  params->set_offset(5);
  VerifyRangeHeader(params.get(), "bytes=105-");

  params->set_offset(0);
  params->set_range_request_offset(100, 200);
  VerifyRangeHeader(params.get(), "bytes=100-200");
  params->set_offset(5);
  VerifyRangeHeader(params.get(), "bytes=105-200");

  params->set_offset(0);
  params->set_range_request_offset(kInvalidRange, 200);
  VerifyRangeHeader(params.get(), "bytes=-200");
  params->set_offset(5);
  VerifyRangeHeader(params.get(), "bytes=-195");
}

}  // namespace
}  // namespace download
