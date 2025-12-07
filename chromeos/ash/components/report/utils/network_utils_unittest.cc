// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/network_utils.h"

#include "base/time/time.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::report::utils {

namespace {
// Production edge server for reporting metrics.
const char kFresnelProductionBaseUrl[] =
    "https://crosfresnel-pa.googleapis.com";
}  // namespace

TEST(NetworkUtilsTest, GetAPIKey) {
  std::string api_key = GetAPIKey();
  EXPECT_FALSE(api_key.empty());
}

TEST(NetworkUtilsTest, ValidateFresnelUrlTarget) {
  EXPECT_STREQ(utils::kFresnelBaseUrl, kFresnelProductionBaseUrl);
}

TEST(NetworkUtilsTest, GetOprfRequestURL) {
  GURL oprf_request_url = GetOprfRequestURL();
  EXPECT_FALSE(oprf_request_url.is_empty());
}

TEST(NetworkUtilsTest, GetQueryRequestURL) {
  GURL query_request_url = GetQueryRequestURL();
  EXPECT_FALSE(query_request_url.is_empty());
}

TEST(NetworkUtilsTest, GetImportRequestURL) {
  GURL import_request_url = GetImportRequestURL();
  EXPECT_FALSE(import_request_url.is_empty());
}

TEST(NetworkUtilsTest, GetOprfRequestTimeout) {
  base::TimeDelta oprf_request_timeout = GetOprfRequestTimeout();
  EXPECT_EQ(oprf_request_timeout, base::Seconds(15));
}

TEST(NetworkUtilsTest, GetQueryRequestTimeout) {
  base::TimeDelta query_request_timeout = GetQueryRequestTimeout();
  EXPECT_EQ(query_request_timeout, base::Seconds(65));
}

TEST(NetworkUtilsTest, GetImportRequestTimeout) {
  base::TimeDelta import_request_timeout = GetImportRequestTimeout();
  EXPECT_EQ(import_request_timeout, base::Seconds(15));
}

TEST(NetworkUtilsTest, GetMaxFresnelResponseSizeBytes) {
  size_t max_response_size = GetMaxFresnelResponseSizeBytes();
  EXPECT_GT(max_response_size, static_cast<size_t>(0));
}

TEST(NetworkUtilsTest, GenerateResourceRequest) {
  GURL url(utils::kFresnelBaseUrl);
  std::unique_ptr<network::ResourceRequest> resource_request =
      GenerateResourceRequest(url);
  EXPECT_EQ(resource_request->url, url);
  EXPECT_EQ(resource_request->method, net::HttpRequestHeaders::kPostMethod);

  EXPECT_EQ(resource_request->headers.GetHeader("x-goog-api-key"), GetAPIKey());
  EXPECT_EQ(resource_request->headers.GetHeader(
                net::HttpRequestHeaders::kContentType),
            "application/x-protobuf");
}

}  // namespace ash::report::utils
