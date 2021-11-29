// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_utils.h"

#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/download_features.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  // Arbitrary range request must expect HTTP 204 as a successful response.
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

}  // namespace
}  // namespace download
