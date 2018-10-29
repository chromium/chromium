// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/http/http_response_headers.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

TEST(DataReductionProxyHeadersTest, IsEmptyImagePreview) {
  const struct {
    const char* headers;
    bool expected_result;
  } tests[] = {
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: foo\n",
          false,
      },
      {
          "HTTP/1.1 200 OK\n", false,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: empty-image\n",
          true,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: empty-image;foo\n",
          true,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: Empty-Image\n",
          true,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: foo;empty-image\n",
          false,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Another-Header: empty-image\n",
          false,
      },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
    EXPECT_EQ(tests[i].expected_result, IsEmptyImagePreview(*parsed));
  }
}

TEST(DataReductionProxyHeadersTest, IsEmptyImagePreviewValue) {
  const struct {
    const char* chrome_proxy_content_transform_header;
    const char* chrome_proxy_header;
    bool expected_result;
  } tests[] = {
      {"", "", false},
      {"foo", "bar", false},
      {"empty-image", "", true},
      {"empty-image;foo", "", true},
      {"Empty-Image", "", true},
      {"foo;empty-image", "", false},
      {"empty-image", "foo", true},
  };
  for (const auto& test : tests) {
    EXPECT_EQ(test.expected_result,
              IsEmptyImagePreview(test.chrome_proxy_content_transform_header,
                                  test.chrome_proxy_header));
  }
}

TEST(DataReductionProxyHeadersTest, IsLitePagePreview) {
  const struct {
    const char* headers;
    bool expected_result;
  } tests[] = {
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: foo\n",
          false,
      },
      {
          "HTTP/1.1 200 OK\n", false,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: lite-page\n",
          true,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: lite-page;foo\n",
          true,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: Lite-Page\n",
          true,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Chrome-Proxy-Content-Transform: foo;lite-page\n",
          false,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Another-Header: lite-page\n",
          false,
      },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
    EXPECT_EQ(tests[i].expected_result, IsLitePagePreview(*parsed));
  }
}

TEST(DataReductionProxyHeadersTest, GetDataReductionProxyActionValue) {
  const struct {
     const char* headers;
     std::string action_key;
     bool expected_result;
     std::string expected_action_value;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 999\n",
      "a",
      false,
      "",
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Content-Length: 999\n",
      "a",
      false,
      "",
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=86400\n"
      "Content-Length: 999\n",
      "bypass",
      true,
      "86400",
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass86400\n"
      "Content-Length: 999\n",
      "bypass",
      false,
      "",
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=0\n"
      "Content-Length: 999\n",
      "bypass",
      true,
      "0",
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=1500\n"
      "Chrome-Proxy: bypass=86400\n"
      "Content-Length: 999\n",
      "bypass",
      true,
      "1500",
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block=1500, block=3600\n"
      "Content-Length: 999\n",
      "block",
      true,
      "1500",
    },
    { "HTTP/1.1 200 OK\n"
      "connection: proxy-bypass\n"
      "Chrome-Proxy:    key=123   \n"
      "Content-Length: 999\n",
      "key",
      true,
      "123",
    },
    { "HTTP/1.1 200 OK\n"
      "connection: proxy-bypass\n"
      "Chrome-Proxy: block-once\n"
      "Content-Length: 999\n",
      "block-once",
      false,
      "",
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    std::string action_value;
    bool has_action_key = GetDataReductionProxyActionValue(
        parsed.get(), tests[i].action_key, &action_value);
    EXPECT_EQ(tests[i].expected_result, has_action_key);
    if (has_action_key) {
      EXPECT_EQ(tests[i].expected_action_value, action_value);
    }
  }
}

TEST(DataReductionProxyHeadersTest, GetProxyBypassInfo) {
  const struct {
     const char* headers;
     bool expected_result;
     int64_t expected_retry_delay;
     bool expected_bypass_all;
     bool expected_mark_proxies_as_bad;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=-1\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=xyz\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: foo=abc, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=86400, bar=abc\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=3600\n"
      "Chrome-Proxy: bypass=86400\n"
      "Content-Length: 999\n",
      true,
      3600,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=3600, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      3600,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass\n"
      "Chrome-Proxy: bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block=, block=3600\n"
      "Content-Length: 999\n",
      true,
      3600,
      true,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=86400, block=3600\n"
      "Content-Length: 999\n",
      true,
      3600,
      true,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: proxy-bypass\n"
      "Chrome-Proxy: block=, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: proxy-bypass\n"
      "Chrome-Proxy: block=-1\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: proxy-bypass\n"
      "Chrome-Proxy: block=99999999999999999999\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block-once\n"
      "Content-Length: 999\n",
      true,
      0,
      true,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block-once=\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block-once=10\n"
      "Content-Length: 999\n",
      false,
      0,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block-once, bypass=86400, block=3600\n"
      "Content-Length: 999\n",
      true,
      3600,
      true,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block-once\n"
      "Chrome-Proxy: bypass=86400, block=3600\n"
      "Content-Length: 999\n",
      true,
      3600,
      true,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block-once, bypass=86400\n"
      "Content-Length: 999\n",
      true,
      86400,
      false,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: block-once, block=3600\n"
      "Content-Length: 999\n",
      true,
      3600,
      true,
      true,
    },
    { "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=, block=, block-once\n"
      "Content-Length: 999\n",
      true,
      0,
      true,
      false,
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    DataReductionProxyInfo data_reduction_proxy_info;
    EXPECT_EQ(tests[i].expected_result,
              ParseHeadersForBypassInfo(*parsed, &data_reduction_proxy_info));
    EXPECT_EQ(tests[i].expected_retry_delay,
              data_reduction_proxy_info.bypass_duration.InSeconds());
    EXPECT_EQ(tests[i].expected_bypass_all,
              data_reduction_proxy_info.bypass_all);
    EXPECT_EQ(tests[i].expected_mark_proxies_as_bad,
              data_reduction_proxy_info.mark_proxies_as_bad);
  }
}

TEST(DataReductionProxyHeadersTest, ParseHeadersAndSetProxyInfo) {
  std::string headers =
      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Chrome-Proxy: bypass=0\n"
      "Content-Length: 999\n";
  HeadersToRaw(&headers);
  scoped_refptr<net::HttpResponseHeaders> parsed(
      new net::HttpResponseHeaders(headers));

  DataReductionProxyInfo data_reduction_proxy_info;
  EXPECT_TRUE(ParseHeadersForBypassInfo(*parsed, &data_reduction_proxy_info));
  EXPECT_LE(60, data_reduction_proxy_info.bypass_duration.InSeconds());
  EXPECT_GE(5 * 60, data_reduction_proxy_info.bypass_duration.InSeconds());
  EXPECT_FALSE(data_reduction_proxy_info.bypass_all);
}

TEST(DataReductionProxyHeadersTest, HasDataReductionProxyViaHeader) {
  const struct {
    const char* headers;
    bool expected_result;
    bool expected_has_intermediary;
    bool ignore_intermediary;
  } tests[] = {
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Proxy\n",
      false,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1\n",
      false,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      true,
      true,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.0 Chrome-Compression-Proxy\n",
      true,
      true,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Foo-Bar, 1.1 Chrome-Compression-Proxy\n",
      true,
      true,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy, 1.1 Bar-Foo\n",
      true,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 chrome-compression-proxy\n",
      false,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Foo-Bar\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      true,
      true,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n"
      "Via: 1.1 Foo-Bar\n",
      true,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Proxy\n",
      false,
      false,
      false,
    },
    { "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome Compression Proxy\n",
      false,
      false,
      false,
    },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));

    bool has_chrome_proxy_via_header, has_intermediary;
    if (tests[i].ignore_intermediary) {
      has_chrome_proxy_via_header =
          HasDataReductionProxyViaHeader(*parsed, nullptr);
    }
    else {
      has_chrome_proxy_via_header =
          HasDataReductionProxyViaHeader(*parsed, &has_intermediary);
    }
    EXPECT_EQ(tests[i].expected_result, has_chrome_proxy_via_header);
    if (has_chrome_proxy_via_header && !tests[i].ignore_intermediary) {
      EXPECT_EQ(tests[i].expected_has_intermediary, has_intermediary);
    }
  }
}

TEST(DataReductionProxyHeadersTest, BypassMissingViaIfExperiment) {
  const char kWarmupFetchCallbackEnabledParam[] =
      "warmup_fetch_callback_enabled";

  const struct {
    const char* headers;
    std::map<std::string, std::string> feature_parameters;
    DataReductionProxyBypassType expected_result;
  } tests[] = {
      {
          "HTTP/1.1 200 OK\n",
          {
              {kWarmupFetchCallbackEnabledParam, "true"},
              {params::GetMissingViaBypassParamName(), "true"},
          },
          BYPASS_EVENT_TYPE_MAX,
      },
      {
          "HTTP/1.1 200 OK\n",
          {
              {kWarmupFetchCallbackEnabledParam, "true"},
              {params::GetMissingViaBypassParamName(), "false"},
          },
          BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER,
      },
      {
          "HTTP/1.1 200 OK\n",
          {
              {kWarmupFetchCallbackEnabledParam, "false"},
              {params::GetMissingViaBypassParamName(), "false"},
          },
          BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER,
      },
  };
  for (auto test : tests) {
    std::string headers(test.headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
    DataReductionProxyInfo proxy_info;

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kDataReductionProxyRobustConnection, test.feature_parameters);

    EXPECT_EQ(test.expected_result,
              GetDataReductionProxyBypassType(std::vector<GURL>(), *parsed,
                                              &proxy_info));
    if (test.expected_result != BYPASS_EVENT_TYPE_MAX)
      EXPECT_TRUE(proxy_info.mark_proxies_as_bad);
  }
}

TEST(DataReductionProxyHeadersTest, GetDataReductionProxyBypassEventType) {
  const struct {
     const char* headers;
     DataReductionProxyBypassType expected_result;
  } tests[] = {{
                   "HTTP/1.1 200 OK\n"
                   "Chrome-Proxy: bypass=0\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_MEDIUM,
               },
               {
                   "HTTP/1.1 200 OK\n"
                   "Chrome-Proxy: bypass=1\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_SHORT,
               },
               {
                   "HTTP/1.1 200 OK\n"
                   "Chrome-Proxy: bypass=59\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_SHORT,
               },
               {
                   "HTTP/1.1 200 OK\n"
                   "Chrome-Proxy: bypass=60\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_MEDIUM,
               },
               {
                   "HTTP/1.1 200 OK\n"
                   "Chrome-Proxy: bypass=300\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_MEDIUM,
               },
               {
                   "HTTP/1.1 200 OK\n"
                   "Chrome-Proxy: bypass=301\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_LONG,
               },
               {
                   "HTTP/1.1 200 OK\n"
                   "Chrome-Proxy: block-once\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_CURRENT,
               },
               {
                   "HTTP/1.1 500 Internal Server Error\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR,
               },
               {
                   "HTTP/1.1 501 Not Implemented\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_MAX,
               },
               {
                   "HTTP/1.1 502 Bad Gateway\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY,
               },
               {
                   "HTTP/1.1 503 Service Unavailable\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE,
               },
               {
                   "HTTP/1.1 504 Gateway Timeout\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_MAX,
               },
               {
                   "HTTP/1.1 505 HTTP Version Not Supported\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_MAX,
               },
               {
                   "HTTP/1.1 304 Not Modified\n", BYPASS_EVENT_TYPE_MAX,
               },
               {
                   "HTTP/1.1 200 OK\n", BYPASS_EVENT_TYPE_MAX,
               },
               {
                   "HTTP/1.1 200 OK\n"
                   "Chrome-Proxy: bypass=59\n",
                   BYPASS_EVENT_TYPE_SHORT,
               },
               {
                   "HTTP/1.1 502 Bad Gateway\n",
                   BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY,
               },
               {
                   "HTTP/1.1 502 Bad Gateway\n"
                   "Chrome-Proxy: bypass=59\n",
                   BYPASS_EVENT_TYPE_SHORT,
               },
               {
                   "HTTP/1.1 502 Bad Gateway\n"
                   "Chrome-Proxy: bypass=59\n",
                   BYPASS_EVENT_TYPE_SHORT,
               },
               {
                   "HTTP/1.1 414 Request-URI Too Long\n", BYPASS_EVENT_TYPE_MAX,
               },
               {
                   "HTTP/1.1 414 Request-URI Too Long\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_MAX,
               },
               {
                   "HTTP/1.1 407 Proxy Authentication Required\n",
                   BYPASS_EVENT_TYPE_MALFORMED_407,
               },
               {
                   "HTTP/1.1 407 Proxy Authentication Required\n"
                   "Proxy-Authenticate: Basic\n"
                   "Via: 1.1 Chrome-Compression-Proxy\n",
                   BYPASS_EVENT_TYPE_MAX,
               }};
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string headers(tests[i].headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
    DataReductionProxyInfo chrome_proxy_info;

    EXPECT_EQ(tests[i].expected_result,
              GetDataReductionProxyBypassType(std::vector<GURL>(), *parsed,
                                              &chrome_proxy_info));
  }
}

TEST(DataReductionProxyHeadersTest,
     GetDataReductionProxyBypassEventTypeURLRedirectCycle) {
  const struct {
    const char* headers;
    std::vector<GURL> url_chain;
    DataReductionProxyBypassType expected_result;
  } tests[] = {
      {
          "HTTP/1.1 200 OK\n"
          "Via: 1.1 Chrome-Compression-Proxy\n",
          std::vector<GURL>{GURL("http://google.com/1"),
                            GURL("http://google.com/2"),
                            GURL("http://google.com/1")},
          BYPASS_EVENT_TYPE_URL_REDIRECT_CYCLE,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Via: 1.1 Chrome-Compression-Proxy\n",
          std::vector<GURL>{
              GURL("http://google.com/1"), GURL("http://google.com/2"),
              GURL("http://google.com/1"), GURL("http://google.com/2")},
          BYPASS_EVENT_TYPE_URL_REDIRECT_CYCLE,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Via: 1.1 Chrome-Compression-Proxy\n",
          std::vector<GURL>{GURL("http://google.com/1")}, BYPASS_EVENT_TYPE_MAX,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Via: 1.1 Chrome-Compression-Proxy\n",
          std::vector<GURL>{GURL("http://google.com/1"),
                            GURL("http://google.com/2")},
          BYPASS_EVENT_TYPE_MAX,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Via: 1.1 Chrome-Compression-Proxy\n",
          std::vector<GURL>{GURL("http://google.com/1"),
                            GURL("http://google.com/2"),
                            GURL("http://google.com/3")},
          BYPASS_EVENT_TYPE_MAX,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Via: 1.1 Chrome-Compression-Proxy\n",
          std::vector<GURL>{
              GURL("http://google.com/1"), GURL("http://google.com/2"),
              GURL("http://google.com/3"), GURL("http://google.com/1")},
          BYPASS_EVENT_TYPE_URL_REDIRECT_CYCLE,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Via: 1.1 Chrome-Compression-Proxy\n",
          std::vector<GURL>{
              GURL("http://google.com/1"), GURL("http://google.com/2"),
              GURL("http://google.com/1"), GURL("http://google.com/3")},
          BYPASS_EVENT_TYPE_MAX,
      },
      {
          "HTTP/1.1 200 OK\n"
          "Via: 1.1 Chrome-Compression-Proxy\n",
          std::vector<GURL>(), BYPASS_EVENT_TYPE_MAX,
      }};

  for (const auto& test : tests) {
    std::string headers(test.headers);
    HeadersToRaw(&headers);
    scoped_refptr<net::HttpResponseHeaders> parsed(
        new net::HttpResponseHeaders(headers));
    DataReductionProxyInfo chrome_proxy_info;

    EXPECT_EQ(test.expected_result,
              GetDataReductionProxyBypassType(test.url_chain, *parsed,
                                              &chrome_proxy_info));
  }
}

}  // namespace data_reduction_proxy
