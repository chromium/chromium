// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

using testing::ContainerEq;
using testing::ElementsAre;

TEST(DataReductionProxyBypassProtocolTest, InvalidHeadersRetry) {
  auto proxy_server = net::ProxyServer::FromPacString("HTTPS proxy.com");
  std::vector<DataReductionProxyServer> servers{
      DataReductionProxyServer(proxy_server)};
  base::Optional<DataReductionProxyTypeInfo> type_info =
      DataReductionProxyTypeInfo(servers, 0u);
  std::vector<GURL> url_chain{GURL("http://google.com")};
  DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;
  DataReductionProxyInfo proxy_info;
  std::vector<net::ProxyServer> bad_proxies;
  int additional_load_flags = 0;

  DataReductionProxyBypassProtocol protocol;
  EXPECT_TRUE(protocol.MaybeBypassProxyAndPrepareToRetry(
      "GET", url_chain, nullptr /* response_headers */,
      net::ProxyServer::Direct(), net::ERR_FAILED, net::ProxyRetryInfoMap(),
      type_info, &bypass_type, &proxy_info, &bad_proxies,
      &additional_load_flags));

  EXPECT_FALSE(proxy_info.bypass_all);
  EXPECT_TRUE(proxy_info.mark_proxies_as_bad);
  EXPECT_THAT(bad_proxies, ElementsAre(proxy_server));
}

TEST(DataReductionProxyBypassProtocolTest, InvalidHeadersNoRetry) {
  base::Optional<DataReductionProxyTypeInfo> type_info;
  std::vector<GURL> url_chain{GURL("http://google.com")};
  DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;
  DataReductionProxyInfo proxy_info;
  std::vector<net::ProxyServer> bad_proxies;
  int additional_load_flags = 0;

  DataReductionProxyBypassProtocol protocol;
  EXPECT_FALSE(protocol.MaybeBypassProxyAndPrepareToRetry(
      "GET", url_chain, nullptr /* response_headers */,
      net::ProxyServer::Direct(), net::ERR_ABORTED, net::ProxyRetryInfoMap(),
      type_info, &bypass_type, &proxy_info, &bad_proxies,
      &additional_load_flags));
}

// After each test, the proxy retry info will contain zero, one, or two of the
// data reduction proxies depending on whether no bypass was indicated by the
// initial response, a single proxy bypass was indicated, or a double bypass
// was indicated. In both the single and double bypass cases, if the request
// was idempotent, it will be retried over a direct connection.
TEST(DataReductionProxyBypassProtocolTest, BypassLogic) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetchCallback);
  const struct {
    const char* method;
    const char* response_headers;
    bool expected_retry;
    size_t expected_bad_proxy_count;
    int expected_duration;
    DataReductionProxyBypassType expected_bypass_type;
  } tests[] = {
    // Valid data reduction proxy response with no bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      0u,
      -1,
      BYPASS_EVENT_TYPE_MAX,
    },
    // Valid data reduction proxy response with chained via header,
    // no bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy, 1.0 some-other-proxy\r\n\r\n",
      false,
      0u,
      -1,
      BYPASS_EVENT_TYPE_MAX
    },
    // Valid data reduction proxy response with a bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Valid data reduction proxy response with a bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=1\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
    // Same as above with the OPTIONS method, which is idempotent.
    { "OPTIONS",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Same as above with the HEAD method, which is idempotent.
    { "HEAD",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Same as above with the PUT method, which is idempotent.
    { "PUT",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Same as above with the DELETE method, which is idempotent.
    { "DELETE",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Same as above with the TRACE method, which is idempotent.
    { "TRACE",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // 500 responses should be bypassed.
    { "GET",
      "HTTP/1.1 500 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR
    },
    // 502 responses should be bypassed.
    { "GET",
      "HTTP/1.1 502 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY
    },
    // 503 responses should be bypassed.
    { "GET",
      "HTTP/1.1 503 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE
    },
    // Invalid data reduction proxy 4xx response. Missing Via header.
    { "GET",
      "HTTP/1.1 404 Not Found\r\n"
      "Server: proxy\r\n\r\n",
      true,
      0u,
      0,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_4XX
    },
    // Invalid data reduction proxy response. Missing Via header.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER
    },
    // Invalid data reduction proxy response. Wrong Via header.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.0 some-other-proxy\r\n\r\n",
      true,
      1u,
      0,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER
    },
    // Valid data reduction proxy response. 304 missing Via header.
    { "GET",
      "HTTP/1.1 304 Not Modified\r\n"
      "Server: proxy\r\n\r\n",
      false,
      0u,
      0,
      BYPASS_EVENT_TYPE_MAX
    },
    // Valid data reduction proxy response with a bypass message. It will
    // not be retried because the request is non-idempotent.
    { "POST",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=0\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      1u,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // Valid data reduction proxy response with block message. Both proxies
    // should be on the retry list when it completes.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block=1\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      2u,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
    // Valid data reduction proxy response with a block-once message. It will be
    // retried, and there will be no proxies on the retry list since block-once
    // only affects the current request.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      0u,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the OPTIONS method, which is idempotent.
    { "OPTIONS",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      0u,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the HEAD method, which is idempotent.
    { "HEAD",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      0u,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the PUT method, which is idempotent.
    { "PUT",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      0u,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the DELETE method, which is idempotent.
    { "DELETE",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      0u,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Same as above with the TRACE method, which is idempotent.
    { "TRACE",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      0u,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Valid Data Reduction Proxy response with a block-once message. It will
    // be retried because block-once indicates that request did not reach the
    // origin and client should retry. Only current request is retried direct,
    // so there should be no proxies on the retry list.
    { "POST",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      0u,
      0,
      BYPASS_EVENT_TYPE_CURRENT
    },
    // Valid Data Reduction Proxy response with a bypass message. It will
    // not be retried because the request is non-idempotent. Both proxies
    // should be on the retry list for 1 second.
    { "POST",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block=1\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      2u,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
    // Valid data reduction proxy response with block and block-once messages.
    // The block message will override the block-once message, so both proxies
    // should be on the retry list when it completes.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: block=1, block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      2u,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
    // Valid data reduction proxy response with bypass and block-once messages.
    // The bypass message will override the block-once message, so one proxy
    // should be on the retry list when it completes.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Chrome-Proxy: bypass=1, block-once\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      1u,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
  };
  for (const auto& test : tests) {
    auto primary = net::ProxyServer::FromPacString("HTTPS primary");
    auto fallback = net::ProxyServer::FromPacString("HTTPS fallback");
    std::vector<DataReductionProxyServer> servers{
        DataReductionProxyServer(primary), DataReductionProxyServer(fallback)};
    base::Optional<DataReductionProxyTypeInfo> type_info =
        DataReductionProxyTypeInfo(servers, 0u);
    std::vector<GURL> url_chain{GURL("http://google.com")};
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(test.response_headers));
    DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;
    DataReductionProxyInfo proxy_info;
    std::vector<net::ProxyServer> bad_proxies;
    int additional_load_flags = 0;

    DataReductionProxyBypassProtocol protocol;
    EXPECT_EQ(
        test.expected_retry,
        protocol.MaybeBypassProxyAndPrepareToRetry(
            test.method, url_chain, headers.get(), net::ProxyServer::Direct(),
            net::OK, net::ProxyRetryInfoMap(), type_info, &bypass_type,
            &proxy_info, &bad_proxies, &additional_load_flags));

    std::vector<net::ProxyServer> expected_bad_proxies;
    if (test.expected_bad_proxy_count > 0) {
      expected_bad_proxies.push_back(primary);
      if (test.expected_bad_proxy_count > 1)
        expected_bad_proxies.push_back(fallback);
      EXPECT_THAT(bad_proxies, ContainerEq(expected_bad_proxies));

      if (test.expected_duration == 0) {
        EXPECT_GE(proxy_info.bypass_duration, base::TimeDelta::FromMinutes(1));
        EXPECT_LE(proxy_info.bypass_duration, base::TimeDelta::FromMinutes(5));
      } else {
        EXPECT_EQ(proxy_info.bypass_duration,
                  base::TimeDelta::FromSeconds(test.expected_duration));
      }
    }

    EXPECT_EQ(bypass_type, test.expected_bypass_type);
  }
}

TEST(DataReductionProxyBypassProtocolTest, ResponseProxyServerStateHistogram) {
  const struct {
    const char* pac_string;
    bool enable_data_reduction_proxy;
    const char* response_headers;
    // |RESPONSE_PROXY_SERVER_STATUS_MAX| indicates no expected value.
    DataReductionProxyBypassProtocol::ResponseProxyServerStatus expected_status;
  } test_cases[] = {
      {"DIRECT", false, "HTTP/1.1 200 OK\r\n\r\n",
       DataReductionProxyBypassProtocol::RESPONSE_PROXY_SERVER_STATUS_EMPTY},
      {"DIRECT", true,
       "HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n",
       DataReductionProxyBypassProtocol::RESPONSE_PROXY_SERVER_STATUS_DRP},
      {"PROXY unrecognized-drp.net", false, "HTTP/1.1 200 OK\r\n\r\n",
       DataReductionProxyBypassProtocol::
           RESPONSE_PROXY_SERVER_STATUS_NON_DRP_NO_VIA},
      {"PROXY unrecognized-drp.net", false,
       "HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n",
       DataReductionProxyBypassProtocol::
           RESPONSE_PROXY_SERVER_STATUS_NON_DRP_WITH_VIA},
  };

  for (const auto& test : test_cases) {
    auto primary = net::ProxyServer::FromPacString("HTTPS primary");
    std::vector<DataReductionProxyServer> servers{
        DataReductionProxyServer(primary)};
    base::Optional<DataReductionProxyTypeInfo> type_info;
    if (test.enable_data_reduction_proxy)
      type_info.emplace(servers, 0u);
    std::vector<GURL> url_chain{GURL("http://google.com")};
    auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(test.response_headers));
    DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;
    DataReductionProxyInfo proxy_info;
    std::vector<net::ProxyServer> bad_proxies;
    int additional_load_flags = 0;

    base::HistogramTester histogram_tester;
    DataReductionProxyBypassProtocol protocol;
    protocol.MaybeBypassProxyAndPrepareToRetry(
        "GET", url_chain, headers.get(),
        net::ProxyServer::FromPacString(test.pac_string), net::OK,
        net::ProxyRetryInfoMap(), type_info, &bypass_type, &proxy_info,
        &bad_proxies, &additional_load_flags);

    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.ResponseProxyServerStatus", test.expected_status,
        1);
  }
}

}  // namespace data_reduction_proxy
