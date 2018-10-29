// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::MockRead;
using net::MockWrite;
using testing::Return;

namespace data_reduction_proxy {

namespace {

const std::string kBody = "hello";
const std::string kNextBody = "hello again";
const std::string kErrorBody = "bad";

}  // namespace

class DataReductionProxyBypassStatsTest : public testing::Test {
 public:
  DataReductionProxyBypassStatsTest() : context_(true) {
    context_.Init();

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new net::TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
        url::kHttpScheme, base::WrapUnique(test_job_interceptor_)));

    context_.set_job_factory(&test_job_factory_);

    test_context_ =
        DataReductionProxyTestContext::Builder().WithMockConfig().Build();
    test_context_->DisableWarmupURLFetch();
    mock_url_request_ = context_.CreateRequest(GURL(), net::IDLE, &delegate_,
                                               TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  std::unique_ptr<net::URLRequest> CreateURLRequestWithResponseHeaders(
      const GURL& url,
      const std::string& response_headers) {
    std::unique_ptr<net::URLRequest> fake_request = context_.CreateRequest(
        url, net::IDLE, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);

    // Create a test job that will fill in the given response headers for the
    // |fake_request|.
    std::unique_ptr<net::URLRequestTestJob> test_job(new net::URLRequestTestJob(
        fake_request.get(), context_.network_delegate(), response_headers,
        std::string(), true));

    // Configure the interceptor to use the test job to handle the next request.
    test_job_interceptor_->set_main_intercept_job(std::move(test_job));
    fake_request->Start();
    test_context_->RunUntilIdle();

    EXPECT_TRUE(fake_request->response_headers() != nullptr);
    return fake_request;
  }

 protected:
  std::unique_ptr<DataReductionProxyBypassStats> BuildBypassStats() {
    return std::make_unique<DataReductionProxyBypassStats>(
        test_context_->config(), test_context_->unreachable_callback(),
        network::TestNetworkConnectionTracker::GetInstance());
  }

  MockDataReductionProxyConfig* config() const {
    return test_context_->mock_config();
  }

 private:
  base::MessageLoopForIO message_loop_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  std::unique_ptr<net::URLRequest> mock_url_request_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  net::TestJobInterceptor* test_job_interceptor_;
  net::URLRequestJobFactoryImpl test_job_factory_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
};


// End-to-end tests for the DataReductionProxy.BypassedBytes histograms.
class DataReductionProxyBypassStatsEndToEndTest : public testing::Test {
 public:
  DataReductionProxyBypassStatsEndToEndTest()
      : context_(true), context_storage_(&context_) {}

  ~DataReductionProxyBypassStatsEndToEndTest() override {
    drp_test_context_->io_data()->ShutdownOnUIThread();
    drp_test_context_->RunUntilIdle();
  }

  void SetUp() override {
    drp_test_context_ = DataReductionProxyTestContext::Builder()
                            .WithURLRequestContext(&context_)
                            .WithMockClientSocketFactory(&mock_socket_factory_)
                            .Build();
    drp_test_context_->DisableWarmupURLFetch();
    drp_test_context_->AttachToURLRequestContext(&context_storage_);
    context_.set_client_socket_factory(&mock_socket_factory_);
    proxy_delegate_ = drp_test_context_->io_data()->CreateProxyDelegate();

    // Only use the primary data reduction proxy in order to make it easier to
    // test bypassed bytes due to proxy fallbacks. This way, a test just needs
    // to cause one proxy fallback in order for the data reduction proxy to be
    // fully bypassed.
    std::vector<DataReductionProxyServer> data_reduction_proxy_servers;
    data_reduction_proxy_servers.push_back(DataReductionProxyServer(
        config()->test_params()->proxies_for_http().front().proxy_server(),
        ProxyServer::CORE));
    config()->test_params()->UseNonSecureProxiesForHttp();
    config()->test_params()->SetProxiesForHttp(data_reduction_proxy_servers);
  }

  // Create and execute a fake request using the data reduction proxy stack.
  // Passing in nullptr for |retry_response_headers| indicates that the request
  // is not expected to be retried.
  std::unique_ptr<net::URLRequest> CreateAndExecuteRequest(
      const GURL& url,
      int load_flags,
      net::Error finish_code,
      const char* initial_response_headers,
      const char* initial_response_body,
      const char* retry_response_headers,
      const char* retry_response_body) {
    // Support HTTPS URLs, and fetches over HTTPS proxy.
    net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC,
                                                        finish_code);
    if (url.SchemeIsCryptographic() ||
        (!config()->test_params()->proxies_for_http().empty() &&
         config()
             ->test_params()
             ->proxies_for_http()
             .front()
             .proxy_server()
             .is_https())) {
      mock_socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);
    }

    // Prepare for the initial response.
    MockRead initial_data_reads[] = {
        MockRead(initial_response_headers), MockRead(initial_response_body),
        MockRead(net::SYNCHRONOUS, finish_code),
    };
    net::StaticSocketDataProvider initial_socket_data_provider(
        initial_data_reads, base::span<net::MockWrite>());
    mock_socket_factory_.AddSocketDataProvider(&initial_socket_data_provider);

    // Prepare for the response from retrying the request, if applicable.
    // |retry_data_reads| and |retry_socket_data_provider| are out here so that
    // they stay in scope for when the request is executed.
    std::vector<MockRead> retry_data_reads;
    std::unique_ptr<net::StaticSocketDataProvider> retry_socket_data_provider;
    if (retry_response_headers) {
      retry_data_reads.push_back(MockRead(retry_response_headers));
      retry_data_reads.push_back(MockRead(retry_response_body));
      retry_data_reads.push_back(MockRead(net::SYNCHRONOUS, finish_code));

      retry_socket_data_provider.reset(new net::StaticSocketDataProvider(
          retry_data_reads, base::span<net::MockWrite>()));
      mock_socket_factory_.AddSocketDataProvider(
          retry_socket_data_provider.get());
    }

    std::unique_ptr<net::URLRequest> request(context_.CreateRequest(
        url, net::IDLE, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS));
    request->set_method("GET");
    request->SetLoadFlags(load_flags);
    request->Start();
    drp_test_context_->RunUntilIdle();
    return request;
  }

  // Create and execute a fake request that goes through a redirect loop using
  // the data reduction proxy stack.
  std::unique_ptr<net::URLRequest> CreateAndExecuteURLRedirectCycleRequest() {
    MockRead redirect_mock_reads_1[] = {
        MockRead("HTTP/1.1 302 Found\r\n"
                 "Via: 1.1 Chrome-Compression-Proxy\r\n"
                 "Location: http://bar.com/\r\n\r\n"),
        MockRead(""), MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider redirect_socket_data_provider_1(
        redirect_mock_reads_1, base::span<net::MockWrite>());
    mock_socket_factory_.AddSocketDataProvider(
        &redirect_socket_data_provider_1);

    // The response after the redirect comes through proxy.
    MockRead redirect_mock_reads_2[] = {
        MockRead("HTTP/1.1 302 Found\r\n"
                 "Via: 1.1 Chrome-Compression-Proxy\r\n"
                 "Location: http://foo.com/\r\n\r\n"),
        MockRead(""), MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider redirect_socket_data_provider_2(
        redirect_mock_reads_2, base::span<net::MockWrite>());
    mock_socket_factory_.AddSocketDataProvider(
        &redirect_socket_data_provider_2);

    // The response after the redirect comes through proxy and there is a
    // redirect cycle.
    MockRead redirect_mock_reads_3[] = {
        MockRead("HTTP/1.1 302 Found\r\n"
                 "Via: 1.1 Chrome-Compression-Proxy\r\n"
                 "Location: http://bar.com/\r\n\r\n"),
        MockRead(""), MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider redirect_socket_data_provider_3(
        redirect_mock_reads_3, base::span<net::MockWrite>());
    mock_socket_factory_.AddSocketDataProvider(
        &redirect_socket_data_provider_3);

    // Data reduction proxy should be bypassed, and the response should come
    // directly.
    MockRead response_mock_reads[] = {
        MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead(kBody.c_str()),
        MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider response_socket_data_provider(
        response_mock_reads, base::span<net::MockWrite>());
    mock_socket_factory_.AddSocketDataProvider(&response_socket_data_provider);

    std::unique_ptr<net::URLRequest> request(
        context_.CreateRequest(GURL("http://foo.com"), net::IDLE, &delegate_,
                               TRAFFIC_ANNOTATION_FOR_TESTS));
    request->set_method("GET");
    request->Start();
    drp_test_context_->RunUntilIdle();
    return request;
  }

  void set_proxy_resolution_service(net::ProxyResolutionService* proxy_resolution_service) {
    context_.set_proxy_resolution_service(proxy_resolution_service);
  }

  void set_host_resolver(net::HostResolver* host_resolver) {
    context_.set_host_resolver(host_resolver);
  }

  const DataReductionProxySettings* settings() const {
    return drp_test_context_->settings();
  }

  TestDataReductionProxyConfig* config() const {
    return drp_test_context_->config();
  }

  DataReductionProxyBypassStats* bypass_stats() const {
    return drp_test_context_->bypass_stats();
  }

  void ClearBadProxies() {
    context_.proxy_resolution_service()->ClearBadProxiesCache();
  }

  void InitializeContext() {
    context_.Init();
    context_.proxy_resolution_service()->SetProxyDelegate(
        proxy_delegate_.get());
    drp_test_context_->DisableWarmupURLFetch();
    drp_test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();
  }

  bool IsUnreachable() const {
    return drp_test_context_->settings()->IsDataReductionProxyUnreachable();
  }

  DataReductionProxyTestContext* drp_test_context() const {
    return drp_test_context_.get();
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::set<std::string>& excluded_histograms) const {
    const std::string kHistograms[] = {
        "DataReductionProxy.BypassedBytes.NotBypassed",
        "DataReductionProxy.BypassedBytes.SSL",
        "DataReductionProxy.BypassedBytes.LocalBypassRules",
        "DataReductionProxy.BypassedBytes.ProxyOverridden",
        "DataReductionProxy.BypassedBytes.Current",
        "DataReductionProxy.BypassedBytes.CurrentAudioVideo",
        "DataReductionProxy.BypassedBytes.CurrentApplicationOctetStream",
        "DataReductionProxy.BypassedBytes.ShortAll",
        "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
        "DataReductionProxy.BypassedBytes.ShortAudioVideo",
        "DataReductionProxy.BypassedBytes.MediumAll",
        "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
        "DataReductionProxy.BypassedBytes.LongAll",
        "DataReductionProxy.BypassedBytes.LongTriggeringRequest",
        "DataReductionProxy.BypassedBytes.MissingViaHeader4xx",
        "DataReductionProxy.BypassedBytes.MissingViaHeaderOther",
        "DataReductionProxy.BypassedBytes.Malformed407",
        "DataReductionProxy.BypassedBytes.Status500HttpInternalServerError",
        "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
        "DataReductionProxy.BypassedBytes.Status503HttpServiceUnavailable",
        "DataReductionProxy.BypassedBytes.NetworkErrorOther",
        "DataReductionProxy.BypassedBytes.RedirectCycle",
    };

    for (const std::string& histogram : kHistograms) {
      if (excluded_histograms.find(histogram) ==
          excluded_histograms.end()) {
        histogram_tester.ExpectTotalCount(histogram, 0);
      }
    }
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::string& excluded_histogram) const {
    std::set<std::string> excluded_histograms;
    excluded_histograms.insert(excluded_histogram);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            excluded_histograms);
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::string& first_excluded_histogram,
      const std::string& second_excluded_histogram) const {
    std::set<std::string> excluded_histograms;
    excluded_histograms.insert(first_excluded_histogram);
    excluded_histograms.insert(second_excluded_histogram);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            excluded_histograms);
  }

  net::TestDelegate* delegate() { return &delegate_; }

 private:
  base::MessageLoopForIO message_loop_;
  net::TestDelegate delegate_;
  net::MockClientSocketFactory mock_socket_factory_;
  net::TestURLRequestContext context_;
  net::URLRequestContextStorage context_storage_;
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context_;
  std::unique_ptr<net::ProxyDelegate> proxy_delegate_;
};

TEST_F(DataReductionProxyBypassStatsEndToEndTest, BypassedBytesNoRetry) {
  struct TestCase {
    GURL url;
    const char* histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { GURL("http://foo.com"),
      "DataReductionProxy.BypassedBytes.NotBypassed",
      "HTTP/1.1 200 OK\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
    },
    { GURL("https://foo.com"),
      "DataReductionProxy.BypassedBytes.SSL",
      "HTTP/1.1 200 OK\r\n\r\n",
    },
    { GURL("http://localhost"),
      "DataReductionProxy.BypassedBytes.LocalBypassRules",
      "HTTP/1.1 200 OK\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;
    CreateAndExecuteRequest(test_case.url, net::LOAD_NORMAL, net::OK,
                            test_case.initial_response_headers, kBody.c_str(),
                            nullptr, nullptr);

    histogram_tester.ExpectUniqueSample(test_case.histogram_name, kBody.size(),
                                        1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.histogram_name);
  }
}

// Verify that when there is a URL redirect cycle, data reduction proxy is
// bypassed for a single request.
TEST_F(DataReductionProxyBypassStatsEndToEndTest, URLRedirectCycle) {
  InitializeContext();
  ClearBadProxies();
  base::HistogramTester histogram_tester_1;
  CreateAndExecuteURLRedirectCycleRequest();

  histogram_tester_1.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.URLRedirectCycle", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester_1, "DataReductionProxy.BypassedBytes.URLRedirectCycle");

  // The second request should be sent via the proxy.
  base::HistogramTester histogram_tester_2;
  CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                          kNextBody.c_str(), nullptr, nullptr);
  histogram_tester_2.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.NotBypassed", kNextBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester_2, "DataReductionProxy.BypassedBytes.NotBypassed");
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesProxyOverridden) {
  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service(
      net::ProxyResolutionService::CreateFixed("http://test.com:80",
                                               TRAFFIC_ANNOTATION_FOR_TESTS));
  set_proxy_resolution_service(proxy_resolution_service.get());
  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n\r\n", kBody.c_str(), nullptr,
                          nullptr);

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.ProxyOverridden", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.ProxyOverridden");
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest, BypassedBytesCurrent) {
  InitializeContext();
  struct TestCase {
    const char* histogram_name;
    const char* retry_response_headers;
  };
  const TestCase test_cases[] = {
      {"DataReductionProxy.BypassedBytes.Current", "HTTP/1.1 200 OK\r\n\r\n"},
      {"DataReductionProxy.BypassedBytes.CurrentAudioVideo",
       "HTTP/1.1 200 OK\r\n"
       "Content-Type: video/mp4\r\n\r\n"},
      {"DataReductionProxy.BypassedBytes.CurrentApplicationOctetStream",
       "HTTP/1.1 200 OK\r\n"
       "Content-Type: application/octet-stream\r\n\r\n"},
  };
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;
    CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                            "HTTP/1.1 502 Bad Gateway\r\n"
                            "Via: 1.1 Chrome-Compression-Proxy\r\n"
                            "Chrome-Proxy: block-once\r\n\r\n",
                            kErrorBody.c_str(),
                            test_case.retry_response_headers, kBody.c_str());

    histogram_tester.ExpectUniqueSample(test_case.histogram_name, kBody.size(),
                                        1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.histogram_name);
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesShortAudioVideo) {
  InitializeContext();
  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 502 Bad Gateway\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n"
                          "Chrome-Proxy: block=1\r\n\r\n",
                          kErrorBody.c_str(),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: video/mp4\r\n\r\n",
                          kBody.c_str());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.ShortAudioVideo", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.ShortAudioVideo");
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesShortAudioVideoCancelled) {
  InitializeContext();
  base::HistogramTester histogram_tester;

  delegate()->set_cancel_in_received_data(true);
  CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 502 Bad Gateway\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n"
                          "Chrome-Proxy: block=1\r\n\r\n",
                          kErrorBody.c_str(),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: video/mp4\r\n\r\n",
                          kBody.c_str());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.ShortAudioVideo", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.ShortAudioVideo");
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest, BypassedBytesExplicitBypass) {
  struct TestCase {
    const char* triggering_histogram_name;
    const char* all_histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
      "DataReductionProxy.BypassedBytes.ShortAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=1\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
      "DataReductionProxy.BypassedBytes.MediumAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=0\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.LongTriggeringRequest",
      "DataReductionProxy.BypassedBytes.LongAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=3600\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;

    CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                            test_case.initial_response_headers,
                            kErrorBody.c_str(), "HTTP/1.1 200 OK\r\n\r\n",
                            kBody.c_str());
    // The first request caused the proxy to be marked as bad, so this second
    // request should not come through the proxy.
    CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                            "HTTP/1.1 200 OK\r\n\r\n", kNextBody.c_str(),
                            nullptr, nullptr);

    histogram_tester.ExpectUniqueSample(test_case.triggering_histogram_name,
                                        kBody.size(), 1);
    histogram_tester.ExpectUniqueSample(test_case.all_histogram_name,
                                        kNextBody.size(), 1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.triggering_histogram_name,
                                            test_case.all_histogram_name);
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesClientSideFallback) {
  struct TestCase {
    const char* histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { "DataReductionProxy.BypassedBytes.Malformed407",
      "HTTP/1.1 407 Proxy Authentication Required\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status500HttpInternalServerError",
      "HTTP/1.1 500 Internal Server Error\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
      "HTTP/1.1 502 Bad Gateway\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status503HttpServiceUnavailable",
      "HTTP/1.1 503 Service Unavailable\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;

    CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                            test_case.initial_response_headers,
                            kErrorBody.c_str(), "HTTP/1.1 200 OK\r\n\r\n",
                            kBody.c_str());

    EXPECT_LT(
        0u, histogram_tester
                .GetAllSamples("DataReductionProxy.ConfigService.HTTPRequests")
                .size());

    // The first request caused the proxy to be marked as bad, so this second
    // request should not come through the proxy.
    CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                            "HTTP/1.1 200 OK\r\n\r\n", kNextBody.c_str(),
                            nullptr, nullptr);

    histogram_tester.ExpectTotalCount(test_case.histogram_name, 2);
    histogram_tester.ExpectBucketCount(test_case.histogram_name, kBody.size(),
                                       1);
    histogram_tester.ExpectBucketCount(test_case.histogram_name,
                                       kNextBody.size(), 1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.histogram_name);

    histogram_tester.ExpectBucketCount(
        "DataReductionProxy.ConfigService.HTTPRequests", 0, 0);
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest, BypassedBytesNetErrorOther) {
  // Make the data reduction proxy host fail to resolve.
  std::unique_ptr<net::MockHostResolver> host_resolver(
      new net::MockHostResolver());

  for (const auto& proxy_server : config()->test_params()->proxies_for_http()) {
    host_resolver->rules()->AddSimulatedFailure(
        proxy_server.proxy_server().host_port_pair().host());
  }

  set_host_resolver(host_resolver.get());
  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n\r\n", kBody.c_str(), nullptr,
                          nullptr);

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.NetworkErrorOther", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.NetworkErrorOther");
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassOnNetworkErrorPrimary",
      -net::ERR_PROXY_CONNECTION_FAILED, 1);
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       ProxyUnreachableThenReachable) {
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("origin.net:80", net::ProxyServer::SCHEME_HTTP);

  InitializeContext();

  // Proxy falls back.
  bypass_stats()->OnProxyFallback(fallback_proxy_server,
                                  net::ERR_PROXY_CONNECTION_FAILED);
  drp_test_context()->RunUntilIdle();
  EXPECT_TRUE(IsUnreachable());

  // Proxy succeeds.
  CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                          kNextBody.c_str(), nullptr, nullptr);
  drp_test_context()->RunUntilIdle();
  EXPECT_FALSE(IsUnreachable());
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       ProxyReachableThenUnreachable) {
  InitializeContext();
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("origin.net:80", net::ProxyServer::SCHEME_HTTP);

  // Proxy succeeds.
  CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                          kNextBody.c_str(), nullptr, nullptr);
  drp_test_context()->RunUntilIdle();
  EXPECT_FALSE(IsUnreachable());

  // Then proxy falls back indefinitely after |kMaxFailedRequestsBeforeReset|
  // failures.
  for (size_t i = 0; i < 4; ++i) {
    bypass_stats()->OnProxyFallback(fallback_proxy_server,
                                    net::ERR_PROXY_CONNECTION_FAILED);
  }

  drp_test_context()->RunUntilIdle();
  EXPECT_TRUE(IsUnreachable());
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       DetectAndRecordMissingViaHeaderResponseCode) {
  const std::string kPrimaryHistogramName =
      "DataReductionProxy.MissingViaHeader.ResponseCode.Primary";
  const std::string kFallbackHistogramName =
      "DataReductionProxy.MissingViaHeader.ResponseCode.Fallback";
  InitializeContext();
  struct TestCase {
    bool is_primary;
    const char* headers;
    int expected_primary_sample;   // -1 indicates no expected sample.
    int expected_fallback_sample;  // -1 indicates no expected sample.
  };
  const TestCase test_cases[] = {
      {true,
       "HTTP/1.1 200 OK\n"
       "Via: 1.1 Chrome-Compression-Proxy\n",
       -1, -1},
      {false,
       "HTTP/1.1 200 OK\n"
       "Via: 1.1 Chrome-Compression-Proxy\n",
       -1, -1},
      {true, "HTTP/1.1 200 OK\n", 200, -1},
      {false, "HTTP/1.1 200 OK\n", -1, 200},
      {true, "HTTP/1.1 304 Not Modified\n", 304, -1},
      {false, "HTTP/1.1 304 Not Modified\n", -1, 304},
      {true, "HTTP/1.1 404 Not Found\n", 404, -1},
      {false, "HTTP/1.1 404 Not Found\n", -1, 404}};

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    base::HistogramTester histogram_tester;
    std::string raw_headers(test_cases[i].headers);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    DataReductionProxyBypassStats::DetectAndRecordMissingViaHeaderResponseCode(
        test_cases[i].is_primary, *headers);

    if (test_cases[i].expected_primary_sample == -1) {
      histogram_tester.ExpectTotalCount(kPrimaryHistogramName, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kPrimaryHistogramName, test_cases[i].expected_primary_sample, 1);
    }

    if (test_cases[i].expected_fallback_sample == -1) {
      histogram_tester.ExpectTotalCount(kFallbackHistogramName, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kFallbackHistogramName, test_cases[i].expected_fallback_sample, 1);
    }
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       SuccessfulPrimaryProxyRequestCompletion) {
  const std::string kHistogramName =
      "DataReductionProxy.SuccessfulRequestCompletionCounts";
  const std::string kMainFrameHistogramName =
      "DataReductionProxy.SuccessfulRequestCompletionCounts.MainFrame";

  InitializeContext();

  const struct {
    int load_flags;
    net::Error net_error;
    bool expect_histogram_sample;
    bool expect_main_frame_histogram_sample;
  } tests[] = {
      {net::LOAD_BYPASS_PROXY | net::LOAD_MAIN_FRAME_DEPRECATED, net::OK, false,
       false},
      {net::LOAD_BYPASS_PROXY | net::LOAD_MAIN_FRAME_DEPRECATED,
       net::ERR_TOO_MANY_REDIRECTS, false, false},
      {net::LOAD_BYPASS_PROXY, net::OK, false, false},
      {net::LOAD_BYPASS_PROXY, net::ERR_TOO_MANY_REDIRECTS, false, false},
      {net::LOAD_MAIN_FRAME_DEPRECATED, net::OK, true, true},
      {net::LOAD_MAIN_FRAME_DEPRECATED, net::ERR_TOO_MANY_REDIRECTS, false,
       false},
      {0, net::OK, true, false},
      {0, net::ERR_TOO_MANY_REDIRECTS, false, false},
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;
    CreateAndExecuteRequest(GURL("http://foo.com"), test.load_flags,
                            test.net_error,
                            "HTTP/1.1 200 OK\r\n"
                            "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                            kNextBody.c_str(), nullptr, nullptr);
    drp_test_context()->RunUntilIdle();

    if (test.expect_histogram_sample)
      histogram_tester.ExpectUniqueSample(kHistogramName, 0, 1);
    else
      histogram_tester.ExpectTotalCount(kHistogramName, 0);

    if (test.expect_main_frame_histogram_sample)
      histogram_tester.ExpectUniqueSample(kMainFrameHistogramName, 0, 1);
    else
      histogram_tester.ExpectTotalCount(kMainFrameHistogramName, 0);
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       SuccessfulFallbackProxyRequestCompletion) {
  const std::string kHistogramName =
      "DataReductionProxy.SuccessfulRequestCompletionCounts";
  const std::string kMainFrameHistogramName =
      "DataReductionProxy.SuccessfulRequestCompletionCounts.MainFrame";

  // Explicitly set primary and fallback Data Reduction Proxies to use.
  config()->test_params()->SetProxiesForHttp(
      std::vector<DataReductionProxyServer>(
          {DataReductionProxyServer(
               net::ProxyServer::FromURI("http://origin.net",
                                         net::ProxyServer::SCHEME_HTTP),
               ProxyServer::CORE),
           DataReductionProxyServer(
               net::ProxyServer::FromURI("http://fallback.net",
                                         net::ProxyServer::SCHEME_HTTP),
               ProxyServer::CORE)}));

  // Make the first Data Reduction Proxy host in the list of Data Reduction
  // Proxies to use fail to resolve, so that the tests below will use the
  // fallback proxy.
  std::unique_ptr<net::MockHostResolver> host_resolver(
      new net::MockHostResolver());
  const DataReductionProxyServer& primary_proxy =
      config()->test_params()->proxies_for_http().front();
  host_resolver->rules()->AddSimulatedFailure(
      primary_proxy.proxy_server().host_port_pair().host());

  set_host_resolver(host_resolver.get());
  InitializeContext();

  const struct {
    int load_flags;
    net::Error net_error;
    bool expect_histogram_sample;
    bool expect_main_frame_histogram_sample;
  } tests[] = {
      {net::LOAD_BYPASS_PROXY | net::LOAD_MAIN_FRAME_DEPRECATED, net::OK, false,
       false},
      {net::LOAD_BYPASS_PROXY | net::LOAD_MAIN_FRAME_DEPRECATED,
       net::ERR_TOO_MANY_REDIRECTS, false, false},
      {net::LOAD_BYPASS_PROXY, net::OK, false, false},
      {net::LOAD_BYPASS_PROXY, net::ERR_TOO_MANY_REDIRECTS, false, false},
      {net::LOAD_MAIN_FRAME_DEPRECATED, net::OK, true, true},
      {net::LOAD_MAIN_FRAME_DEPRECATED, net::ERR_TOO_MANY_REDIRECTS, false,
       false},
      {0, net::OK, true, false},
      {0, net::ERR_TOO_MANY_REDIRECTS, false, false},
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;
    CreateAndExecuteRequest(GURL("http://foo.com"), test.load_flags,
                            test.net_error,
                            "HTTP/1.1 200 OK\r\n"
                            "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                            kNextBody.c_str(), nullptr, nullptr);
    drp_test_context()->RunUntilIdle();

    if (test.expect_histogram_sample)
      histogram_tester.ExpectUniqueSample(kHistogramName, 1, 1);
    else
      histogram_tester.ExpectTotalCount(kHistogramName, 0);

    if (test.expect_main_frame_histogram_sample)
      histogram_tester.ExpectUniqueSample(kMainFrameHistogramName, 1, 1);
    else
      histogram_tester.ExpectTotalCount(kMainFrameHistogramName, 0);
  }
}

// Verifies that the scheme of the HTTP data reduction proxy used is recorded
// correctly.
TEST_F(DataReductionProxyBypassStatsEndToEndTest, HttpProxyScheme) {
  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                          kNextBody.c_str(), nullptr, nullptr);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.ProxySchemeUsed",
                                      1 /*PROXY_SCHEME_HTTP */, 1);
}

}  // namespace data_reduction_proxy
