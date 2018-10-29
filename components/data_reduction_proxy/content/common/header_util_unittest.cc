// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/common/header_util.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/common/previews_state.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {
namespace {

constexpr char kUrl[] = "http://example.com";
constexpr char kHttpsUrl[] = "https://example.com";

}  // namespace

TEST(HeaderUtilTest, MaybeSetAcceptTransformHeader) {
  const struct {
    GURL url;
    content::ResourceType resource_type;
    content::PreviewsState previews_state;
    std::string expected_header;
  } tests[] = {
      {GURL(kUrl), content::RESOURCE_TYPE_MEDIA, 0,
       compressed_video_directive()},
      {GURL(kHttpsUrl), content::RESOURCE_TYPE_MEDIA, 0, ""},
      {GURL(kUrl), content::RESOURCE_TYPE_MAIN_FRAME,
       content::SERVER_LITE_PAGE_ON, lite_page_directive()},
      {GURL(kUrl), content::RESOURCE_TYPE_SUB_FRAME, content::SERVER_LOFI_ON,
       empty_image_directive()},
      {GURL(kUrl), content::RESOURCE_TYPE_SUB_FRAME, 0, ""},
      {GURL(kUrl), content::RESOURCE_TYPE_SCRIPT, content::SERVER_LOFI_ON, ""},
  };

  for (const auto& test : tests) {
    net::HttpRequestHeaders headers;
    MaybeSetAcceptTransformHeader(test.url, test.resource_type,
                                  test.previews_state, &headers);

    std::string value;
    EXPECT_EQ(headers.GetHeader(chrome_proxy_accept_transform_header(), &value),
              !test.expected_header.empty());
    EXPECT_EQ(value, test.expected_header);
  }
}

}  // namespace data_reduction_proxy
