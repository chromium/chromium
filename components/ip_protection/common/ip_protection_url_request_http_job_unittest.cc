// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/memory/raw_ptr.h"
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

const char kResponseBody[] = "Test Content";

net::ProxyChain GetIpProtectionProxyChain() {
  net::ProxyServer proxy_server_a = net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, "proxy-a", 443);
  net::ProxyServer proxy_server_b = net::ProxyServer::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, "proxy-b", 443);
  return net::ProxyChain::ForIpProtection({proxy_server_a, proxy_server_b});
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
  std::optional<std::string> GetProbabilisticRevealToken(
      const GURL& url,
      const net::SchemefulSite& top_frame_site) override {
    return std::nullopt;
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
  bool ShouldRequestIncludeProbabilisticRevealToken(
      const GURL& request_url) override {
    return false;
  }
  IpProxyStatus GetIpProxyStatus() override { return IpProxyStatus::kOk; }
  bool IsProxyBypassed() override { return false; }
  void SetBypassProxy(bool bypass_proxy) override {}
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
    auto proxy_resolution_service =
        net::ConfiguredProxyResolutionService::CreateDirect();
    proxy_resolution_service->SetProxyDelegate(&proxy_delegate_);
    context_builder->set_proxy_resolution_service(
        std::move(proxy_resolution_service));

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

 private:
  net::MockClientSocketFactory socket_factory_;
  MockIpProtectionCore ipp_core_;
  IpProtectionProxyDelegate proxy_delegate_{&ipp_core_};
  std::unique_ptr<net::URLRequestContext> context_;
};

class IpProtectionUrlRequestHttpJobTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

}  // namespace

TEST_F(IpProtectionUrlRequestHttpJobTest, FallbackToDirect) {
  base::test::ScopedFeatureList scoped_feature_list(
      net::features::kEnableIpProtectionProxy);

  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetIpProtectionProxyChain()});

  // Mock a request to the proxy that fails.
  net::MockConnect mock_connect_1(net::SYNCHRONOUS, net::ERR_CONNECTION_RESET);
  net::StaticSocketDataProvider connect_data_1;
  connect_data_1.set_connect_data(mock_connect_1);
  request_context.socket_factory().AddSocketDataProvider(&connect_data_1);

  // Mock a direct request that succeeds.
  net::MockWrite writes[] = {net::MockWrite(kSimpleGetMockWrite)};
  net::MockRead reads[] = {net::MockRead(kResponseHeaders),
                           net::MockRead(kResponseBody),
                           net::MockRead(net::ASYNC, net::OK)};
  net::StaticSocketDataProvider socket_data(reads, writes);
  request_context.socket_factory().AddSocketDataProvider(&socket_data);

  net::TestDelegate delegate;
  base::HistogramTester histograms;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());

  // Since we fall back to direct, after trying the proxy chain, we expect a
  // direct IP Protection proxy chain.
  EXPECT_EQ(net::ProxyChain::ForIpProtection({}), request->proxy_chain());
  EXPECT_THAT(
      histograms.GetAllSamples("NetworkService.IpProtection.ProxyResolution"),
      base::BucketsAre(base::Bucket(ProxyResolutionResult::kAttemptProxy, 1)));

  EXPECT_THAT(histograms.GetAllSamples("Net.HttpJob.IpProtection.BytesSent"),
              base::BucketsAre());
  histograms.ExpectUniqueSample("Net.HttpJob.IpProtection.BytesSent2",
                                std::size(kSimpleGetMockWrite),
                                /*expected_bucket_count=*/1);
  EXPECT_THAT(
      histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached"),
      base::BucketsAre());
  EXPECT_EQ(
      histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached3")
          .size(),
      1u);
  EXPECT_EQ(histograms
                .GetAllSamples(
                    "Net.HttpJob.IpProtection.Fallback.TotalTimeNotCached2")
                .size(),
            1u);
  EXPECT_EQ(histograms.GetAllSamples("Net.HttpJob.TotalTime").size(), 1u);
}

TEST_F(IpProtectionUrlRequestHttpJobTest, NoProxies) {
  base::test::ScopedFeatureList scoped_feature_list(
      net::features::kEnableIpProtectionProxy);

  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({});

  // Mock a direct request that succeeds.
  net::MockWrite writes[] = {net::MockWrite(kSimpleGetMockWrite)};
  net::MockRead reads[] = {net::MockRead(kResponseHeaders),
                           net::MockRead(kResponseBody),
                           net::MockRead(net::ASYNC, net::OK)};
  net::StaticSocketDataProvider socket_data(reads, writes);
  request_context.socket_factory().AddSocketDataProvider(&socket_data);

  base::HistogramTester histograms;
  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());

  EXPECT_EQ(net::ProxyChain::Direct(), request->proxy_chain());
  EXPECT_THAT(
      histograms.GetAllSamples("NetworkService.IpProtection.ProxyResolution"),
      base::BucketsAre(base::Bucket(ProxyResolutionResult::kAttemptProxy, 1)));

  // IP Protection metrics are not recorded because the request was made using
  // a direct proxy chain only.
  EXPECT_THAT(histograms.GetAllSamples("Net.HttpJob.IpProtection.BytesSent"),
              base::BucketsAre());
  EXPECT_THAT(histograms.GetAllSamples("Net.HttpJob.IpProtection.BytesSent2"),
              base::BucketsAre());
  EXPECT_THAT(
      histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached"),
      base::BucketsAre());
  EXPECT_THAT(
      histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached3"),
      base::BucketsAre());
  EXPECT_THAT(histograms.GetAllSamples(
                  "Net.HttpJob.IpProtection.Fallback.TotalTimeNotCached2"),
              base::BucketsAre());

  EXPECT_EQ(histograms.GetAllSamples("Net.HttpJob.TotalTime").size(), 1u);
}

TEST_F(IpProtectionUrlRequestHttpJobTest, DirectOnlyFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {
          {net::features::kIpPrivacyDirectOnly.name, "true"},
      });

  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetIpProtectionProxyChain()});

  // Mock a direct request that succeeds.
  base::HistogramTester histograms;
  net::MockWrite writes[] = {net::MockWrite(kSimpleGetMockWrite)};
  net::MockRead reads[] = {net::MockRead(kResponseHeaders),
                           net::MockRead(kResponseBody),
                           net::MockRead(net::ASYNC, net::OK)};
  net::StaticSocketDataProvider socket_data(reads, writes);
  request_context.socket_factory().AddSocketDataProvider(&socket_data);

  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());
  EXPECT_EQ(12, request->received_response_content_length());
  EXPECT_EQ(CountWriteBytes(writes), request->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads), request->GetTotalReceivedBytes());

  EXPECT_EQ(net::ProxyChain::ForIpProtection({}), request->proxy_chain());
  EXPECT_THAT(
      histograms.GetAllSamples("NetworkService.IpProtection.ProxyResolution"),
      base::BucketsAre(base::Bucket(ProxyResolutionResult::kAttemptProxy, 1)));

  // Even though we only used a direct proxy chain, all IP Protection metrics
  // are recorded because the IpPrivacyDirectOnly feature param is enabled.
  histograms.ExpectUniqueSample("Net.HttpJob.IpProtection.BytesSent",
                                std::size(kSimpleGetMockWrite),
                                /*expected_bucket_count=*/1);
  histograms.ExpectUniqueSample("Net.HttpJob.IpProtection.BytesSent2",
                                std::size(kSimpleGetMockWrite),
                                /*expected_bucket_count=*/1);
  EXPECT_EQ(
      histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached")
          .size(),
      1u);
  EXPECT_EQ(
      histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached3")
          .size(),
      1u);
  EXPECT_THAT(histograms.GetAllSamples(
                  "Net.HttpJob.IpProtection.Fallback.TotalTimeNotCached2"),
              base::BucketsAre());

  EXPECT_EQ(histograms.GetAllSamples("Net.HttpJob.TotalTime").size(), 1u);
}

TEST_F(IpProtectionUrlRequestHttpJobTest, MetricsBadProxyChain) {
  base::test::ScopedFeatureList scoped_feature_list(
      net::features::kEnableIpProtectionProxy);

  UrlRequestContextWrapper request_context;
  request_context.ipp_core().SetProxyList({GetIpProtectionProxyChain()});

  {
    // Mock a request to the proxy that fails.
    net::MockConnect mock_connect_1(net::SYNCHRONOUS,
                                    net::ERR_CONNECTION_RESET);
    net::StaticSocketDataProvider connect_data_1;
    connect_data_1.set_connect_data(mock_connect_1);
    request_context.socket_factory().AddSocketDataProvider(&connect_data_1);

    // Mock a direct request that succeeds.
    net::MockWrite writes[] = {net::MockWrite(kSimpleGetMockWrite)};
    net::MockRead reads[] = {net::MockRead(kResponseHeaders),
                             net::MockRead(kResponseBody),
                             net::MockRead(net::ASYNC, net::OK)};
    net::StaticSocketDataProvider socket_data(reads, writes);
    request_context.socket_factory().AddSocketDataProvider(&socket_data);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request =
        request_context.CreateRequest(GURL(kUrl), &delegate);
    request->Start();
    ASSERT_TRUE(request->is_pending());
    delegate.RunUntilComplete();

    EXPECT_THAT(delegate.request_status(), IsOk());
    EXPECT_EQ(net::ProxyChain::ForIpProtection({}), request->proxy_chain());
    EXPECT_THAT(request_context.proxy_resolution_service().proxy_retry_info(),
                Contains(Key(GetIpProtectionProxyChain())));
  }

  base::HistogramTester histograms;

  // Mock a direct request that succeeds.
  net::MockWrite writes[] = {net::MockWrite(kSimpleGetMockWrite)};
  net::MockRead reads[] = {net::MockRead(kResponseHeaders),
                           net::MockRead(kResponseBody),
                           net::MockRead(net::ASYNC, net::OK)};
  net::StaticSocketDataProvider socket_data(reads, writes);
  request_context.socket_factory().AddSocketDataProvider(&socket_data);

  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request =
      request_context.CreateRequest(GURL(kUrl), &delegate);

  request->Start();
  ASSERT_TRUE(request->is_pending());
  delegate.RunUntilComplete();

  EXPECT_THAT(delegate.request_status(), IsOk());

  EXPECT_EQ(net::ProxyChain::Direct(), request->proxy_chain());
  EXPECT_THAT(
      histograms.GetAllSamples("NetworkService.IpProtection.ProxyResolution"),
      base::BucketsAre(base::Bucket(ProxyResolutionResult::kAttemptProxy, 1)));

  // IP Protection metrics are not recorded because the second request was made
  // using a direct proxy chain only.
  EXPECT_THAT(histograms.GetAllSamples("Net.HttpJob.IpProtection.BytesSent"),
              base::BucketsAre());
  EXPECT_THAT(histograms.GetAllSamples("Net.HttpJob.IpProtection.BytesSent2"),
              base::BucketsAre());
  EXPECT_THAT(
      histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached"),
      base::BucketsAre());
  EXPECT_THAT(
      histograms.GetAllSamples("Net.HttpJob.IpProtection.TotalTimeNotCached3"),
      base::BucketsAre());
  EXPECT_THAT(histograms.GetAllSamples(
                  "Net.HttpJob.IpProtection.Fallback.TotalTimeNotCached2"),
              base::BucketsAre());

  EXPECT_EQ(histograms.GetAllSamples("Net.HttpJob.TotalTime").size(), 1u);
}

}  // namespace ip_protection
