// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/socket/socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::MockRead;

namespace data_reduction_proxy {

namespace {

class CountingURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  CountingURLRequestInterceptor()
      : request_count_(0), redirect_count_(0), response_count_(0) {
  }

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    request_count_++;
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override {
    redirect_count_++;
    return nullptr;
  }

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    response_count_++;
    return nullptr;
  }

  int request_count() const {
    return request_count_;
  }

  int redirect_count() const {
    return redirect_count_;
  }

  int response_count() const {
    return response_count_;
  }

 private:
  mutable int request_count_;
  mutable int redirect_count_;
  mutable int response_count_;
};

class TestURLRequestContextWithDataReductionProxy
    : public net::TestURLRequestContext {
 public:
  TestURLRequestContextWithDataReductionProxy(const net::ProxyServer& origin,
                                              net::NetworkDelegate* delegate)
      : net::TestURLRequestContext(true) {
    context_storage_.set_proxy_resolution_service(
        net::ProxyResolutionService::CreateFixed(origin.ToURI(),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS));
    set_network_delegate(delegate);
  }

  ~TestURLRequestContextWithDataReductionProxy() override {}
};

class DataReductionProxyInterceptorTest : public testing::Test {
 public:
  DataReductionProxyInterceptorTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .Build();
    default_context_.reset(new TestURLRequestContextWithDataReductionProxy(
        test_context_->config()
            ->test_params()
            ->proxies_for_http()
            .front()
            .proxy_server(),
        &default_network_delegate_));
    default_context_->set_network_delegate(&default_network_delegate_);
    test_context_->config()->test_params()->UseNonSecureProxiesForHttp();
  }

  ~DataReductionProxyInterceptorTest() override {
    // URLRequestJobs may post clean-up tasks on destruction.
    base::RunLoop().RunUntilIdle();
  }

  void Init(std::unique_ptr<net::URLRequestJobFactory> factory) {
    job_factory_ = std::move(factory);
    default_context_->set_job_factory(job_factory_.get());
    default_context_->Init();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  net::TestNetworkDelegate default_network_delegate_;
  std::unique_ptr<net::URLRequestJobFactory> job_factory_;
  std::unique_ptr<net::TestURLRequestContext> default_context_;
};

// Disabled on Mac due to flakiness. See crbug.com/601562.
#if defined(OS_MACOSX)
#define MAYBE_TestJobFactoryChaining DISABLED_TestJobFactoryChaining
#else
#define MAYBE_TestJobFactoryChaining TestJobFactoryChaining
#endif
TEST_F(DataReductionProxyInterceptorTest, MAYBE_TestJobFactoryChaining) {
  // Verifies that job factories can be chained.
  std::unique_ptr<net::URLRequestJobFactory> impl(
      new net::URLRequestJobFactoryImpl());

  CountingURLRequestInterceptor* interceptor2 =
      new CountingURLRequestInterceptor();
  std::unique_ptr<net::URLRequestJobFactory> factory2(
      new net::URLRequestInterceptingJobFactory(
          std::move(impl), base::WrapUnique(interceptor2)));

  CountingURLRequestInterceptor* interceptor1 =
      new CountingURLRequestInterceptor();
  std::unique_ptr<net::URLRequestJobFactory> factory1(
      new net::URLRequestInterceptingJobFactory(
          std::move(factory2), base::WrapUnique(interceptor1)));

  Init(std::move(factory1));

  net::TestDelegate d;
  std::unique_ptr<net::URLRequest> req(default_context_->CreateRequest(
      GURL("http://foo.test"), net::DEFAULT_PRIORITY, &d,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  req->Start();
  base::RunLoop().Run();
  EXPECT_EQ(1, interceptor1->request_count());
  EXPECT_EQ(0, interceptor1->redirect_count());
  EXPECT_EQ(1, interceptor1->response_count());
  EXPECT_EQ(1, interceptor2->request_count());
  EXPECT_EQ(0, interceptor2->redirect_count());
  EXPECT_EQ(1, interceptor2->response_count());
}

class DataReductionProxyInterceptorWithServerTest : public testing::Test {
 public:
  DataReductionProxyInterceptorWithServerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        context_(true) {
    context_.set_network_delegate(&network_delegate_);
  }

  ~DataReductionProxyInterceptorWithServerTest() override {
    test_context_->io_data()->ShutdownOnUIThread();
    // URLRequestJobs may post clean-up tasks on destruction.
    test_context_->RunUntilIdle();
  }

  void SetUp() override {
    base::FilePath root_path, proxy_file_path, direct_file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path);
    proxy_file_path = root_path.AppendASCII(
        "components/test/data/data_reduction_proxy/proxy");
    direct_file_path = root_path.AppendASCII(
        "components/test/data/data_reduction_proxy/direct");
    proxy_.ServeFilesFromDirectory(proxy_file_path);
    direct_.ServeFilesFromDirectory(direct_file_path);
    ASSERT_TRUE(proxy_.Start());
    ASSERT_TRUE(direct_.Start());

    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithURLRequestContext(&context_)
                        .Build();
    std::string spec;
    base::TrimString(proxy_.GetURL("/").spec(), "/", &spec);
    net::ProxyServer origin =
        net::ProxyServer::FromURI(spec, net::ProxyServer::SCHEME_HTTP);
    std::vector<DataReductionProxyServer> proxies_for_http;
    proxies_for_http.push_back(
        DataReductionProxyServer(origin, ProxyServer::UNSPECIFIED_TYPE));
    test_context_->config()->test_params()->SetProxiesForHttp(proxies_for_http);
    std::string proxy_name = origin.ToURI();
    proxy_resolution_service_ =
        net::ProxyResolutionService::CreateFixedFromPacResult(
            "PROXY " + proxy_name + "; DIRECT", TRAFFIC_ANNOTATION_FOR_TESTS);

    context_.set_proxy_resolution_service(proxy_resolution_service_.get());

    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory_impl(
        new net::URLRequestJobFactoryImpl());
    job_factory_.reset(new net::URLRequestInterceptingJobFactory(
        std::move(job_factory_impl),
        test_context_->io_data()->CreateInterceptor()));
    context_.set_job_factory(job_factory_.get());
    context_.Init();
  }

  const net::TestURLRequestContext& context() {
    return context_;
  }

  const net::EmbeddedTestServer& direct() { return direct_; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  net::TestNetworkDelegate network_delegate_;
  net::TestURLRequestContext context_;
  net::EmbeddedTestServer proxy_;
  net::EmbeddedTestServer direct_;
  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<net::URLRequestJobFactory> job_factory_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyInterceptorWithServerTest, TestBypass) {
  // Tests the mechanics of proxy bypass work with a "real" server. For tests
  // that cover every imaginable response that could trigger a bypass, see:
  // DataReductionProxyProtocolTest.
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request(context().CreateRequest(
      direct().GetURL("/block10.html"), net::DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  EXPECT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_EQ(net::OK, delegate.request_status());
  EXPECT_EQ("hello", delegate.data_received());
}

TEST_F(DataReductionProxyInterceptorWithServerTest, TestNoBypass) {
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request(context().CreateRequest(
      direct().GetURL("/noblock.html"), net::DEFAULT_PRIORITY, &delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  EXPECT_TRUE(request->is_pending());
  base::RunLoop().Run();

  EXPECT_EQ(net::OK, delegate.request_status());
  EXPECT_EQ("hello", delegate.data_received());
}

class DataReductionProxyInterceptorEndToEndTest : public testing::Test {
 public:
  DataReductionProxyInterceptorEndToEndTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        context_(true),
        context_storage_(&context_) {}

  ~DataReductionProxyInterceptorEndToEndTest() override {}

  void SetUp() override {
    drp_test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithURLRequestContext(&context_)
            .WithMockClientSocketFactory(&mock_socket_factory_)
            .Build();
    drp_test_context_->config()->test_params()->UseNonSecureProxiesForHttp();
    drp_test_context_->AttachToURLRequestContext(&context_storage_);
    context_.set_client_socket_factory(&mock_socket_factory_);
    proxy_delegate_ = drp_test_context_->io_data()->CreateProxyDelegate();
    context_.Init();
    context_.proxy_resolution_service()->SetProxyDelegate(
        proxy_delegate_.get());
    drp_test_context_->DisableWarmupURLFetch();
    drp_test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();

    // Three proxies should be available for use: primary, fallback, and direct.
    const net::ProxyConfig& proxy_config =
        drp_test_context_->configurator()->GetProxyConfig();
    EXPECT_EQ(3U, proxy_config.proxy_rules().proxies_for_http.size());
  }

  // Creates a URLRequest using the test's TestURLRequestContext and executes
  // it. Returns the created URLRequest.
  std::unique_ptr<net::URLRequest> CreateAndExecuteRequest(const GURL& url) {
    std::unique_ptr<net::URLRequest> request(context_.CreateRequest(
        url, net::IDLE, &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS));
    request->Start();
    drp_test_context_->RunUntilIdle();
    return request;
  }

  const net::TestDelegate& delegate() const { return delegate_; }

  net::MockClientSocketFactory* mock_socket_factory() {
    return &mock_socket_factory_;
  }

  TestDataReductionProxyConfig* config() const {
    return drp_test_context_->config();
  }

  net::ProxyServer origin() const {
    return config()->test_params()->proxies_for_http().front().proxy_server();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  net::TestDelegate delegate_;
  net::MockClientSocketFactory mock_socket_factory_;
  net::TestURLRequestContext context_;
  net::URLRequestContextStorage context_storage_;
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context_;
  std::unique_ptr<net::ProxyDelegate> proxy_delegate_;
};

const std::string kBody = "response body";

TEST_F(DataReductionProxyInterceptorEndToEndTest, ResponseWithoutRetry) {
  // The response comes through the proxy and should not be retried.
  MockRead mock_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n"),
      MockRead(kBody.c_str()),
      MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      mock_reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&socket_data_provider);

  std::unique_ptr<net::URLRequest> request =
      CreateAndExecuteRequest(GURL("http://foo.com"));

  EXPECT_EQ(net::OK, delegate().request_status());
  EXPECT_EQ(200, request->GetResponseCode());
  EXPECT_EQ(kBody, delegate().data_received());
  EXPECT_EQ(origin(), request->proxy_server());
}

TEST_F(DataReductionProxyInterceptorEndToEndTest, RedirectWithoutRetry) {
  // The redirect comes through the proxy and should not be retried.
  MockRead redirect_mock_reads[] = {
      MockRead("HTTP/1.1 302 Found\r\n"
               "Via: 1.1 Chrome-Compression-Proxy\r\n"
               "Location: http://bar.com/\r\n\r\n"),
      MockRead(""),
      MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider redirect_socket_data_provider(
      redirect_mock_reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&redirect_socket_data_provider);

  // The response after the redirect comes through proxy and should not be
  // retried.
  MockRead response_mock_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n"),
      MockRead(kBody.c_str()),
      MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider response_socket_data_provider(
      response_mock_reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&response_socket_data_provider);

  std::unique_ptr<net::URLRequest> request =
      CreateAndExecuteRequest(GURL("http://foo.com"));

  EXPECT_EQ(net::OK, delegate().request_status());
  EXPECT_EQ(200, request->GetResponseCode());
  EXPECT_EQ(kBody, delegate().data_received());
  EXPECT_EQ(origin(), request->proxy_server());
  // The redirect should have been processed and followed normally.
  EXPECT_EQ(1, delegate().received_redirect_count());
}

// Test that data reduction proxy is byppassed if there is a URL redirect cycle.
TEST_F(DataReductionProxyInterceptorEndToEndTest, URLRedirectCycle) {
  base::HistogramTester histogram_tester;
  MockRead redirect_mock_reads_1[] = {
      MockRead("HTTP/1.1 302 Found\r\n"
               "Via: 1.1 Chrome-Compression-Proxy\r\n"
               "Location: http://bar.com/\r\n\r\n"),
      MockRead(""), MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider redirect_socket_data_provider_1(
      redirect_mock_reads_1, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(
      &redirect_socket_data_provider_1);

  MockRead redirect_mock_reads_2[] = {
      MockRead("HTTP/1.1 302 Found\r\n"
               "Via: 1.1 Chrome-Compression-Proxy\r\n"
               "Location: http://foo.com/\r\n\r\n"),
      MockRead(""), MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider redirect_socket_data_provider_2(
      redirect_mock_reads_2, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(
      &redirect_socket_data_provider_2);

  // Redirect cycle.
  MockRead redirect_mock_reads_3[] = {
      MockRead("HTTP/1.1 302 Found\r\n"
               "Via: 1.1 Chrome-Compression-Proxy\r\n"
               "Location: http://bar.com/\r\n\r\n"),
      MockRead(""), MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider redirect_socket_data_provider_3(
      redirect_mock_reads_3, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(
      &redirect_socket_data_provider_3);

  // Data reduction proxy should be bypassed.
  MockRead redirect_mock_reads_4[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead(kBody.c_str()),
      MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider redirect_socket_data_provider_4(
      redirect_mock_reads_4, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(
      &redirect_socket_data_provider_4);

  std::unique_ptr<net::URLRequest> request =
      CreateAndExecuteRequest(GURL("http://foo.com"));

  EXPECT_EQ(net::OK, delegate().request_status());
  EXPECT_EQ(200, request->GetResponseCode());
  EXPECT_EQ(kBody, delegate().data_received());
  EXPECT_TRUE(request->proxy_server().is_direct());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.BypassedBytes.URLRedirectCycle", 1);
}

TEST_F(DataReductionProxyInterceptorEndToEndTest, ResponseWithBypassAndRetry) {
  // The first try gives a bypass.
  MockRead initial_mock_reads[] = {
      MockRead("HTTP/1.1 502 Bad Gateway\r\n"
               "Via: 1.1 Chrome-Compression-Proxy\r\n"
               "Chrome-Proxy: block-once\r\n\r\n"),
      MockRead(""),
      MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider initial_socket_data_provider(
      initial_mock_reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&initial_socket_data_provider);

  // The retry after the bypass is successful.
  MockRead retry_mock_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      MockRead(kBody.c_str()),
      MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider retry_socket_data_provider(
      retry_mock_reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&retry_socket_data_provider);

  std::unique_ptr<net::URLRequest> request =
      CreateAndExecuteRequest(GURL("http://foo.com"));

  EXPECT_EQ(net::OK, delegate().request_status());
  EXPECT_EQ(200, request->GetResponseCode());
  EXPECT_EQ(kBody, delegate().data_received());
  EXPECT_TRUE(request->proxy_server().is_direct());
  // The bypassed response should have been intercepted before the response was
  // processed, so only the final response after the retry should have been
  // processed.
  EXPECT_EQ(1, delegate().response_started_count());
}

TEST_F(DataReductionProxyInterceptorEndToEndTest, RedirectWithBypassAndRetry) {
  MockRead mock_reads_array[][3] = {
      // First, get a bypass which should be retried
      // using the fallback proxy.
      {
          MockRead("HTTP/1.1 302 Found\r\n"
                   "Chrome-Proxy: bypass=0\r\n"
                   "Location: http://bar.com/\r\n\r\n"),
          MockRead(""), MockRead(net::SYNCHRONOUS, net::OK),
      },
      // Same as before, but through the fallback proxy. Now both proxies are
      // bypassed, and the request should be retried over direct.
      {
          MockRead("HTTP/1.1 302 Found\r\n"
                   "Chrome-Proxy: bypass=0\r\n"
                   "Location: http://baz.com/\r\n\r\n"),
          MockRead(""), MockRead(net::SYNCHRONOUS, net::OK),
      },
      // Finally, a successful response is received.
      {
          MockRead("HTTP/1.1 200 OK\r\n\r\n"), MockRead(kBody.c_str()),
          MockRead(net::SYNCHRONOUS, net::OK),
      },
  };
  std::vector<std::unique_ptr<net::SocketDataProvider>> socket_data_providers;
  for (MockRead* mock_reads : mock_reads_array) {
    socket_data_providers.push_back(
        std::make_unique<net::StaticSocketDataProvider>(
            base::make_span(mock_reads, 3), base::span<net::MockWrite>()));
    mock_socket_factory()->AddSocketDataProvider(
        socket_data_providers.back().get());
  }

  std::unique_ptr<net::URLRequest> request =
      CreateAndExecuteRequest(GURL("http://foo.com"));

  EXPECT_EQ(net::OK, delegate().request_status());
  EXPECT_EQ(200, request->GetResponseCode());
  EXPECT_EQ(kBody, delegate().data_received());
  EXPECT_TRUE(request->proxy_server().is_direct());

  // Each of the redirects should have been intercepted before being followed.
  EXPECT_EQ(0, delegate().received_redirect_count());
  EXPECT_EQ(std::vector<GURL>(1, GURL("http://foo.com")), request->url_chain());
}

TEST_F(DataReductionProxyInterceptorEndToEndTest, RedirectChainToHttps) {
  // First, a redirect is successfully received through the Data Reduction
  // Proxy. HSTS is forced for play.google.com and prebaked into Chrome, so
  // http://play.google.com will automatically be redirected to
  // https://play.google.com. See net/http/transport_security_state_static.json.
  MockRead first_redirect_reads[] = {
      MockRead(
          "HTTP/1.1 302 Found\r\n"
          "Location: http://play.google.com\r\n"
          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n"),
      MockRead(""),
      MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider first_redirect_socket(
      first_redirect_reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&first_redirect_socket);

  // Receive the response for https://play.google.com.
  MockRead https_response_reads[] = {
      MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      MockRead(kBody.c_str()),
      MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider https_response_socket(
      https_response_reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&https_response_socket);
  net::SSLSocketDataProvider https_response_ssl_socket(net::SYNCHRONOUS,
                                                       net::OK);
  mock_socket_factory()->AddSSLSocketDataProvider(&https_response_ssl_socket);

  std::unique_ptr<net::URLRequest> request =
      CreateAndExecuteRequest(GURL("http://music.google.com"));
  request->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                        net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  EXPECT_FALSE(delegate().request_failed());
  EXPECT_EQ(kBody, delegate().data_received());

  std::vector<GURL> expected_url_chain;
  expected_url_chain.push_back(GURL("http://music.google.com"));
  expected_url_chain.push_back(GURL("http://play.google.com"));
  expected_url_chain.push_back(GURL("https://play.google.com"));
  EXPECT_EQ(expected_url_chain, request->url_chain());
}

}  // namespace

}  // namespace data_reduction_proxy
