// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace data_reduction_proxy {

namespace {

// Constructs and returns a proxy with the specified scheme.
net::ProxyServer GetProxyWithScheme(net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_HTTP:
      return net::ProxyServer::FromURI("origin.net:443",
                                       net::ProxyServer::SCHEME_HTTP);
    case net::ProxyServer::SCHEME_HTTPS:
      return net::ProxyServer::FromURI("https://origin.net:443",
                                       net::ProxyServer::SCHEME_HTTP);
    case net::ProxyServer::SCHEME_QUIC:
      return net::ProxyServer::FromURI("quic://origin.net:443",
                                       net::ProxyServer::SCHEME_QUIC);
    case net::ProxyServer::SCHEME_DIRECT:
      return net::ProxyServer::Direct();
    default:
      NOTREACHED();
      return net::ProxyServer::FromURI("", net::ProxyServer::SCHEME_INVALID);
  }
}

class TestDataReductionProxyDelegate : public DataReductionProxyDelegate {
 public:
  TestDataReductionProxyDelegate(
      DataReductionProxyConfig* config,
      const DataReductionProxyConfigurator* configurator,
      DataReductionProxyBypassStats* bypass_stats,
      bool proxy_supports_quic)
      : DataReductionProxyDelegate(config, configurator, bypass_stats),
        proxy_supports_quic_(proxy_supports_quic) {}

  ~TestDataReductionProxyDelegate() override {}

  bool SupportsQUIC(const net::ProxyServer& proxy_server) const override {
    return proxy_supports_quic_;
  }

  // Verifies if the histograms related to use of QUIC proxy are recorded
  // correctly.
  void VerifyQuicHistogramCounts(const base::HistogramTester& histogram_tester,
                                 bool expect_alternative_proxy_server,
                                 bool supports_quic,
                                 bool broken) const {
    if (expect_alternative_proxy_server && !broken) {
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.Quic.ProxyStatus",
          TestDataReductionProxyDelegate::QuicProxyStatus::
              QUIC_PROXY_STATUS_AVAILABLE,
          1);
    } else if (!supports_quic && !broken) {
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.Quic.ProxyStatus",
          TestDataReductionProxyDelegate::QuicProxyStatus::
              QUIC_PROXY_NOT_SUPPORTED,
          1);
    } else {
      ASSERT_TRUE(broken);
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.Quic.ProxyStatus",
          TestDataReductionProxyDelegate::QuicProxyStatus::
              QUIC_PROXY_STATUS_MARKED_AS_BROKEN,
          1);
    }
  }

  using DataReductionProxyDelegate::QuicProxyStatus;

 private:
  const bool proxy_supports_quic_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyDelegate);
};

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
#else
const Client kClient = Client::UNKNOWN;
#endif

class TestLoFiUIService : public LoFiUIService {
 public:
  TestLoFiUIService() {}
  ~TestLoFiUIService() override {}

  void OnLoFiReponseReceived(const net::URLRequest& request) override {}
};

class DataReductionProxyDelegateTest : public testing::Test {
 public:
  DataReductionProxyDelegateTest()
      : context_(true),
        context_storage_(&context_),
        test_context_(DataReductionProxyTestContext::Builder()
                          .WithClient(kClient)
                          .WithMockClientSocketFactory(&mock_socket_factory_)
                          .WithURLRequestContext(&context_)
                          .Build()) {
    context_.set_client_socket_factory(&mock_socket_factory_);
    test_context_->AttachToURLRequestContext(&context_storage_);

    std::unique_ptr<TestLoFiUIService> lofi_ui_service(new TestLoFiUIService());
    lofi_ui_service_ = lofi_ui_service.get();
    test_context_->io_data()->set_lofi_ui_service(std::move(lofi_ui_service));

    proxy_delegate_ = test_context_->io_data()->CreateProxyDelegate();
    context_.Init();
    context_.proxy_resolution_service()->SetProxyDelegate(
        proxy_delegate_.get());

    proxy_delegate_->InitializeOnIOThread(test_context_->io_data());

    test_context_->DisableWarmupURLFetch();
    test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();
  }

  // Each line in |response_headers| should end with "\r\n" and not '\0', and
  // the last line should have a second "\r\n".
  // An empty |response_headers| is allowed. It works by making this look like
  // an HTTP/0.9 response, since HTTP/0.9 responses don't have headers.
  std::unique_ptr<net::URLRequest> FetchURLRequest(
      const GURL& url,
      net::HttpRequestHeaders* request_headers,
      const std::string& response_headers,
      int64_t response_content_length) {
    const std::string response_body(
        base::checked_cast<size_t>(response_content_length), ' ');
    net::MockRead reads[] = {net::MockRead(response_headers.c_str()),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::SYNCHRONOUS, net::OK)};
    net::StaticSocketDataProvider socket(reads, base::span<net::MockWrite>());
    mock_socket_factory_.AddSocketDataProvider(&socket);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
        url, net::IDLE, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    if (request_headers)
      request->SetExtraRequestHeaders(*request_headers);

    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

  int64_t total_received_bytes() const {
    test_context_->RunUntilIdle();
    return test_context_->pref_service()->GetInt64(
        prefs::kHttpReceivedContentLength);
  }

  int64_t total_original_received_bytes() const {
    test_context_->RunUntilIdle();
    return test_context_->pref_service()->GetInt64(
        prefs::kHttpOriginalContentLength);
  }

  net::MockClientSocketFactory* mock_socket_factory() {
    return &mock_socket_factory_;
  }

  net::TestURLRequestContext* context() { return &context_; }

  TestDataReductionProxyParams* params() const {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyConfig* config() const {
    return test_context_->config();
  }

  TestDataReductionProxyIOData* io_data() const {
    return test_context_->io_data();
  }

  DataReductionProxyDelegate* proxy_delegate() const {
    return proxy_delegate_.get();
  }

 private:
  base::MessageLoopForIO message_loop_;
  net::MockClientSocketFactory mock_socket_factory_;
  net::TestURLRequestContext context_;
  net::URLRequestContextStorage context_storage_;

  TestLoFiUIService* lofi_ui_service_;

  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxyDelegate> proxy_delegate_;
};

TEST_F(DataReductionProxyDelegateTest, OnResolveProxy) {
  GURL url("http://www.google.com/");
  params()->UseNonSecureProxiesForHttp();

  // Other proxy info
  net::ProxyInfo other_proxy_info;
  other_proxy_info.UseNamedProxy("proxy.com");
  EXPECT_FALSE(other_proxy_info.is_empty());

  // Direct
  net::ProxyInfo direct_proxy_info;
  direct_proxy_info.UseDirect();
  EXPECT_TRUE(direct_proxy_info.is_direct());

  // Empty retry info map
  net::ProxyRetryInfoMap empty_proxy_retry_info;

  // Retry info map with the data reduction proxy;
  net::ProxyRetryInfoMap data_reduction_proxy_retry_info;
  net::ProxyRetryInfo retry_info;
  retry_info.current_delay = base::TimeDelta::FromSeconds(1000);
  retry_info.bad_until = base::TimeTicks().Now() + retry_info.current_delay;
  retry_info.try_while_bad = false;
  data_reduction_proxy_retry_info
      [params()->proxies_for_http().front().proxy_server().ToURI()] =
          retry_info;

  net::ProxyInfo result;
  // Another proxy is used. It should be used afterwards.
  result.Use(other_proxy_info);
  proxy_delegate()->OnResolveProxy(url, "GET", empty_proxy_retry_info, &result);
  EXPECT_EQ(other_proxy_info.proxy_server(), result.proxy_server());

  // A direct connection is used. The data reduction proxy should be used
  // afterwards.
  // Another proxy is used. It should be used afterwards.
  result.Use(direct_proxy_info);
  proxy_delegate()->OnResolveProxy(url, "GET", empty_proxy_retry_info, &result);
  EXPECT_EQ(params()->proxies_for_http().front().proxy_server(),
            result.proxy_server());

  // A direct connection is used, but the data reduction proxy is on the retry
  // list. A direct connection should be used afterwards.
  result.Use(direct_proxy_info);
  proxy_delegate()->OnResolveProxy(GURL("ws://echo.websocket.org/"), "GET",
                                   data_reduction_proxy_retry_info, &result);
  EXPECT_TRUE(result.proxy_server().is_direct());

  // Test that ws:// and wss:// URLs bypass the data reduction proxy.
  result.UseDirect();
  proxy_delegate()->OnResolveProxy(GURL("wss://echo.websocket.org/"), "GET",
                                   empty_proxy_retry_info, &result);
  EXPECT_TRUE(result.is_direct());

  result.UseDirect();
  proxy_delegate()->OnResolveProxy(GURL("wss://echo.websocket.org/"), "GET",
                                   empty_proxy_retry_info, &result);
  EXPECT_TRUE(result.is_direct());

  // POST methods go direct.
  result.UseDirect();
  proxy_delegate()->OnResolveProxy(url, "POST", empty_proxy_retry_info,
                                   &result);
  EXPECT_TRUE(result.is_direct());
}

TEST_F(DataReductionProxyDelegateTest, OnResolveProxyWarmupURL) {
  const struct {
    bool is_secure_proxy;
    bool is_core_proxy;
    bool use_warmup_url;
  } tests[] = {
      {false, false, false}, {false, true, false}, {true, false, false},
      {true, true, false},   {false, false, true}, {false, true, true},
      {true, false, true},   {true, true, true},
  };

  for (const auto& test : tests) {
    config()->SetInFlightWarmupProxyDetails(
        std::make_pair(test.is_secure_proxy, test.is_core_proxy));
    GURL url;
    if (test.use_warmup_url) {
      url = params::GetWarmupURL();
    } else {
      url = GURL("http://www.google.com");
    }
    params()->UseNonSecureProxiesForHttp();

    // A regular HTTP URL (i.e., a non-warmup URL) should always be fetched
    // using the data reduction proxies configured by this test framework.
    bool expect_data_reduction_proxy_used = true;

    if (test.use_warmup_url) {
      // This test framework only sets insecure, core proxies in the data
      // reduction proxy configuration. When the in-flight warmup proxy details
      // are set to a proxy that is either secure or non-core, then all
      // configured data saver proxies should be removed when doing proxy
      // resolution for the warmup URL. Hence, the warmup URL will be fetched
      // directly in all cases except when the in-flight warmup proxy details
      // match the properties of the data saver proxies configured by this test.
      expect_data_reduction_proxy_used =
          !test.is_secure_proxy && test.is_core_proxy;
    }

    // Other proxy info
    net::ProxyInfo other_proxy_info;
    other_proxy_info.UseNamedProxy("proxy.com");
    EXPECT_FALSE(other_proxy_info.is_empty());

    // Direct
    net::ProxyInfo direct_proxy_info;
    direct_proxy_info.UseDirect();
    EXPECT_TRUE(direct_proxy_info.is_direct());

    // Empty retry info map
    net::ProxyRetryInfoMap empty_proxy_retry_info;

    net::ProxyInfo result;
    // Another proxy is used. It should be used afterwards.
    result.Use(other_proxy_info);
    proxy_delegate()->OnResolveProxy(url, "GET", empty_proxy_retry_info,
                                     &result);
    EXPECT_EQ(other_proxy_info.proxy_server(), result.proxy_server());

    // A direct connection is used. The data reduction proxy should be used
    // afterwards.
    result.Use(direct_proxy_info);
    proxy_delegate()->OnResolveProxy(url, "GET", empty_proxy_retry_info,
                                     &result);
    //
    if (expect_data_reduction_proxy_used) {
      EXPECT_EQ(params()->proxies_for_http().front().proxy_server(),
                result.proxy_server());
    } else {
      EXPECT_TRUE(result.proxy_server().is_direct());
    }
  }
}

// Verifies that DataReductionProxyDelegate correctly implements
// alternative proxy functionality.
TEST_F(DataReductionProxyDelegateTest, AlternativeProxy) {
  const struct {
    bool is_in_quic_field_trial;
    bool proxy_supports_quic;
    net::ProxyServer::Scheme first_proxy_scheme;
    net::ProxyServer::Scheme second_proxy_scheme;
  } tests[] = {{false, true, net::ProxyServer::SCHEME_HTTPS,
                net::ProxyServer::SCHEME_HTTP},
               {true, true, net::ProxyServer::SCHEME_HTTPS,
                net::ProxyServer::SCHEME_HTTP},
               {true, true, net::ProxyServer::SCHEME_HTTP,
                net::ProxyServer::SCHEME_HTTPS},
               {true, true, net::ProxyServer::SCHEME_QUIC,
                net::ProxyServer::SCHEME_HTTP},
               {true, true, net::ProxyServer::SCHEME_QUIC,
                net::ProxyServer::SCHEME_HTTPS},
               {true, false, net::ProxyServer::SCHEME_HTTPS,
                net::ProxyServer::SCHEME_HTTP},
               {true, false, net::ProxyServer::SCHEME_HTTP,
                net::ProxyServer::SCHEME_HTTPS}};
  GURL url("http://www.example.com");

  for (const auto test : tests) {
    // True if there should exist a valid alternative proxy server corresponding
    // to the first proxy in the list of proxies available to the data reduction
    // proxy.
    const bool expect_alternative_proxy_server_to_first_proxy =
        test.is_in_quic_field_trial && test.proxy_supports_quic &&
        test.first_proxy_scheme == net::ProxyServer::SCHEME_HTTPS;

    // True if there should exist a valid alternative proxy server corresponding
    // to the second proxy in the list of proxies available to the data
    // reduction proxy.
    const bool expect_alternative_proxy_server_to_second_proxy =
        test.is_in_quic_field_trial && test.proxy_supports_quic &&
        test.second_proxy_scheme == net::ProxyServer::SCHEME_HTTPS;

    std::vector<DataReductionProxyServer> proxies_for_http;

    net::ProxyServer first_proxy = GetProxyWithScheme(test.first_proxy_scheme);
    proxies_for_http.push_back(
        DataReductionProxyServer(first_proxy, ProxyServer::CORE));

    net::ProxyServer second_proxy =
        GetProxyWithScheme(test.second_proxy_scheme);
    proxies_for_http.push_back(
        DataReductionProxyServer(second_proxy, ProxyServer::UNSPECIFIED_TYPE));

    params()->SetProxiesForHttpForTesting(proxies_for_http);

    TestDataReductionProxyDelegate delegate(config(), io_data()->configurator(),
                                            io_data()->bypass_stats(),
                                            test.proxy_supports_quic);

    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(
        params::GetQuicFieldTrialName(),
        test.is_in_quic_field_trial ? "Enabled" : "Control");

    net::ProxyInfo proxy_info;
    net::ProxyRetryInfoMap empty_proxy_retry_info;
    net::ProxyServer alternative_proxy_server_to_first_proxy;
    net::ProxyServer alternative_proxy_server_to_second_proxy;

    {
      proxy_info.UseDirect();

      // Test if the alternative proxy is correctly set if the resolved proxy is
      // |first_proxy|.
      base::HistogramTester histogram_tester;
      delegate.OnResolveProxy(url, "GET", empty_proxy_retry_info, &proxy_info);
      ASSERT_EQ(first_proxy, proxy_info.proxy_server());
      alternative_proxy_server_to_first_proxy = proxy_info.alternative_proxy();
      EXPECT_EQ(expect_alternative_proxy_server_to_first_proxy,
                alternative_proxy_server_to_first_proxy.is_valid());

      // Verify that the metrics are recorded correctly.
      if (test.is_in_quic_field_trial &&
          test.first_proxy_scheme == net::ProxyServer::SCHEME_HTTPS) {
        delegate.VerifyQuicHistogramCounts(
            histogram_tester, expect_alternative_proxy_server_to_first_proxy,
            test.proxy_supports_quic, false);
      } else {
        if (!test.is_in_quic_field_trial) {
          histogram_tester.ExpectUniqueSample(
              "DataReductionProxy.Quic.ProxyStatus",
              3 /* QUIC_PROXY_DISABLED_VIA_FIELD_TRIAL */, 1);
        } else {
          histogram_tester.ExpectTotalCount(
              "DataReductionProxy.Quic.ProxyStatus", 0);
        }
      }
    }

    {
      // Test if the alternative proxy is correctly set if the resolved proxy is
      // |second_proxy|. Mark the first proxy as failed so that the second is
      // selected.
      proxy_info.UseDirect();
      net::ProxyRetryInfoMap proxy_retry_info;
      net::ProxyRetryInfo bad_proxy_info;
      bad_proxy_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();
      proxy_retry_info[first_proxy.ToURI()] = bad_proxy_info;

      base::HistogramTester histogram_tester;
      delegate.OnResolveProxy(url, "GET", proxy_retry_info, &proxy_info);
      EXPECT_EQ(second_proxy, proxy_info.proxy_server());
      alternative_proxy_server_to_second_proxy = proxy_info.alternative_proxy();
      EXPECT_EQ(expect_alternative_proxy_server_to_first_proxy,
                alternative_proxy_server_to_first_proxy.is_valid());
      EXPECT_EQ(expect_alternative_proxy_server_to_second_proxy,
                alternative_proxy_server_to_second_proxy.is_valid());

      // Verify that the metrics are recorded correctly.
      if (test.is_in_quic_field_trial &&
          test.second_proxy_scheme == net::ProxyServer::SCHEME_HTTPS) {
        delegate.VerifyQuicHistogramCounts(
            histogram_tester, expect_alternative_proxy_server_to_second_proxy,
            test.proxy_supports_quic, false);
      } else {
        if (!test.is_in_quic_field_trial) {
          histogram_tester.ExpectUniqueSample(
              "DataReductionProxy.Quic.ProxyStatus",
              3 /* QUIC_PROXY_DISABLED_VIA_FIELD_TRIAL */, 1);
        } else {
          histogram_tester.ExpectTotalCount(
              "DataReductionProxy.Quic.ProxyStatus", 0);
        }
      }
    }

    {
      // Test if the alternative proxy is correctly set if the resolved proxy is
      // a not a data reduction proxy.
      net::ProxyServer non_drp_proxy_server = net::ProxyServer::FromURI(
          "not.data.reduction.proxy.net:443", net::ProxyServer::SCHEME_HTTPS);
      proxy_info.UseProxyServer(non_drp_proxy_server);

      base::HistogramTester histogram_tester;
      delegate.OnResolveProxy(url, "GET", empty_proxy_retry_info, &proxy_info);
      EXPECT_EQ(non_drp_proxy_server, proxy_info.proxy_server());
      EXPECT_FALSE(proxy_info.alternative_proxy().is_valid());

      // Verify that the metrics are recorded correctly.
      if (!test.is_in_quic_field_trial) {
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.Quic.ProxyStatus",
            3 /* QUIC_PROXY_DISABLED_VIA_FIELD_TRIAL */, 1);
      } else {
        histogram_tester.ExpectTotalCount("DataReductionProxy.Quic.ProxyStatus",
                                          0);
      }
    }

    // Test if the alternative proxy is correctly marked as broken.
    if (expect_alternative_proxy_server_to_first_proxy) {
      base::HistogramTester histogram_tester;
      proxy_info.UseDirect();

      // Verify that when the alternative proxy server is reported as broken,
      // then it is no longer returned when OnResolveProxy is called.
      net::ProxyRetryInfoMap proxy_retry_info;
      net::ProxyRetryInfo bad_proxy_info;
      bad_proxy_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();
      bad_proxy_info.try_while_bad = false;
      net::ProxyServer bad_proxy_server(net::ProxyServer::SCHEME_QUIC,
                                        first_proxy.host_port_pair());
      proxy_retry_info[bad_proxy_server.ToURI()] = bad_proxy_info;

      delegate.OnResolveProxy(url, "GET", proxy_retry_info, &proxy_info);
      ASSERT_EQ(first_proxy, proxy_info.proxy_server());
      EXPECT_FALSE(proxy_info.alternative_proxy().is_valid());

      delegate.VerifyQuicHistogramCounts(
          histogram_tester, expect_alternative_proxy_server_to_first_proxy,
          test.proxy_supports_quic, true);
    }
  }
}

// Verifies that requests that were not proxied through data saver proxy due to
// missing config are recorded properly.
TEST_F(DataReductionProxyDelegateTest, HTTPRequests) {
  const struct {
    const char* url;
    bool enabled_by_user;
    bool expect_histogram;
  } test_cases[] = {
      {
          // Request should not be logged because data saver is disabled.
          "http://www.example.com/", false, false,
      },
      {
          "http://www.example.com/", true, true,
      },
      {
          // Request should not be logged because request is HTTPS.
          "https://www.example.com/", true, false,
      },
      {
          // Request to localhost should not be logged.
          "http://127.0.0.1/", true, false,
      },
      {
          // Special use IPv4 address for testing purposes (RFC 5735).
          "http://198.51.100.1/", true, true,
      },
  };

  for (const auto& test : test_cases) {
    ASSERT_TRUE(test.enabled_by_user || !test.expect_histogram);
    base::HistogramTester histogram_tester;
    GURL url(test.url);

    config()->UpdateConfigForTesting(test.enabled_by_user /* enabled */,
                                     false /* secure_proxies_allowed */,
                                     true /* insecure_proxies_allowed */);

    net::ProxyRetryInfoMap empty_proxy_retry_info;

    net::ProxyInfo direct_proxy_info;
    direct_proxy_info.UseDirect();
    EXPECT_TRUE(direct_proxy_info.is_direct());

    net::ProxyInfo result;
    result.Use(direct_proxy_info);
    proxy_delegate()->OnResolveProxy(url, "GET", empty_proxy_retry_info,
                                     &result);
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.ConfigService.HTTPRequests",
        test.expect_histogram ? 1 : 0);

    if (test.expect_histogram) {
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.ConfigService.HTTPRequests", 1, 1);
    }
  }
}

TEST_F(DataReductionProxyDelegateTest, OnCompletedSizeFor200) {
  const struct {
    const std::string DrpResponseHeaders;
  } test_cases[] = {
      {
          "HTTP/1.1 200 OK\r\n"
          "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
          "Warning: 199 Misc-Agent \"some warning text\"\r\n"
          "Via:\r\n"
          "Via: 1.1 Chrome-Compression-Proxy-Suffix, 9.9 other-proxy\r\n"
          "Via: 2.2 Chrome-Compression-Proxy\r\n"
          "Warning: 214 Chrome-Compression-Proxy \"Transformation Applied\"\r\n"
          "Chrome-Proxy: q=low,ofcl=10000\r\n"
          "Content-Length: 1000\r\n\r\n",
      },
      {
          "HTTP/1.1 200 OK\r\n"
          "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
          "Warning: 199 Misc-Agent \"some warning text\"\r\n"
          "Via:\r\n"
          "Via: 1.1 Chrome-Compression-Proxy-Suffix, 9.9 other-proxy\r\n"
          "Via: 2.2 Chrome-Compression-Proxy\r\n"
          "Warning: 214 Chrome-Compression-Proxy \"Transformation Applied\"\r\n"
          "Chrome-Proxy: q=low,ofcl=10000\r\n"
          "Content-Length: 1000\r\n\r\n",
      }};

  params()->UseNonSecureProxiesForHttp();

  for (const auto& test : test_cases) {
    base::HistogramTester histogram_tester;
    int64_t baseline_received_bytes = total_received_bytes();
    int64_t baseline_original_received_bytes = total_original_received_bytes();

    std::unique_ptr<net::URLRequest> request =
        FetchURLRequest(GURL("http://example.com/path/"), nullptr,
                        test.DrpResponseHeaders, 1000);

    EXPECT_EQ(request->GetTotalReceivedBytes(),
              total_received_bytes() - baseline_received_bytes);

    const std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
        test.DrpResponseHeaders.c_str(), test.DrpResponseHeaders.size());
    EXPECT_EQ(
        static_cast<int64_t>(raw_headers.size() +
                             10000 /* original_response_body */),
        total_original_received_bytes() - baseline_original_received_bytes);

    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.ConfigService.HTTPRequests", 1, 1);
  }
}

TEST_F(DataReductionProxyDelegateTest, Holdback) {
  const char kResponseHeaders[] =
      "HTTP/1.1 200 OK\r\n"
      "Via: 1.1 Chrome-Compression-Proxy-Suffix\r\n"
      "Content-Length: 10\r\n\r\n";

  const struct {
    bool holdback;
  } tests[] = {
      {
          true,
      },
      {
          false,
      },
  };
  for (const auto& test : tests) {
    if (!test.holdback)
      params()->UseNonSecureProxiesForHttp();

    base::FieldTrialList field_trial_list(nullptr);
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataCompressionProxyHoldback", test.holdback ? "Enabled" : "Control"));

    base::HistogramTester histogram_tester;
    FetchURLRequest(GURL("http://example.com/path/"), nullptr, kResponseHeaders,
                    10);
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.SuccessfulRequestCompletionCounts",
        test.holdback ? 0 : 1);
  }
}

TEST_F(DataReductionProxyDelegateTest, OnCompletedSizeFor304) {
  const struct {
    const std::string DrpResponseHeaders;
  } test_cases[] = {{
                        "HTTP/1.1 304 Not Modified\r\n"
                        "Via: 1.1 Chrome-Compression-Proxy\r\n"
                        "Chrome-Proxy: ofcl=10000\r\n\r\n",
                    },
                    {
                        "HTTP/1.1 304 Not Modified\r\n"
                        "Via: 1.1 Chrome-Compression-Proxy\r\n"
                        "Chrome-Proxy: ofcl=10000\r\n\r\n",
                    }};

  params()->UseNonSecureProxiesForHttp();

  for (const auto& test : test_cases) {
    int64_t baseline_received_bytes = total_received_bytes();
    int64_t baseline_original_received_bytes = total_original_received_bytes();

    std::unique_ptr<net::URLRequest> request = FetchURLRequest(
        GURL("http://example.com/path/"), nullptr, test.DrpResponseHeaders, 0);

    EXPECT_EQ(request->GetTotalReceivedBytes(),
              total_received_bytes() - baseline_received_bytes);

    const std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
        test.DrpResponseHeaders.c_str(), test.DrpResponseHeaders.size());
    EXPECT_EQ(
        static_cast<int64_t>(raw_headers.size() +
                             10000 /* original_response_body */),
        total_original_received_bytes() - baseline_original_received_bytes);
  }
}

TEST_F(DataReductionProxyDelegateTest, OnCompletedSizeForWriteError) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  params()->UseNonSecureProxiesForHttp();
  net::MockWrite writes[] = {
      net::MockWrite("GET http://example.com/path/ HTTP/1.1\r\n"
                     "Host: example.com\r\n"),
      net::MockWrite(net::ASYNC, net::ERR_ABORTED)};
  net::StaticSocketDataProvider socket(base::span<net::MockRead>(), writes);
  mock_socket_factory()->AddSocketDataProvider(&socket);

  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      context()->CreateRequest(GURL("http://example.com/path/"), net::IDLE,
                               &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);
  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_original_received_bytes() - baseline_original_received_bytes);
}

TEST_F(DataReductionProxyDelegateTest, OnCompletedSizeForReadError) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  params()->UseNonSecureProxiesForHttp();
  net::MockRead reads[] = {net::MockRead("HTTP/1.1 "),
                           net::MockRead(net::ASYNC, net::ERR_ABORTED)};
  net::StaticSocketDataProvider socket(reads, base::span<net::MockWrite>());
  mock_socket_factory()->AddSocketDataProvider(&socket);

  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      context()->CreateRequest(GURL("http://example.com/path/"), net::IDLE,
                               &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);
  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_original_received_bytes() - baseline_original_received_bytes);
}

TEST_F(DataReductionProxyDelegateTest, PartialRangeSavings) {
  const struct {
    std::string response_headers;
    size_t received_content_length;
    int64_t expected_original_content_length;
  } test_cases[] = {
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: 1000\r\n"
       "Chrome-Proxy: ofcl=3000\r\n\r\n",
       100, 300},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: 1000\r\n"
       "Chrome-Proxy: ofcl=1000\r\n\r\n",
       100, 100},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: 3000\r\n"
       "Chrome-Proxy: ofcl=1000\r\n\r\n",
       300, 100},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: 1000\r\n\r\n",
       100, 100},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: 1000\r\n"
       "Chrome-Proxy: ofcl=nonsense\r\n\r\n",
       100, 100},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: 0\r\n"
       "Chrome-Proxy: ofcl=1000\r\n\r\n",
       0, 1000},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Chrome-Proxy: ofcl=1000\r\n\r\n",
       100, 100},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: nonsense\r\n"
       "Chrome-Proxy: ofcl=3000\r\n\r\n",
       100, 100},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: 1000\r\n"
       "Chrome-Proxy: ofcl=0\r\n\r\n",
       100, 0},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: 1000\r\n"
       "Chrome-Proxy: ofcl=0\r\n\r\n",
       0, 0},
      {"HTTP/1.1 200 OK\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Content-Length: " +
           base::Int64ToString(static_cast<int64_t>(1) << 60) +
           "\r\n"
           "Chrome-Proxy: ofcl=" +
           base::Int64ToString((static_cast<int64_t>(1) << 60) * 3) +
           "\r\n\r\n",
       100, 300},
      {"HTTP/1.1 206 Partial Content\r\n"
       "Content-Range: bytes 0-19/40\r\n"
       "Content-Length: 20\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Chrome-Proxy: ofcl=160\r\n\r\n",
       20, 80},
      {"HTTP/1.1 206 Partial Content\r\n"
       "Content-Range: bytes 0-9/40\r\n"
       "Content-Length: 10\r\n"
       "Via: 1.1 Chrome-Compression-Proxy\r\n"
       "Chrome-Proxy: ofcl=160\r\n\r\n",
       10, 40},
  };

  params()->UseNonSecureProxiesForHttp();
  for (const auto& test : test_cases) {
    base::HistogramTester histogram_tester;
    int64_t baseline_received_bytes = total_received_bytes();
    int64_t baseline_original_received_bytes = total_original_received_bytes();

    std::string response_body(test.received_content_length, 'a');

    net::MockRead reads[] = {
        net::MockRead(net::ASYNC, test.response_headers.data(),
                      test.response_headers.size()),
        net::MockRead(net::ASYNC, response_body.data(), response_body.size()),
        net::MockRead(net::SYNCHRONOUS, net::ERR_ABORTED)};
    net::StaticSocketDataProvider socket(reads, base::span<net::MockWrite>());
    mock_socket_factory()->AddSocketDataProvider(&socket);

    net::TestDelegate test_delegate;
    std::unique_ptr<net::URLRequest> request =
        context()->CreateRequest(GURL("http://example.com"), net::IDLE,
                                 &test_delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
    request->Start();

    base::RunLoop().RunUntilIdle();

    int64_t expected_original_size =
        net::HttpUtil::AssembleRawHeaders(test.response_headers.data(),
                                          test.response_headers.size())
            .size() +
        test.expected_original_content_length;

    EXPECT_EQ(request->GetTotalReceivedBytes(),
              total_received_bytes() - baseline_received_bytes)
        << (&test - test_cases);
    EXPECT_EQ(expected_original_size, total_original_received_bytes() -
                                          baseline_original_received_bytes)
        << (&test - test_cases);
    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.ConfigService.HTTPRequests", 1, 1);
  }
}

}  // namespace

}  // namespace data_reduction_proxy
