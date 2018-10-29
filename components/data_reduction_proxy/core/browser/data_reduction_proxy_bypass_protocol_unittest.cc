// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_delegate.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_transaction_test_util.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using net::HostPortPair;
using net::HttpResponseHeaders;
using net::MockRead;
using net::MockWrite;
using net::ProxyRetryInfoMap;
using net::ProxyResolutionService;
using net::StaticSocketDataProvider;
using net::TestDelegate;
using net::TestURLRequestContext;
using net::URLRequest;

namespace data_reduction_proxy {

class SimpleURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return net::URLRequestHttpJob::Factory(request, network_delegate, "http");
  }
};

// Fetches resources from the embedded test server.
class DataReductionProxyProtocolEmbeddedServerTest : public testing::Test {
 public:
  DataReductionProxyProtocolEmbeddedServerTest() {
    embedded_test_server_.RegisterRequestHandler(
        base::Bind(&DataReductionProxyProtocolEmbeddedServerTest::HandleRequest,
                   base::Unretained(this)));
  }

  ~DataReductionProxyProtocolEmbeddedServerTest() override {}

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Send null response headers.
    return std::unique_ptr<net::test_server::HttpResponse>(
        new net::test_server::RawHttpResponse("", ""));
  }

  void SetUp() override {
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
    test_context_ = DataReductionProxyTestContext::Builder()
                        .SkipSettingsInitialization()
                        .Build();
    test_context_->DisableWarmupURLFetch();
    // Since some of the tests fetch a webpage from the embedded server running
    // on localhost, the adding of default bypass rules is disabled. This allows
    // Chrome to fetch webpages using data saver proxy.
    test_context_->config()->SetShouldAddDefaultProxyBypassRules(false);
    test_context_->InitSettingsWithoutCheck();

    test_context_->RunUntilIdle();
  }

  // Sets up the |TestURLRequestContext| with the provided
  // |ProxyResolutionService|.
  void ConfigureTestDependencies(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service) {
    // Create a context with delayed initialization.
    context_.reset(new TestURLRequestContext(true));

    proxy_resolution_service_ = std::move(proxy_resolution_service);
    context_->set_proxy_resolution_service(proxy_resolution_service_.get());

    DataReductionProxyInterceptor* interceptor =
        new DataReductionProxyInterceptor(
            test_context_->config(), test_context_->io_data()->config_client(),
            nullptr /* bypass_stats */);

    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
        new net::URLRequestJobFactoryImpl());

    job_factory_.reset(new net::URLRequestInterceptingJobFactory(
        std::move(job_factory_impl), base::WrapUnique(interceptor)));

    context_->set_job_factory(job_factory_.get());

    proxy_delegate_ = test_context_->io_data()->CreateProxyDelegate();

    context_->Init();

    context_->proxy_resolution_service()->SetProxyDelegate(
        proxy_delegate_.get());
  }

 protected:
  base::MessageLoopForIO message_loop_;
  net::EmbeddedTestServer embedded_test_server_;

  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;

  std::unique_ptr<net::URLRequestInterceptingJobFactory> job_factory_;
  std::unique_ptr<TestURLRequestContext> context_;
  std::unique_ptr<net::ProxyDelegate> proxy_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyProtocolEmbeddedServerTest);
};

// Tests that if the embedded test server resets the connection after accepting
// it, then the data saver proxy is bypassed, and the request is retried.
TEST_F(DataReductionProxyProtocolEmbeddedServerTest,
       EmbeddedTestServerBypassRetryOnPostConnectionErrors) {
  base::HistogramTester histogram_tester;
  embedded_test_server_.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server_.Start());

  test_context_->config()->test_params()->UseNonSecureProxiesForHttp();
  test_context_->SetDataReductionProxyEnabled(true);
  net::ProxyServer proxy_server(net::ProxyServer::SCHEME_HTTP,
                                embedded_test_server_.host_port_pair());

  ASSERT_TRUE(proxy_server.is_http());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy, proxy_server.host_port_pair().ToString());
  test_context_->config()->ResetParamFlagsForTest();
  ConfigureTestDependencies(ProxyResolutionService::CreateDirect());

  test_context_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  {
    const GURL url = embedded_test_server_.GetURL("/simple.html");
    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> url_request(context_->CreateRequest(
        url, net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    url_request->Start();
    while (!url_request->status().is_success()) {
      // Need to pump the thread for the embedded server and the DRP thread.
      test_context_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
    }
    EXPECT_FALSE(url_request->proxy_server().is_http());
    // The proxy should have been marked as bad.
    ProxyRetryInfoMap retry_info =
        proxy_resolution_service_->proxy_retry_info();
    while (retry_info.size() != 1) {
      test_context_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
      retry_info = proxy_resolution_service_->proxy_retry_info();
    }

    EXPECT_LE(base::TimeDelta::FromMinutes(4),
              retry_info.begin()->second.current_delay);
  }

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.InvalidResponseHeadersReceived.NetError",
      std::abs(net::ERR_EMPTY_RESPONSE), 1);

  {
    // Second request should be fetched directly.
    const GURL url = embedded_test_server_.GetURL("/simple2.html");
    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> url_request(context_->CreateRequest(
        url, net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    url_request->Start();
    while (!(url_request->status().is_success())) {
      // Need to pump the thread for the embedded server and the DRP thread.
      test_context_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
    }
    EXPECT_TRUE(!url_request->proxy_server().is_valid() ||
                url_request->proxy_server().is_direct());
    // The proxy should still be marked as bad.
    ProxyRetryInfoMap retry_info =
        proxy_resolution_service_->proxy_retry_info();
    while (retry_info.size() != 1) {
      test_context_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
      retry_info = proxy_resolution_service_->proxy_retry_info();
    }

    EXPECT_LE(base::TimeDelta::FromMinutes(4),
              retry_info.begin()->second.current_delay);
  }
}

// Constructs a |TestURLRequestContext| that uses a |MockSocketFactory| to
// simulate requests and responses.
class DataReductionProxyProtocolTest : public testing::Test {
 public:
  DataReductionProxyProtocolTest() : http_user_agent_settings_("", "") {
    simple_interceptor_.reset(new SimpleURLRequestInterceptor());
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "http", "www.google.com", std::move(simple_interceptor_));
  }

  ~DataReductionProxyProtocolTest() override {
    // URLRequestJobs may post clean-up tasks on destruction.
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(
            "http", "www.google.com");
    test_context_->RunUntilIdle();
  }

  void SetUp() override {
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
    test_context_ = DataReductionProxyTestContext::Builder().Build();
    network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
    test_context_->RunUntilIdle();
  }

  // Sets up the |TestURLRequestContext| with the provided
  // |ProxyResolutionService|.
  void ConfigureTestDependencies(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service,
      bool use_mock_socket_factory,
      bool use_drp_proxy_delegate,
      bool use_test_network_delegate) {
    // Create a context with delayed initialization.
    context_.reset(new TestURLRequestContext(true));

    proxy_resolution_service_ = std::move(proxy_resolution_service);
    if (use_mock_socket_factory) {
      context_->set_client_socket_factory(&mock_socket_factory_);
    }
    context_->set_proxy_resolution_service(proxy_resolution_service_.get());
    if (use_test_network_delegate) {
      network_delegate_.reset(new net::TestNetworkDelegate());
      context_->set_network_delegate(network_delegate_.get());
    }
    // This is needed to prevent the test context from adding language headers
    // to requests.
    context_->set_http_user_agent_settings(&http_user_agent_settings_);
    bypass_stats_.reset(new DataReductionProxyBypassStats(
        test_context_->config(), test_context_->unreachable_callback(),
        network::TestNetworkConnectionTracker::GetInstance()));

    DataReductionProxyInterceptor* interceptor =
        new DataReductionProxyInterceptor(
            test_context_->config(), test_context_->io_data()->config_client(),
            bypass_stats_.get());
    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
        new net::URLRequestJobFactoryImpl());
    job_factory_.reset(new net::URLRequestInterceptingJobFactory(
        std::move(job_factory_impl), base::WrapUnique(interceptor)));

    context_->set_job_factory(job_factory_.get());

    context_->Init();

    if (use_drp_proxy_delegate) {
      proxy_delegate_ = test_context_->io_data()->CreateProxyDelegate();
      context_->proxy_resolution_service()->SetProxyDelegate(
          proxy_delegate_.get());
    }
  }

  // Simulates a request to a data reduction proxy that may result in bypassing
  // the proxy and retrying the the request.
  // Runs a test with the given request |method| that expects the first response
  // from the server to be |first_response|. If |expected_retry|, the test
  // will expect a retry of the request. A response body will be expected
  // if |expect_response_body|. |net_error_code| is the error code returned if
  // |generate_response_error| is true. |expect_final_error| is true if the
  // request is expected to finish with an error.
  void TestProxyFallback(const char* method,
                         const char* first_response,
                         bool expected_retry,
                         bool generate_response_error,
                         size_t expected_bad_proxy_count,
                         bool expect_response_body,
                         int net_error_code,
                         bool expect_final_error,
                         int expect_response_header_count) {
    std::string m(method);
    std::string trailer =
        (m == "PUT" || m == "POST") ? "Content-Length: 0\r\n" : "";

    std::string request1 = base::StringPrintf(
        "%s http://www.google.com/ HTTP/1.1\r\n"
        "Host: www.google.com\r\n"
        "Proxy-Connection: keep-alive\r\n%s"
        "User-Agent: \r\n"
        "Accept-Encoding: gzip, deflate\r\n\r\n",
        method, trailer.c_str());

    std::string payload1 =
        (expected_retry ? "Bypass message" : "content");

    MockWrite data_writes[] = {
      MockWrite(request1.c_str()),
    };

    MockRead data_reads[] = {
      MockRead(first_response),
      MockRead(payload1.c_str()),
      MockRead(net::SYNCHRONOUS, net::OK),
    };
    MockRead data_reads_error[] = {
        MockRead(net::SYNCHRONOUS, net_error_code),
    };

    StaticSocketDataProvider data1(data_reads, data_writes);
    StaticSocketDataProvider data1_error(data_reads_error, data_writes);
    if (!generate_response_error)
      mock_socket_factory_.AddSocketDataProvider(&data1);
    else
      mock_socket_factory_.AddSocketDataProvider(&data1_error);

    std::string response2;
    std::string request2;
    std::string response2_via_header;
    std::string request2_connection_type;
    std::string request2_path = "/";

    if (expected_bad_proxy_count == 1) {
      request2_path = "http://www.google.com/";
      request2_connection_type = "Proxy-";
      response2_via_header = "Via: 1.1 Chrome-Compression-Proxy\r\n";
    }

    std::string request2_prefix = base::StringPrintf(
        "%s %s HTTP/1.1\r\n"
        "Host: www.google.com\r\n"
        "%sConnection: keep-alive\r\n%s",
        method, request2_path.c_str(), request2_connection_type.c_str(),
        trailer.c_str());

    // Cache headers are set only if the request was intercepted and retried by
    // data reduction proxy. If the request was restarted by the network stack,
    // then the cache headers are unset.
    std::string request2_middle = expected_bad_proxy_count == 0
                                      ? "Pragma: no-cache\r\n"
                                        "Cache-Control: no-cache\r\n"
                                      : "";

    std::string request2_suffix =
        "User-Agent: \r\n"
        "Accept-Encoding: gzip, deflate\r\n\r\n";

    request2 = request2_prefix + request2_middle + request2_suffix;

    response2 = base::StringPrintf(
        "HTTP/1.0 200 OK\r\n"
        "Server: foo\r\n%s\r\n", response2_via_header.c_str());

    MockWrite data_writes2[] = {
      MockWrite(request2.c_str()),
    };

    MockRead data_reads2[] = {
      MockRead(response2.c_str()),
      MockRead("content"),
      MockRead(net::SYNCHRONOUS, net::OK),
    };

    StaticSocketDataProvider data2(data_reads2, data_writes2);
    if (expected_retry) {
      mock_socket_factory_.AddSocketDataProvider(&data2);
    }

    // Expect that we get "content" and not "Bypass message", and that there's
    // a "not-proxy" "Server:" header in the final response.
    ExecuteRequestExpectingContentAndHeader(
        method, (expect_response_body ? "content" : ""), expected_retry,
        expect_final_error, net_error_code, expect_response_header_count);
  }

  // Starts a request with the given |method| and checks that the response
  // contains |content|.
  void ExecuteRequestExpectingContentAndHeader(
      const std::string& method,
      const std::string& content,
      bool expected_retry,
      bool expected_error,
      int net_error_code,
      int expect_response_header_count) {
    int initial_headers_received_count =
        network_delegate_ ? network_delegate_->headers_received_count() : 0;
    TestDelegate d;
    std::unique_ptr<URLRequest> r(context_->CreateRequest(
        GURL("http://www.google.com/"), net::DEFAULT_PRIORITY, &d,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    r->set_method(method);
    r->SetLoadFlags(net::LOAD_NORMAL);

    r->Start();
    base::RunLoop().Run();

    if (!expected_error) {
      EXPECT_EQ(net::OK, d.request_status());
      if (network_delegate_) {
        EXPECT_EQ(initial_headers_received_count + expect_response_header_count,
                  network_delegate_->headers_received_count());
      }
      EXPECT_EQ(content, d.data_received());
      return;
    }

    EXPECT_EQ(net_error_code, d.request_status());
    if (network_delegate_) {
      EXPECT_EQ(initial_headers_received_count,
                network_delegate_->headers_received_count());
    }
  }

  // Returns the key to the |ProxyRetryInfoMap|.
  std::string GetProxyKey(const std::string& proxy) {
    net::ProxyServer proxy_server = net::ProxyServer::FromURI(
        proxy, net::ProxyServer::SCHEME_HTTP);
    if (!proxy_server.is_valid())
      return HostPortPair::FromURL(GURL(std::string())).ToString();
    return proxy_server.host_port_pair().ToString();
  }

  // Checks that |expected_num_bad_proxies| proxies are on the proxy retry list.
  // If the list has one proxy, it should match |bad_proxy|. If it has two
  // proxies, it should match |bad_proxy| and |bad_proxy2|. Checks also that
  // the current delay associated with each bad proxy is |duration_seconds|.
  void TestBadProxies(unsigned int expected_num_bad_proxies,
                      int duration_seconds,
                      const std::string& bad_proxy,
                      const std::string& bad_proxy2) {
    const ProxyRetryInfoMap& retry_info =
        proxy_resolution_service_->proxy_retry_info();
    ASSERT_EQ(expected_num_bad_proxies, retry_info.size());

    base::TimeDelta expected_min_duration;
    base::TimeDelta expected_max_duration;
    if (duration_seconds == 0) {
      expected_min_duration = base::TimeDelta::FromMinutes(1);
      expected_max_duration = base::TimeDelta::FromMinutes(5);
    } else {
      expected_min_duration = base::TimeDelta::FromSeconds(duration_seconds);
      expected_max_duration = base::TimeDelta::FromSeconds(duration_seconds);
    }

    if (expected_num_bad_proxies >= 1u) {
      auto i = retry_info.find(GetProxyKey(bad_proxy));
      ASSERT_TRUE(i != retry_info.end());
      EXPECT_TRUE(expected_min_duration <= (*i).second.current_delay);
      EXPECT_TRUE((*i).second.current_delay <= expected_max_duration);
    }
    if (expected_num_bad_proxies == 2u) {
      auto i = retry_info.find(GetProxyKey(bad_proxy2));
      ASSERT_TRUE(i != retry_info.end());
      EXPECT_TRUE(expected_min_duration <= (*i).second.current_delay);
      EXPECT_TRUE((*i).second.current_delay <= expected_max_duration);
    }
  }

 protected:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  std::unique_ptr<net::URLRequestInterceptor> simple_interceptor_;
  net::MockClientSocketFactory mock_socket_factory_;
  std::unique_ptr<net::TestNetworkDelegate> network_delegate_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxyBypassStats> bypass_stats_;
  net::StaticHttpUserAgentSettings http_user_agent_settings_;

  std::unique_ptr<net::URLRequestInterceptingJobFactory> job_factory_;
  std::unique_ptr<TestURLRequestContext> context_;
  std::unique_ptr<net::ProxyDelegate> proxy_delegate_;
};

// Tests that request are deemed idempotent or not according to the method used.
TEST_F(DataReductionProxyProtocolTest, TestIdempotency) {
  net::TestURLRequestContext context;
  const struct {
    const char* method;
    bool expected_result;
  } tests[] = {
      { "GET", true },
      { "OPTIONS", true },
      { "HEAD", true },
      { "PUT", true },
      { "DELETE", true },
      { "TRACE", true },
      { "POST", false },
      { "CONNECT", false },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request(context.CreateRequest(
        GURL("http://www.google.com/"), net::DEFAULT_PRIORITY, nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    request->set_method(tests[i].method);
    EXPECT_EQ(tests[i].expected_result,
              net::HttpUtil::IsMethodIdempotent(request->method()));
  }
}

// Tests that if the connection is reset, then the proxy is bypassed, and
// request is retried with the next proxy.
TEST_F(DataReductionProxyProtocolTest, BypassRetryOnPostConnectionErrors) {
  const struct {
    const char* method;
    const char* first_response;
    bool expected_retry;
    bool generate_response_error;
    size_t expected_bad_proxy_count;
    bool expect_response_body;
    int expected_duration;
    DataReductionProxyBypassType expected_bypass_type;
  } tests[] = {
      {
          "GET", "Connection reset after accept", true, true, 1u, true,
          300 /* 5 minutes */, BYPASS_EVENT_TYPE_MAX,
      },
  };
  test_context_->config()->test_params()->UseNonSecureProxiesForHttp();
  std::string primary = test_context_->config()
                            ->test_params()
                            ->proxies_for_http()
                            .front()
                            .proxy_server()
                            .host_port_pair()
                            .ToString();
  std::string fallback = test_context_->config()
                             ->test_params()
                             ->proxies_for_http()
                             .at(1)
                             .proxy_server()
                             .host_port_pair()
                             .ToString();
  for (size_t i = 0; i < arraysize(tests); ++i) {
    base::HistogramTester histogram_tester;

    ConfigureTestDependencies(
        ProxyResolutionService::CreateFixedFromPacResult(
            net::ProxyServer::FromURI(primary, net::ProxyServer::SCHEME_HTTP)
                    .ToPacString() +
                "; " +
                net::ProxyServer::FromURI(fallback,
                                          net::ProxyServer::SCHEME_HTTP)
                    .ToPacString() +
                "; DIRECT",
            TRAFFIC_ANNOTATION_FOR_TESTS),
        true /* use_mock_socket_factory */, false /* use_drp_proxy_delegate */,
        false /* use_test_network_delegate */);
    // Only 1 set of valid response headers are expected since the proxy
    // connection is reset before the response headers are received.
    TestProxyFallback(
        tests[i].method, tests[i].first_response, tests[i].expected_retry,
        tests[i].generate_response_error, tests[i].expected_bad_proxy_count,
        tests[i].expect_response_body, net::ERR_CONNECTION_RESET, false, 1);
    EXPECT_EQ(tests[i].expected_bypass_type, bypass_stats_->GetBypassType());
    // The proxy should have been marked as bad.
    TestBadProxies(tests[i].expected_bad_proxy_count,
                   tests[i].expected_duration, primary, fallback);

    ProxyRetryInfoMap retry_info =
        proxy_resolution_service_->proxy_retry_info();
    while (retry_info.size() != 1) {
      test_context_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
      retry_info = proxy_resolution_service_->proxy_retry_info();
    }

    EXPECT_LE(base::TimeDelta::FromMinutes(4),
              retry_info.begin()->second.current_delay);
    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.InvalidResponseHeadersReceived.NetError",
        std::abs(net::ERR_CONNECTION_RESET), 1);
  }
}

// After each test, the proxy retry info will contain zero, one, or two of the
// data reduction proxies depending on whether no bypass was indicated by the
// initial response, a single proxy bypass was indicated, or a double bypass
// was indicated. In both the single and double bypass cases, if the request
// was idempotent, it will be retried over a direct connection.
TEST_F(DataReductionProxyProtocolTest, BypassLogic) {
  // The test manually controls the fetch of warmup URL and the response.
  test_context_->DisableWarmupURLFetchCallback();

  const struct {
    const char* method;
    const char* first_response;
    bool expected_retry;
    bool generate_response_error;
    size_t expected_bad_proxy_count;
    bool expect_response_body;
    int expected_duration;
    DataReductionProxyBypassType expected_bypass_type;
  } tests[] = {
    // Valid data reduction proxy response with no bypass message.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      false,
      false,
      0u,
      true,
      -1,
      BYPASS_EVENT_TYPE_MAX,
    },
    // Response error does not result in bypass.
    { "GET",
      "Not an HTTP response",
      false,
      true,
      0u,
      true,
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
      false,
      0u,
      true,
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
      false,
      1u,
      true,
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
      false,
      1u,
      true,
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
      false,
      1u,
      true,
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
      false,
      1u,
      false,
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
      false,
      1u,
      true,
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
      false,
      1u,
      true,
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
      false,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MEDIUM
    },
    // 500 responses should be bypassed.
    { "GET",
      "HTTP/1.1 500 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      false,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_STATUS_500_HTTP_INTERNAL_SERVER_ERROR
    },
    // 502 responses should be bypassed.
    { "GET",
      "HTTP/1.1 502 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      false,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY
    },
    // 503 responses should be bypassed.
    { "GET",
      "HTTP/1.1 503 Internal Server Error\r\n"
      "Server: proxy\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      true,
      false,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_STATUS_503_HTTP_SERVICE_UNAVAILABLE
    },
    // Invalid data reduction proxy 4xx response. Missing Via header.
    { "GET",
      "HTTP/1.1 404 Not Found\r\n"
      "Server: proxy\r\n\r\n",
      true,
      false,
      0u,
      true,
      0,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_4XX
    },
    // Invalid data reduction proxy response. Missing Via header.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n\r\n",
      true,
      false,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER
    },
    // Invalid data reduction proxy response. Wrong Via header.
    { "GET",
      "HTTP/1.1 200 OK\r\n"
      "Server: proxy\r\n"
      "Via: 1.0 some-other-proxy\r\n\r\n",
      true,
      false,
      1u,
      true,
      0,
      BYPASS_EVENT_TYPE_MISSING_VIA_HEADER_OTHER
    },
    // Valid data reduction proxy response. 304 missing Via header.
    { "GET",
      "HTTP/1.1 304 Not Modified\r\n"
      "Server: proxy\r\n\r\n",
      false,
      false,
      0u,
      false,
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
      false,
      1u,
      true,
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
      false,
      2u,
      true,
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
      false,
      0u,
      true,
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
      false,
      0u,
      true,
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
      false,
      0u,
      false,
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
      false,
      0u,
      true,
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
      false,
      0u,
      true,
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
      false,
      0u,
      true,
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
      false,
      0u,
      true,
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
      false,
      2u,
      true,
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
      false,
      2u,
      true,
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
      false,
      1u,
      true,
      1,
      BYPASS_EVENT_TYPE_SHORT
    },
  };
  test_context_->config()->test_params()->UseNonSecureProxiesForHttp();
  std::string primary = test_context_->config()
                            ->test_params()
                            ->proxies_for_http()
                            .front()
                            .proxy_server()
                            .host_port_pair()
                            .ToString();
  std::string fallback = test_context_->config()
                             ->test_params()
                             ->proxies_for_http()
                             .at(1)
                             .proxy_server()
                             .host_port_pair()
                             .ToString();
  for (size_t i = 0; i < arraysize(tests); ++i) {
    ConfigureTestDependencies(
        ProxyResolutionService::CreateFixedFromPacResult(
            net::ProxyServer::FromURI(primary, net::ProxyServer::SCHEME_HTTP)
                    .ToPacString() +
                "; " +
                net::ProxyServer::FromURI(fallback,
                                          net::ProxyServer::SCHEME_HTTP)
                    .ToPacString() +
                "; DIRECT",
            TRAFFIC_ANNOTATION_FOR_TESTS),
        true /* use_mock_socket_factory */, false /* use_drp_proxy_delegate */,
        true /* use_test_network_delegate */);
    TestProxyFallback(
        tests[i].method, tests[i].first_response, tests[i].expected_retry,
        tests[i].generate_response_error, tests[i].expected_bad_proxy_count,
        tests[i].expect_response_body, net::ERR_INTERNET_DISCONNECTED,
        tests[i].generate_response_error, tests[i].expected_retry ? 2 : 1);
    EXPECT_EQ(tests[i].expected_bypass_type, bypass_stats_->GetBypassType());
    // We should also observe the bad proxy in the retry list.
    TestBadProxies(tests[i].expected_bad_proxy_count,
                   tests[i].expected_duration,
                   primary, fallback);
  }
}

TEST_F(DataReductionProxyProtocolTest,
       ProxyBypassIgnoredOnDirectConnection) {
  // Verify that a Chrome-Proxy header is ignored when returned from a directly
  // connected origin server.
  ConfigureTestDependencies(ProxyResolutionService::CreateDirect(),
                            true /* use_mock_socket_factory */,
                            false /* use_drp_proxy_delegate */,
                            true /* use_test_network_delegate */);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"
             "Chrome-Proxy: bypass=0\r\n\r\n"),
    MockRead("Bypass message"),
    MockRead(net::SYNCHRONOUS, net::OK),
  };
  MockWrite data_writes[] = {
      MockWrite("GET / HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "User-Agent: \r\n"
                "Accept-Encoding: gzip, deflate\r\n\r\n"),
  };
  StaticSocketDataProvider data1(data_reads, data_writes);
  mock_socket_factory_.AddSocketDataProvider(&data1);

  TestDelegate d;
  std::unique_ptr<URLRequest> r(context_->CreateRequest(
      GURL("http://www.google.com/"), net::DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  r->set_method("GET");
  r->SetLoadFlags(net::LOAD_NORMAL);

  r->Start();
  base::RunLoop().Run();

  EXPECT_EQ(net::OK, d.request_status());

  EXPECT_EQ("Bypass message", d.data_received());

  // We should have no entries in our bad proxy list.
  TestBadProxies(0, -1, "", "");
}

class DataReductionProxyBypassProtocolEndToEndTest : public testing::Test {
 public:
  DataReductionProxyBypassProtocolEndToEndTest() {}

  void ResetDependencies() {
    drp_test_context_.reset();
    mock_socket_factory_.reset();
    storage_.reset();

    context_.reset(new net::TestURLRequestContext(true));
    storage_.reset(new net::URLRequestContextStorage(context_.get()));
    mock_socket_factory_.reset(new net::MockClientSocketFactory());
    context_->set_client_socket_factory(mock_socket_factory_.get());
    drp_test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithMockClientSocketFactory(mock_socket_factory_.get())
            .WithURLRequestContext(context_.get())
            .Build();
  }

  void AttachToContextAndInit() {
    drp_test_context_->AttachToURLRequestContext(storage_.get());
    context_->Init();
    context_->proxy_resolution_service()->SetProxyDelegate(
        drp_test_context_->io_data()->proxy_delegate());
  }

  net::TestURLRequestContext* context() { return context_.get(); }
  net::URLRequestContextStorage* storage() { return storage_.get(); }
  net::MockClientSocketFactory* mock_socket_factory() {
    return mock_socket_factory_.get();
  }
  DataReductionProxyTestContext* drp_test_context() {
    return drp_test_context_.get();
  }

 private:
  base::MessageLoopForIO loop_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  std::unique_ptr<net::URLRequestContextStorage> storage_;
  std::unique_ptr<net::MockClientSocketFactory> mock_socket_factory_;
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyBypassProtocolEndToEndTest);
};

TEST_F(DataReductionProxyBypassProtocolEndToEndTest,
       BypassLogicAlwaysAppliesWhenViaHeaderPresent) {
  const struct {
    const char* first_response;
    bool expected_retry;
    bool expected_bad_proxy;
    DataReductionProxyBypassType expected_bypass_type;
  } test_cases[] = {
      {"HTTP/1.1 200 OK\r\n"
       "Server: proxy\r\n"
       "Chrome-Proxy: block=0\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
       true, true, BYPASS_EVENT_TYPE_MEDIUM},
      {"HTTP/1.1 200 OK\r\n"
       "Server: proxy\r\n"
       "Chrome-Proxy: bypass=0\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
       true, true, BYPASS_EVENT_TYPE_MEDIUM},
      {"HTTP/1.1 502 Bad Gateway\r\n"
       "Server: proxy\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
       true, true, BYPASS_EVENT_TYPE_STATUS_502_HTTP_BAD_GATEWAY},
      {"HTTP/1.1 200 OK\r\n"
       "Server: proxy\r\n"
       "Chrome-Proxy: block=0\r\n\r\n",
       false, false, BYPASS_EVENT_TYPE_MAX},
      {"HTTP/1.1 502 Bad Gateway\r\n"
       "Server: proxy\r\n\r\n",
       false, false, BYPASS_EVENT_TYPE_MAX},
  };

  for (const auto& test : test_cases) {
    const std::string kPrimary = "https://unrecognized-drp.net:443";

    ResetDependencies();
    storage()->set_proxy_resolution_service(ProxyResolutionService::CreateFixed(
        kPrimary + ",direct://", TRAFFIC_ANNOTATION_FOR_TESTS));
    AttachToContextAndInit();

    // The proxy is an HTTPS proxy, so set up the fake SSL socket data.
    net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
    mock_socket_factory()->AddSSLSocketDataProvider(&ssl_socket);

    MockRead first_reads[] = {MockRead(test.first_response), MockRead(""),
                              MockRead(net::SYNCHRONOUS, net::OK)};
    net::StaticSocketDataProvider first_socket(first_reads,
                                               base::span<MockWrite>());
    mock_socket_factory()->AddSocketDataProvider(&first_socket);

    MockRead retry_reads[] = {MockRead("HTTP/1.1 200 OK\n\r\n\r"), MockRead(""),
                              MockRead(net::SYNCHRONOUS, net::OK)};
    net::StaticSocketDataProvider retry_socket(retry_reads,
                                               base::span<MockWrite>());
    if (test.expected_retry)
      mock_socket_factory()->AddSocketDataProvider(&retry_socket);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> url_request(
        context()->CreateRequest(GURL("http://www.google.com"), net::IDLE,
                                 &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    url_request->Start();
    drp_test_context()->RunUntilIdle();

    EXPECT_EQ(test.expected_bypass_type,
              drp_test_context()->io_data()->bypass_stats()->GetBypassType());
    // Check the bad proxy list.
    EXPECT_EQ(test.expected_bad_proxy,
              base::ContainsKey(
                  context()->proxy_resolution_service()->proxy_retry_info(),
                  kPrimary));
  }
}

TEST_F(DataReductionProxyBypassProtocolEndToEndTest,
       ResponseProxyServerStateHistogram) {
  const struct {
    const char* proxy_rules;
    bool enable_data_reduction_proxy;
    const char* response_headers;
    // |RESPONSE_PROXY_SERVER_STATUS_MAX| indicates no expected value.
    DataReductionProxyBypassProtocol::ResponseProxyServerStatus expected_status;
  } test_cases[] = {
      {"direct://", false, "HTTP/1.1 200 OK\r\n\r\n",
       DataReductionProxyBypassProtocol::RESPONSE_PROXY_SERVER_STATUS_EMPTY},
      {"direct://", true,
       "HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n",
       DataReductionProxyBypassProtocol::RESPONSE_PROXY_SERVER_STATUS_DRP},
      {"unrecognized-drp.net", false, "HTTP/1.1 200 OK\r\n\r\n",
       DataReductionProxyBypassProtocol::
           RESPONSE_PROXY_SERVER_STATUS_NON_DRP_NO_VIA},
      {"unrecognized-drp.net", false,
       "HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n",
       DataReductionProxyBypassProtocol::
           RESPONSE_PROXY_SERVER_STATUS_NON_DRP_WITH_VIA},
  };

  for (const auto& test : test_cases) {
    ResetDependencies();
    storage()->set_proxy_resolution_service(
        net::ProxyResolutionService::CreateFixed(test.proxy_rules,
                                                 TRAFFIC_ANNOTATION_FOR_TESTS));
    AttachToContextAndInit();
    if (test.enable_data_reduction_proxy) {
      drp_test_context()->DisableWarmupURLFetch();
      drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();
    }
    drp_test_context()->config()->test_params()->UseNonSecureProxiesForHttp();

    MockRead reads[] = {MockRead(test.response_headers), MockRead(""),
                        MockRead(net::SYNCHRONOUS, net::OK)};
    net::StaticSocketDataProvider socket(reads, base::span<MockWrite>());
    mock_socket_factory()->AddSocketDataProvider(&socket);

    base::HistogramTester histogram_tester;
    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request(
        context()->CreateRequest(GURL("http://google.com"), net::IDLE,
                                 &delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
    request->Start();
    drp_test_context()->RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.ResponseProxyServerStatus", test.expected_status,
        1);
  }
}

}  // namespace data_reduction_proxy
