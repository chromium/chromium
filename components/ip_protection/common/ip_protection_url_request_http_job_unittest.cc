// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_delegate.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;
using ::testing::Contains;
using ::testing::Key;

namespace ip_protection {

namespace {

constexpr std::string_view kUrl = "http://www.example.com/";

constexpr std::string_view kSimpleGetMockWrite =
    "GET / HTTP/1.1\r\n"
    "Host: www.example.com\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: \r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n\r\n";

constexpr std::string_view kResponseHeaders =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 12\r\n\r\n";

const std::string_view kResponseBody = "Test Content";

constexpr std::string_view kConnectToProxyBHeaders =
    "CONNECT proxy-b:443 HTTP/1.1\r\n"
    "Host: proxy-b:443\r\n"
    "Proxy-Connection: keep-alive\r\n"
    "Authorization: a-token\r\n\r\n";
constexpr std::string_view kConnectToProxyDHeaders =
    "CONNECT proxy-d:443 HTTP/1.1\r\n"
    "Host: proxy-d:443\r\n"
    "Proxy-Connection: keep-alive\r\n"
    "Authorization: a-token\r\n\r\n";

constexpr std::string_view kConnectToProxyResponse =
    "HTTP/1.1 200 Connection Established\r\n\r\n";

constexpr std::string_view kConnectToDestinationHeaders =
    "CONNECT www.example.com:80 HTTP/1.1\r\n"
    "Host: www.example.com:80\r\n"
    "Proxy-Connection: keep-alive\r\n"
    "Authorization: a-token\r\n\r\n";
constexpr std::string_view kConnectToDestinationResponse =
    "HTTP/1.1 200 Connection Established\r\n\r\n";

net::ProxyChain GetProxyChain1() {
  return net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy-a", 443),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy-b", 443)},
      /*chain_id=*/1);
}

net::ProxyChain GetProxyChain2() {
  return net::ProxyChain::ForIpProtection(
      {net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy-c", 443),
       net::ProxyServer::FromSchemeHostAndPort(net::ProxyServer::SCHEME_HTTPS,
                                               "proxy-d", 443)},
      /*chain_id=*/2);
}

net::ProxyChain GetProxyChainDirect() {
  return net::ProxyChain::ForIpProtection({});
}

// A wrapper around SocketDataProvider that owns the reads and writes.
struct SocketDataWrapper {
  SocketDataWrapper() = default;
  SocketDataWrapper(SocketDataWrapper&& other) = default;
  ~SocketDataWrapper() {
    CHECK(socket_data_provider);
    socket_data_provider->ExpectAllReadDataConsumed();
    socket_data_provider->ExpectAllWriteDataConsumed();
  }

  std::unique_ptr<net::StaticSocketDataProvider> socket_data_provider;
  // For successful requests that use SSL.
  std::vector<net::SSLSocketDataProvider> ssl_data_providers;
  // Might be empty if the request only mocks connection.
  std::vector<net::MockWrite> writes;
  std::vector<net::MockRead> reads;
};

SocketDataWrapper CreateRequestFailsSocketData() {
  SocketDataWrapper wrapper;
  wrapper.socket_data_provider =
      std::make_unique<net::StaticSocketDataProvider>();
  wrapper.socket_data_provider->set_connect_data(
      net::MockConnect(net::SYNCHRONOUS, net::ERR_CONNECTION_RESET));
  return wrapper;
}

SocketDataWrapper CreateDirectRequestSucceedsSocketData() {
  SocketDataWrapper wrapper;
  wrapper.writes.emplace_back(kSimpleGetMockWrite);

  wrapper.reads.emplace_back(kResponseHeaders);
  wrapper.reads.emplace_back(kResponseBody);

  wrapper.socket_data_provider =
      std::make_unique<net::StaticSocketDataProvider>(wrapper.reads,
                                                      wrapper.writes);
  return wrapper;
}

SocketDataWrapper CreateProxiedRequestSucceedsSocketData(
    const net::ProxyChain& proxy_chain) {
  SocketDataWrapper wrapper;

  // We can't construct the string dynamically because net::MockWrite doesn't
  // store the string; it stores a string_view.
  std::string_view connect_to_proxy_headers;
  std::string second_hop_proxy_host = proxy_chain.GetProxyServer(1).GetHost();
  if (second_hop_proxy_host == "proxy-b") {
    connect_to_proxy_headers = kConnectToProxyBHeaders;
  } else if (second_hop_proxy_host == "proxy-d") {
    connect_to_proxy_headers = kConnectToProxyDHeaders;
  } else {
    NOTREACHED();
  }
  wrapper.writes.emplace_back(connect_to_proxy_headers);
  wrapper.reads.emplace_back(kConnectToProxyResponse);
  wrapper.ssl_data_providers.emplace_back(net::ASYNC, net::OK);

  wrapper.writes.emplace_back(kConnectToDestinationHeaders);
  wrapper.reads.emplace_back(kConnectToDestinationResponse);
  wrapper.ssl_data_providers.emplace_back(net::ASYNC, net::OK);

  wrapper.writes.emplace_back(kSimpleGetMockWrite);
  wrapper.reads.emplace_back(kResponseHeaders);
  wrapper.reads.emplace_back(kResponseBody);

  wrapper.socket_data_provider =
      std::make_unique<net::StaticSocketDataProvider>(wrapper.reads,
                                                      wrapper.writes);
  return wrapper;
}

class MockIpProtectionCore : public IpProtectionCore {
 public:
  MockIpProtectionCore() = default;
  ~MockIpProtectionCore() override = default;

  void SetProxyList(std::vector<net::ProxyChain> proxy_list) {
    proxy_list_ = std::move(proxy_list);
  }

  // IpProtectionCore
  bool IsIpProtectionEnabled() override { return true; }
  bool AreAuthTokensAvailable() override { return true; }
  bool IsProxyListAvailable() override { return proxy_list_.has_value(); }
  std::vector<net::ProxyChain> GetProxyChainList() override {
    return *proxy_list_;
  }
  bool WereTokenCachesEverFilled() override { return true; }
  std::optional<BlindSignedAuthToken> GetAuthToken(
      size_t chain_index) override {
    return std::make_optional<BlindSignedAuthToken>({.token = "a-token"});
  }
  void QuicProxiesFailed() override {}
  void RequestRefreshProxyList() override {}
  void GeoObserved(const std::string& geo_id) override {}
  bool HasTrackingProtectionException(
      const GURL& first_party_url) const override {
    return false;
  }
  void SetTrackingProtectionContentSetting(
      const ContentSettingsForOneType& settings) override {}
  bool RequestShouldBeProxied(
      const GURL& request_url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    return true;
  }
  bool IsMdlPopulated() override { return true; }
  void RecordTokenDemand(size_t i) override {}

 private:
  std::optional<std::vector<net::ProxyChain>> proxy_list_;
};

// Wraps a URLRequestContext and holds other classes it needs e.g.
// ProxyResolutionService.
class UrlRequestContextWrapper {
 public:
  UrlRequestContextWrapper() {
    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->set_client_socket_factory_for_testing(&socket_factory_);

    context_builder->set_proxy_delegate(
        std::make_unique<IpProtectionProxyDelegate>(&ipp_core_));

    context_builder->set_proxy_resolution_service(
        net::ConfiguredProxyResolutionService::CreateDirect());

    context_ = context_builder->Build();
  }

  UrlRequestContextWrapper(const UrlRequestContextWrapper&) = delete;
  UrlRequestContextWrapper& operator=(const UrlRequestContextWrapper&) = delete;

  MockIpProtectionCore& ipp_core() { return ipp_core_; }
  net::MockClientSocketFactory& socket_factory() { return socket_factory_; }
  net::ProxyResolutionService& proxy_resolution_service() {
    return *context_->proxy_resolution_service();
  }

  std::unique_ptr<net::URLRequest> CreateRequest(
      const GURL& url,
      net::URLRequest::Delegate* delegate) {
    return context_->CreateRequest(url, net::DEFAULT_PRIORITY, delegate,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  void AddSocketData(SocketDataWrapper& wrapper) {
    socket_factory_.AddSocketDataProvider(wrapper.socket_data_provider.get());
    for (auto& ssl_data : wrapper.ssl_data_providers) {
      socket_factory_.AddSSLSocketDataProvider(&ssl_data);
    }
  }

 private:
  std::vector<SocketDataWrapper> socket_data_providers_;
  net::MockClientSocketFactory socket_factory_;
  MockIpProtectionCore ipp_core_;
  std::unique_ptr<net::URLRequestContext> context_;
};

class IpProtectionUrlRequestHttpJobTest : public testing::Test {
 protected:
  enum class RequestMetricsExpectations {
    // Metrics recorded for a successful request i.e. a request that used
    // an IPP ProxyChain (which can be direct when DirectOnly is enabled.)
    kSuccess,
    // Metrics recorded for requests that fell back to direct.
    kDirectFallback,
    // Metrics recorded for requests that used a direct ProxyChain and no
    // IP Protection chains.
    kDirectOnly,
  };

  void CheckPerRequestMetrics(const base::HistogramTester& histograms,
                              RequestMetricsExpectations expectations) {
    using enum RequestMetricsExpectations;
    // These are always recorded for any IPP-eligible request.
    EXPECT_THAT(
        histograms.GetAllSamples("NetworkService.IpProtection.ProxyResolution"),
        base::BucketsAre(
            base::Bucket(ProxyResolutionResult::kAttemptProxy, 1)));
    EXPECT_EQ(histograms.GetAllSamples("Net.HttpJob.TotalTime").size(), 1u);

    if (expectations == kSuccess) {
      histograms.ExpectUniqueSample("Net.HttpJob.IpProtection.BytesSent",
                                    std::size(kSimpleGetMockWrite),
                                    /*expected_bucket_count=*/1);
    } else {
      EXPECT_THAT(
          histograms.GetAllSamples("Net.HttpJob.IpProtection.BytesSent"),
          base::BucketsAre());
    }

    if (expectations == kSuccess || expectations == kDirectFallback) {
      histograms.ExpectUniqueSample("Net.HttpJob.IpProtection.BytesSent2",
                                    std::size(kSimpleGetMockWrite),
                                    /*expected_bucket_count=*/1);
    } else {
      EXPECT_THAT(
          histograms.GetAllSamples("Net.HttpJob.IpProtection.BytesSent2"),
          base::BucketsAre());
    }

    EXPECT_EQ(
        histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached")
            .size(),
        (expectations == kSuccess) ? 1u : 0u);

    EXPECT_EQ(
        histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached3")
            .size(),
        (expectations == kSuccess || expectations == kDirectFallback) ? 1u
                                                                      : 0u);

    EXPECT_EQ(histograms
                  .GetAllSamples(
                      "Net.HttpJob.IpProtection.Fallback.TotalTimeNotCached2")
                  .size(),
              (expectations == kDirectFallback) ? 1u : 0u);
  }

  void CheckPerChainMetrics(
      const base::HistogramTester& histograms,
      base::optional_ref<const net::ProxyChain> used_chain,
      base::span<const net::ProxyChain> failed_chains) {
    if (used_chain.has_value()) {
      const std::string suffix = used_chain->GetHistogramSuffix();
      histograms.ExpectTotalCount(
          base::StrCat({"Net.IpProtection.StreamCreationSuccessTime.", suffix}),
          1);
    }

    for (const auto& chain : failed_chains) {
      const std::string suffix = chain.GetHistogramSuffix();
      histograms.ExpectTotalCount(
          base::StrCat({"Net.IpProtection.StreamCreationErrorTime.", suffix}),
          1);
      histograms.ExpectTotalCount(
          base::StrCat({"Net.IpProtection.StreamCreationError.", suffix}), 1);
    }

    // Check that no extra histograms were recorded.
    EXPECT_THAT(histograms.GetAllSamplesForPrefix(
                    "Net.IpProtection.StreamCreationSuccessTime."),
                ::testing::SizeIs(used_chain.has_value() ? 1 : 0));
    EXPECT_THAT(histograms.GetAllSamplesForPrefix(
                    "Net.IpProtection.StreamCreationErrorTime."),
                ::testing::SizeIs(failed_chains.size()));
    EXPECT_THAT(histograms.GetAllSamplesForPrefix(
                    "Net.IpProtection.StreamCreationError."),
                ::testing::SizeIs(failed_chains.size()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list{
      net::features::kEnableIpProtectionProxy};
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(IpProtectionUrlRequestHttpJobTest, SuccessFirstChain) {
  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetProxyChain1(), GetProxyChain2()});

  // Mock a request to the proxy chain that succeeds.
  auto proxy_data = CreateProxiedRequestSucceedsSocketData(GetProxyChain1());
  request_context.AddSocketData(proxy_data);

  base::HistogramTester histograms;
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);
  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  ASSERT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(kResponseHeaders.size() + kResponseBody.size(),
            base::checked_cast<size_t>(request->GetTotalReceivedBytes()));
  EXPECT_EQ(kSimpleGetMockWrite.size(),
            base::checked_cast<size_t>(request->GetTotalSentBytes()));

  EXPECT_EQ(GetProxyChain1(), request->proxy_chain());
  CheckPerRequestMetrics(histograms, RequestMetricsExpectations::kSuccess);
  CheckPerChainMetrics(histograms, GetProxyChain1(), {});
}

TEST_F(IpProtectionUrlRequestHttpJobTest, SuccessSecondChain) {
  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetProxyChain1(), GetProxyChain2()});

  // Mock a request to the proxy that fails.
  auto connect_data = CreateRequestFailsSocketData();
  request_context.AddSocketData(connect_data);
  // Mock a request to the proxy chain that succeeds.
  auto proxy_data = CreateProxiedRequestSucceedsSocketData(GetProxyChain2());
  request_context.AddSocketData(proxy_data);

  base::HistogramTester histograms;
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);
  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  ASSERT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(kResponseHeaders.size() + kResponseBody.size(),
            base::checked_cast<size_t>(request->GetTotalReceivedBytes()));
  EXPECT_EQ(kSimpleGetMockWrite.size(),
            base::checked_cast<size_t>(request->GetTotalSentBytes()));

  EXPECT_EQ(GetProxyChain2(), request->proxy_chain());
  CheckPerRequestMetrics(histograms, RequestMetricsExpectations::kSuccess);
  CheckPerChainMetrics(histograms, GetProxyChain2(), {GetProxyChain1()});
}

TEST_F(IpProtectionUrlRequestHttpJobTest, FallbackToDirect) {
  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetProxyChain1()});

  // Mock a request to the proxy that fails.
  auto connect_data = CreateRequestFailsSocketData();
  request_context.AddSocketData(connect_data);
  // Mock a direct request that succeeds.
  auto direct_data = CreateDirectRequestSucceedsSocketData();
  request_context.AddSocketData(direct_data);

  net::TestDelegate delegate;
  base::HistogramTester histograms;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(direct_data.writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(direct_data.reads),
            request->GetTotalReceivedBytes());

  // Since we fall back to direct, after trying the proxy chain, we expect a
  // direct IP Protection proxy chain.
  EXPECT_EQ(GetProxyChainDirect(), request->proxy_chain());
  CheckPerRequestMetrics(histograms,
                         RequestMetricsExpectations::kDirectFallback);
  CheckPerChainMetrics(histograms, GetProxyChainDirect(), {GetProxyChain1()});
}

TEST_F(IpProtectionUrlRequestHttpJobTest, NoProxies) {
  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({});

  // Mock a direct request that succeeds.
  auto direct_data = CreateDirectRequestSucceedsSocketData();
  request_context.AddSocketData(direct_data);

  base::HistogramTester histograms;
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(direct_data.writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(direct_data.reads),
            request->GetTotalReceivedBytes());

  EXPECT_EQ(net::ProxyChain::Direct(), request->proxy_chain());
  CheckPerRequestMetrics(histograms, RequestMetricsExpectations::kDirectOnly);
  CheckPerChainMetrics(histograms, std::nullopt, {});
}

TEST_F(IpProtectionUrlRequestHttpJobTest, DirectOnlyFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {
          {net::features::kIpPrivacyDirectOnly.name, "true"},
      });

  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetProxyChain1()});

  // Mock a direct request that succeeds.
  auto direct_data = CreateDirectRequestSucceedsSocketData();
  request_context.AddSocketData(direct_data);

  base::HistogramTester histograms;
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(direct_data.writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(direct_data.reads),
            request->GetTotalReceivedBytes());

  EXPECT_EQ(GetProxyChainDirect(), request->proxy_chain());
  CheckPerRequestMetrics(histograms, RequestMetricsExpectations::kSuccess);
  CheckPerChainMetrics(histograms, GetProxyChainDirect(), {});
}

TEST_F(IpProtectionUrlRequestHttpJobTest, AllBadProxyChains) {
  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetProxyChain1(), GetProxyChain2()});

  {
    // Mock a request to the proxy that fails.
    auto connect_data1 = CreateRequestFailsSocketData();
    request_context.AddSocketData(connect_data1);

    // Mock a request to the proxy that fails.
    auto connect_data2 = CreateRequestFailsSocketData();
    request_context.AddSocketData(connect_data2);

    // Mock a direct request that succeeds.
    auto direct_data = CreateDirectRequestSucceedsSocketData();
    request_context.AddSocketData(direct_data);

    base::HistogramTester histograms;
    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request =
        request_context.CreateRequest(GURL(kUrl), &delegate);
    request->Start();
    ASSERT_TRUE(request->is_pending());
    delegate.RunUntilComplete();

    EXPECT_THAT(delegate.request_status(), IsOk());
    EXPECT_EQ(GetProxyChainDirect(), request->proxy_chain());
    EXPECT_THAT(request_context.proxy_resolution_service().proxy_retry_info(),
                Contains(Key(GetProxyChain1())));
    CheckPerRequestMetrics(histograms,
                           RequestMetricsExpectations::kDirectFallback);
    CheckPerChainMetrics(histograms, GetProxyChainDirect(),
                         {GetProxyChain1(), GetProxyChain2()});
  }

  // Mock a direct request that succeeds.
  auto direct_data = CreateDirectRequestSucceedsSocketData();
  request_context.AddSocketData(direct_data);

  base::HistogramTester histograms;
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());

  EXPECT_EQ(net::ProxyChain::Direct(), request->proxy_chain());
  CheckPerRequestMetrics(histograms, RequestMetricsExpectations::kDirectOnly);
  CheckPerChainMetrics(histograms, std::nullopt, {});
}

TEST_F(IpProtectionUrlRequestHttpJobTest, OneBadProxyChain) {
  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetProxyChain1(), GetProxyChain2()});

  {
    // Mock a request to the ProxyChain that fails. This proxy chain will be
    // marked as a bad proxy chain.
    auto connect_data = CreateRequestFailsSocketData();
    request_context.AddSocketData(connect_data);

    // Mock a request to the proxy chain that succeeds.
    auto proxy_data = CreateProxiedRequestSucceedsSocketData(GetProxyChain2());
    request_context.AddSocketData(proxy_data);

    base::HistogramTester histograms;
    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request =
        request_context.CreateRequest(GURL(kUrl), &delegate);
    request->Start();
    ASSERT_TRUE(request->is_pending());
    delegate.RunUntilComplete();

    ASSERT_THAT(delegate.request_status(), IsOk());
    EXPECT_EQ(GetProxyChain2(), request->proxy_chain());
    EXPECT_THAT(request_context.proxy_resolution_service().proxy_retry_info(),
                Contains(Key(GetProxyChain1())));
    CheckPerRequestMetrics(histograms, RequestMetricsExpectations::kSuccess);
    CheckPerChainMetrics(histograms, GetProxyChain2(), {GetProxyChain1()});
  }

  // Mock a request to the proxy that fails.
  auto connect_data = CreateRequestFailsSocketData();
  request_context.AddSocketData(connect_data);
  // Mock a request to direct that fails.
  auto direct_data = CreateRequestFailsSocketData();
  request_context.AddSocketData(direct_data);

  base::HistogramTester histograms;
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  // Since the first proxy chain was marked as bad, it should have been removed
  // from the proxy list. We shouldn't fall back and the request should fail.
  EXPECT_THAT(delegate.request_status(),
              net::test::IsError(net::ERR_CONNECTION_RESET));
  EXPECT_EQ(GetProxyChainDirect(), request->proxy_chain());
  CheckPerChainMetrics(histograms, {},
                       {GetProxyChain2(), GetProxyChainDirect()});
}

}  // namespace ip_protection
