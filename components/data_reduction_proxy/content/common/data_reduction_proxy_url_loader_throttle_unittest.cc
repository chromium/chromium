// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/common/data_reduction_proxy_url_loader_throttle.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/common/previews_state.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

TEST(DataReductionProxyURLLoaderThrottleTest, AcceptTransformHeaderSet) {
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()));
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  request.resource_type = content::RESOURCE_TYPE_MEDIA;
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_pre_cache_headers.GetHeader(
      chrome_proxy_accept_transform_header(), &value));
  EXPECT_EQ(value, compressed_video_directive());
}

TEST(DataReductionProxyURLLoaderThrottleTest,
     AcceptTransformHeaderSetForMainFrame) {
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()));
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  request.resource_type = content::RESOURCE_TYPE_MAIN_FRAME;
  request.previews_state = content::SERVER_LITE_PAGE_ON;
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_pre_cache_headers.GetHeader(
      chrome_proxy_accept_transform_header(), &value));
  EXPECT_EQ(value, lite_page_directive());
}

TEST(DataReductionProxyURLLoaderThrottleTest,
     ConstructorHeadersAddedToPostCacheHeaders) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("foo", "bar");
  DataReductionProxyURLLoaderThrottle throttle(headers);
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_post_cache_headers.GetHeader("foo", &value));
  EXPECT_EQ(value, "bar");
}

TEST(DataReductionProxyURLLoaderThrottleTest, UseAlternateProxyList) {
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()));
  network::ResourceRequest request;
  request.resource_type = content::RESOURCE_TYPE_MEDIA;
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_TRUE(request.custom_proxy_use_alternate_proxy_list);
}

TEST(DataReductionProxyURLLoaderThrottleTest, DontUseAlternateProxyList) {
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()));
  network::ResourceRequest request;
  request.resource_type = content::RESOURCE_TYPE_MAIN_FRAME;
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(request.custom_proxy_use_alternate_proxy_list);
}

}  // namespace data_reduction_proxy
