// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_server.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::MockRead;

namespace data_reduction_proxy {

TEST(ChromeNetworkDailyDataSavingMetricsTest,
     GetDataReductionProxyRequestType) {
  base::MessageLoopForIO message_loop;
  std::unique_ptr<DataReductionProxyTestContext> test_context =
      DataReductionProxyTestContext::Builder()
          .Build();
  TestDataReductionProxyConfig* config = test_context->config();
  config->test_params()->UseNonSecureProxiesForHttp();

  net::ProxyServer origin =
      config->test_params()->proxies_for_http().front().proxy_server();
  net::ProxyConfig data_reduction_proxy_config;
  data_reduction_proxy_config.proxy_rules().ParseFromString(
      "http=" + origin.host_port_pair().ToString() + ",direct://");
  data_reduction_proxy_config.proxy_rules().bypass_rules.ParseFromString(
      "localbypass.com");

  struct TestCase {
    GURL url;
    net::ProxyServer proxy_server;
    base::TimeDelta bypass_duration;  // 0 indicates not bypassed.
    int load_flags;
    const char* response_headers;
    DataReductionProxyRequestType expected_request_type;
  };
  const TestCase test_cases[] = {
      {
          GURL("http://foo.com"), origin, base::TimeDelta(), net::LOAD_NORMAL,
          "HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n",
          VIA_DATA_REDUCTION_PROXY,
      },
      {
          GURL("https://foo.com"), net::ProxyServer::Direct(),
          base::TimeDelta(), net::LOAD_NORMAL, "HTTP/1.1 200 OK\r\n\r\n", HTTPS,
      },
      {
          GURL("http://foo.com"), net::ProxyServer::Direct(),
          base::TimeDelta::FromSeconds(1), net::LOAD_NORMAL,
          "HTTP/1.1 200 OK\r\n\r\n", DIRECT_HTTP,
      },
      {
          GURL("http://foo.com"), net::ProxyServer::Direct(),
          base::TimeDelta::FromSeconds(1), net::LOAD_NORMAL,
          "HTTP/1.1 304 Not Modified\r\n\r\n", DIRECT_HTTP,
      },
      {
          GURL("http://foo.com"), net::ProxyServer::Direct(),
          base::TimeDelta::FromMinutes(60), net::LOAD_NORMAL,
          "HTTP/1.1 200 OK\r\n\r\n", DIRECT_HTTP,
      },
      {
          GURL("http://foo.com"), net::ProxyServer::Direct(),
          base::TimeDelta::FromMinutes(60), net::LOAD_NORMAL,
          "HTTP/1.1 304 Not Modified\r\n\r\n", DIRECT_HTTP,
      },
      {
          GURL("http://foo.com"), origin, base::TimeDelta::FromSeconds(1),
          net::LOAD_NORMAL, "HTTP/1.1 200 OK\r\n\r\n", SHORT_BYPASS,
      },
      {
          GURL("http://foo.com"), origin, base::TimeDelta::FromSeconds(1),
          net::LOAD_NORMAL, "HTTP/1.1 304 Not Modified\r\n\r\n", SHORT_BYPASS,
      },
      {
          GURL("http://foo.com"), origin, base::TimeDelta::FromMinutes(60),
          net::LOAD_NORMAL, "HTTP/1.1 200 OK\r\n\r\n", LONG_BYPASS,
      },
      {
          GURL("http://foo.com"), origin, base::TimeDelta::FromMinutes(60),
          net::LOAD_NORMAL, "HTTP/1.1 304 Not Modified\r\n\r\n", LONG_BYPASS,
      },  // Requests with LOAD_BYPASS_PROXY (e.g. block-once) should be
          // classified
      // as SHORT_BYPASS.
      {
          GURL("http://foo.com"), net::ProxyServer::Direct(), base::TimeDelta(),
          net::LOAD_BYPASS_PROXY, "HTTP/1.1 200 OK\r\n\r\n", SHORT_BYPASS,
      },
      // Another proxy overriding the Data Reduction Proxy should be classified
      // as SHORT_BYPASS.
      {
          GURL("http://foo.com"),
          net::ProxyServer::FromPacString("PROXY otherproxy.net:80"),
          base::TimeDelta(), net::LOAD_NORMAL, "HTTP/1.1 200 OK\r\n\r\n",
          SHORT_BYPASS,
      },
      // Bypasses due to local bypass rules should be classified as
      // SHORT_BYPASS.
      {
          GURL("http://localbypass.com"), net::ProxyServer::Direct(),
          base::TimeDelta(), net::LOAD_NORMAL, "HTTP/1.1 200 OK\r\n\r\n",
          SHORT_BYPASS,
      },
      {
          GURL("http://foo.com"), net::ProxyServer::Direct(), base::TimeDelta(),
          net::LOAD_NORMAL, "HTTP/1.1 200 OK\r\n\r\n", DIRECT_HTTP,
      },
      // Responses that seem like they should have come through the Data
      // Reduction Proxy, but did not, should be classified as UNKNOWN_TYPE.
      {
          GURL("http://foo.com"), origin, base::TimeDelta(), net::LOAD_NORMAL,
          "HTTP/1.1 200 OK\r\n\r\n", UNKNOWN_TYPE,
      },
      // The proxy is currently bypassed, but the response still came through
      // the Data Reduction Proxy, so it should be classified as
      // VIA_DATA_REDUCTION_PROXY.
      {
          GURL("http://foo.com"), origin, base::TimeDelta::FromMinutes(60),
          net::LOAD_NORMAL,
          "HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n",
          VIA_DATA_REDUCTION_PROXY,
      },
      // The request was sent over a direct connection, but the response still
      // has the Data Reduction Proxy Via header, so it should be classified as
      // VIA_DATA_REDUCTION_PROXY.
      {
          GURL("http://foo.com"), net::ProxyServer::Direct(), base::TimeDelta(),
          net::LOAD_NORMAL,
          "HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n",
          VIA_DATA_REDUCTION_PROXY,
      },
      // A 304 response with a DRP Via header should be classified as
      // VIA_DATA_REDUCTION_PROXY.
      {
          GURL("http://foo.com"), net::ProxyServer::Direct(), base::TimeDelta(),
          net::LOAD_NORMAL,
          "HTTP/1.1 304 Not Modified\r\n"
          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
          VIA_DATA_REDUCTION_PROXY,
      },
      // A 304 response without a DRP Via header where the request was sent
      // through the DRP should be classified as VIA_DATA_REDUCTION_PROXY.
      {
          GURL("http://foo.com"), origin, base::TimeDelta(), net::LOAD_NORMAL,
          "HTTP/1.1 304 Not Modified\r\n\r\n", VIA_DATA_REDUCTION_PROXY,
      },
  };

  for (const TestCase& test_case : test_cases) {
    net::TestURLRequestContext context(true);
    net::MockClientSocketFactory mock_socket_factory;
    context.set_client_socket_factory(&mock_socket_factory);
    // Set the |proxy_resolution_service| to use |test_case.proxy_server| for requests.
    std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service(
        net::ProxyResolutionService::CreateFixedFromPacResult(
            test_case.proxy_server.ToPacString(),
            TRAFFIC_ANNOTATION_FOR_TESTS));
    context.set_proxy_resolution_service(proxy_resolution_service.get());
    context.Init();

    // Create a fake URLRequest and fill it with the appropriate response
    // headers and proxy server by executing it with fake socket data.
    net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);
    if (test_case.url.SchemeIsCryptographic())
      mock_socket_factory.AddSSLSocketDataProvider(&ssl_socket_data_provider);
    MockRead mock_reads[] = {
        MockRead(test_case.response_headers),
        MockRead("hello world"),
        MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider socket_data_provider(
        mock_reads, base::span<net::MockWrite>());
    mock_socket_factory.AddSocketDataProvider(&socket_data_provider);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request = context.CreateRequest(
        test_case.url, net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    request->SetLoadFlags(test_case.load_flags);
    request->Start();
    test_context->RunUntilIdle();

    // Mark the Data Reduction Proxy as bad if the test specifies to.
    if (test_case.bypass_duration > base::TimeDelta()) {
      net::ProxyInfo proxy_info;
      proxy_info.UseProxyList(
          data_reduction_proxy_config.proxy_rules().proxies_for_http);
      EXPECT_TRUE(context.proxy_resolution_service()->MarkProxiesAsBadUntil(
          proxy_info, test_case.bypass_duration,
          std::vector<net::ProxyServer>(), net::NetLogWithSource()));
    }

    EXPECT_EQ(test_case.expected_request_type,
              GetDataReductionProxyRequestType(
                  *request, data_reduction_proxy_config,
                  *test_context->config()));
  }
}

}  // namespace data_reduction_proxy
