// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"

#include <map>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/uma_util.h"
#include "net/base/proxy_server.h"
#include "net/http/http_status_code.h"
#include "net/socket/socket_test_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class WarmupURLFetcherTest : public WarmupURLFetcher {
 public:
  WarmupURLFetcherTest(network::mojom::URLLoaderFactory* url_loader_factory)
      : WarmupURLFetcher(
            base::BindRepeating(
                [](const std::vector<DataReductionProxyServer>&) {
                  return network::mojom::CustomProxyConfig::New();
                }),
            base::BindRepeating(
                &WarmupURLFetcherTest::HandleWarmupFetcherResponse,
                base::Unretained(this)),
            base::BindRepeating(&WarmupURLFetcherTest::GetHttpRttEstimate,
                                base::Unretained(this)),
            std::string() /*user_agent*/),
        url_loader_factory_(url_loader_factory) {}
  ~WarmupURLFetcherTest() override {}

  size_t callback_received_count() const { return callback_received_count_; }
  const net::ProxyServer& proxy_server_last() const {
    return proxy_server_last_;
  }
  FetchResult success_response_last() const { return success_response_last_; }

  base::TimeDelta GetFetchWaitTime() const override {
    if (!fetch_wait_time_)
      return WarmupURLFetcher::GetFetchWaitTime();

    return fetch_wait_time_.value();
  }

  void SetFetchWaitTime(base::Optional<base::TimeDelta> fetch_wait_time) {
    fetch_wait_time_ = fetch_wait_time;
  }

  void SetFetchTimeout(base::Optional<base::TimeDelta> fetch_timeout) {
    fetch_timeout_ = fetch_timeout;
  }

  using WarmupURLFetcher::FetchWarmupURL;
  using WarmupURLFetcher::GetWarmupURLWithQueryParam;
  using WarmupURLFetcher::OnFetchTimeout;
  using WarmupURLFetcher::OnURLLoadComplete;

  base::TimeDelta GetFetchTimeout() const override {
    if (!fetch_timeout_)
      return WarmupURLFetcher::GetFetchTimeout();
    return fetch_timeout_.value();
  }

  void VerifyStateCleanedUp() const {
    DCHECK(!url_loader_);
    DCHECK(!fetch_delay_timer_.IsRunning());
    DCHECK(!fetch_timeout_timer_.IsRunning());
    DCHECK(!is_fetch_in_flight_);
  }

  void SetHttpRttOverride(base::TimeDelta http_rtt) {
    http_rtt_override_ = http_rtt;
  }

  network::mojom::URLLoaderFactory* GetNetworkServiceURLLoaderFactory(
      const DataReductionProxyServer& proxy_server) override {
    return url_loader_factory_;
  }

 private:
  base::Optional<base::TimeDelta> GetHttpRttEstimate() const {
    if (http_rtt_override_)
      return http_rtt_override_.value();
    return base::TimeDelta::FromMilliseconds(5);
  }
  void HandleWarmupFetcherResponse(const net::ProxyServer& proxy_server,
                                   FetchResult success_response) {
    callback_received_count_++;
    proxy_server_last_ = proxy_server;
    success_response_last_ = success_response;
  }

  base::Optional<base::TimeDelta> fetch_wait_time_;
  size_t callback_received_count_ = 0;
  net::ProxyServer proxy_server_last_;
  FetchResult success_response_last_ = FetchResult::kFailed;
  base::Optional<base::TimeDelta> fetch_timeout_;
  base::Optional<base::TimeDelta> http_rtt_override_;
  network::mojom::URLLoaderFactory* url_loader_factory_;
  DISALLOW_COPY_AND_ASSIGN(WarmupURLFetcherTest);
};

// Test that query param for the warmup URL is randomly set.
TEST(WarmupURLFetcherTest, TestGetWarmupURLWithQueryParam) {
  base::test::SingleThreadTaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;
  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);

  GURL gurl_original;
  warmup_url_fetcher.GetWarmupURLWithQueryParam(&gurl_original);
  EXPECT_FALSE(gurl_original.query().empty());

  bool query_param_different = false;

  // Generate 5 more GURLs. At least one of them should have a different query
  // param than that of |gurl_original|. Multiple GURLs are generated to
  // probability of test failing due to query params of two GURLs being equal
  // due to chance.
  for (size_t i = 0; i < 5; ++i) {
    GURL gurl;
    warmup_url_fetcher.GetWarmupURLWithQueryParam(&gurl);
    EXPECT_EQ(gurl_original.host(), gurl.host());
    EXPECT_EQ(gurl_original.port(), gurl.port());
    EXPECT_EQ(gurl_original.path(), gurl.path());

    EXPECT_FALSE(gurl.query().empty());

    if (gurl_original.query() != gurl.query())
      query_param_different = true;
  }
  EXPECT_TRUE(query_param_different);
  warmup_url_fetcher.VerifyStateCleanedUp();
}

TEST(WarmupURLFetcherTest, TestSuccessfulFetchWarmupURLNoViaHeader) {
  base::test::SingleThreadTaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;

  base::HistogramTester histogram_tester;

  auto proxy_server = net::ProxyServer::Direct();
  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());
  warmup_url_fetcher.FetchWarmupURL(0, DataReductionProxyServer(proxy_server));
  EXPECT_TRUE(warmup_url_fetcher.IsFetchInFlight());
  task_environment.RunUntilIdle();

  auto url_response_head = network::CreateURLResponseHead(net::HTTP_OK);
  url_response_head->proxy_server = proxy_server;
  test_url_loader_factory.SimulateResponseWithoutRemovingFromPendingList(
      test_url_loader_factory.GetPendingRequest(0),
      std::move(url_response_head), "foobarbaz",
      network::URLLoaderCompletionStatus(net::OK));

  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 1, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode", net::HTTP_OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HasViaHeader", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed",
      ConvertNetProxySchemeToProxyScheme(net::ProxyServer::SCHEME_DIRECT), 1);

  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  EXPECT_EQ(net::ProxyServer::SCHEME_DIRECT,
            warmup_url_fetcher.proxy_server_last().scheme());
  // success_response_last() should be false since the response does not contain
  // the via header.
  EXPECT_EQ(WarmupURLFetcher::FetchResult::kFailed,
            warmup_url_fetcher.success_response_last());
  warmup_url_fetcher.VerifyStateCleanedUp();
}

TEST(WarmupURLFetcherTest, TestSuccessfulFetchWarmupURLWithViaHeader) {
  base::test::SingleThreadTaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;

  base::HistogramTester histogram_tester;

  auto proxy_server = net::ProxyServer::Direct();
  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());
  warmup_url_fetcher.FetchWarmupURL(0, DataReductionProxyServer(proxy_server));
  EXPECT_TRUE(warmup_url_fetcher.IsFetchInFlight());
  task_environment.RunUntilIdle();

  auto url_response_head = network::CreateURLResponseHead(net::HTTP_NOT_FOUND);
  url_response_head->proxy_server = proxy_server;
  static const char kDataReductionProxyViaValue[] =
      "Via: 1.1 Chrome-Compression-Proxy";
  url_response_head->headers->AddHeader(kDataReductionProxyViaValue);
  test_url_loader_factory.SimulateResponseWithoutRemovingFromPendingList(
      test_url_loader_factory.GetPendingRequest(0),
      std::move(url_response_head), "foobarbaz",
      network::URLLoaderCompletionStatus(net::OK));

  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 1, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode", net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HasViaHeader", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed",
      ConvertNetProxySchemeToProxyScheme(net::ProxyServer::SCHEME_DIRECT), 1);

  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  EXPECT_EQ(net::ProxyServer::SCHEME_DIRECT,
            warmup_url_fetcher.proxy_server_last().scheme());
  // The last response contained the via header.
  EXPECT_EQ(WarmupURLFetcher::FetchResult::kSuccessful,
            warmup_url_fetcher.success_response_last());
  warmup_url_fetcher.VerifyStateCleanedUp();

  // If the fetch times out, it should cause DCHECK to trigger.
  EXPECT_DCHECK_DEATH(warmup_url_fetcher.OnFetchTimeout());
}

TEST(WarmupURLFetcherTest,
     TestSuccessfulFetchWarmupURLWithViaHeaderExperimentNotEnabled) {
  base::test::SingleThreadTaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;

  base::HistogramTester histogram_tester;

  auto proxy_server = net::ProxyServer::Direct();
  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);
  warmup_url_fetcher.FetchWarmupURL(0, DataReductionProxyServer(proxy_server));
  base::RunLoop().RunUntilIdle();

  auto url_response_head = network::CreateURLResponseHead(net::HTTP_NO_CONTENT);
  url_response_head->proxy_server = proxy_server;
  static const char kDataReductionProxyViaValue[] =
      "Via: 1.1 Chrome-Compression-Proxy";
  url_response_head->headers->AddHeader(kDataReductionProxyViaValue);
  test_url_loader_factory.SimulateResponseWithoutRemovingFromPendingList(
      test_url_loader_factory.GetPendingRequest(0),
      std::move(url_response_head), "foobarbaz",
      network::URLLoaderCompletionStatus(net::OK));

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 1, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode", net::HTTP_NO_CONTENT, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HasViaHeader", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed",
      ConvertNetProxySchemeToProxyScheme(net::ProxyServer::SCHEME_DIRECT), 1);

  // The callback should be run.
  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  warmup_url_fetcher.VerifyStateCleanedUp();
}

TEST(WarmupURLFetcherTest, TestConnectionResetFetchWarmupURL) {
  base::test::SingleThreadTaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;

  base::HistogramTester histogram_tester;

  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());
  warmup_url_fetcher.FetchWarmupURL(
      0, DataReductionProxyServer(net::ProxyServer::Direct()));
  EXPECT_TRUE(warmup_url_fetcher.IsFetchInFlight());
  base::RunLoop().RunUntilIdle();

  test_url_loader_factory.SimulateResponseWithoutRemovingFromPendingList(
      test_url_loader_factory.GetPendingRequest(0),
      network::mojom::URLResponseHead::New(), "foobarbaz",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_RESET));

  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());

  const int kInvalidHttpResponseCode = -1;
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 0, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      std::abs(net::ERR_CONNECTION_RESET), 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode",
      std::abs(kInvalidHttpResponseCode), 1);
  histogram_tester.ExpectTotalCount("DataReductionProxy.WarmupURL.HasViaHeader",
                                    0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed", 0);
  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  EXPECT_EQ(net::ProxyServer::SCHEME_INVALID,
            warmup_url_fetcher.proxy_server_last().scheme());
  EXPECT_EQ(WarmupURLFetcher::FetchResult::kFailed,
            warmup_url_fetcher.success_response_last());
  warmup_url_fetcher.VerifyStateCleanedUp();
}

TEST(WarmupURLFetcherTest, TestFetchTimesout) {
  base::test::SingleThreadTaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;

  base::HistogramTester histogram_tester;

  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);
  // Set the timeout to a very low value. This should cause warmup URL fetcher
  // to run the callback with appropriate error code.
  warmup_url_fetcher.SetFetchTimeout(base::TimeDelta::FromSeconds(0));
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());
  warmup_url_fetcher.FetchWarmupURL(
      0, DataReductionProxyServer(net::ProxyServer::Direct()));
  EXPECT_TRUE(warmup_url_fetcher.IsFetchInFlight());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 0, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      net::ERR_ABORTED, 1);

  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  // The last response should have timedout.
  EXPECT_EQ(WarmupURLFetcher::FetchResult::kTimedOut,
            warmup_url_fetcher.success_response_last());
  warmup_url_fetcher.VerifyStateCleanedUp();

  // If the URL fetch completes, it should cause DCHECK to trigger.
  EXPECT_DCHECK_DEATH(
      warmup_url_fetcher.OnURLLoadComplete(std::make_unique<std::string>()));
}

TEST(WarmupURLFetcherTest, TestSuccessfulFetchWarmupURLWithDelay) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME);
  network::TestURLLoaderFactory test_url_loader_factory;

  base::HistogramTester histogram_tester;

  auto proxy_server = net::ProxyServer::Direct();
  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());
  warmup_url_fetcher.SetFetchWaitTime(base::TimeDelta::FromMilliseconds(1));
  warmup_url_fetcher.FetchWarmupURL(1, DataReductionProxyServer(proxy_server));
  task_environment.FastForwardBy(base::TimeDelta::FromMilliseconds(2));

  auto url_response_head = network::CreateURLResponseHead(net::HTTP_NOT_FOUND);
  url_response_head->proxy_server = proxy_server;
  static const char kDataReductionProxyViaValue[] =
      "Via: 1.1 Chrome-Compression-Proxy";
  url_response_head->headers->AddHeader(kDataReductionProxyViaValue);
  test_url_loader_factory.SimulateResponseWithoutRemovingFromPendingList(
      test_url_loader_factory.GetPendingRequest(0),
      std::move(url_response_head), "foobarbaz",
      network::URLLoaderCompletionStatus(net::OK));

  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.FetchSuccessful", 1, 1);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.WarmupURL.NetError",
                                      net::OK, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HttpResponseCode", net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.HasViaHeader", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURL.ProxySchemeUsed",
      ConvertNetProxySchemeToProxyScheme(net::ProxyServer::SCHEME_DIRECT), 1);

  EXPECT_EQ(1u, warmup_url_fetcher.callback_received_count());
  EXPECT_EQ(net::ProxyServer::SCHEME_DIRECT,
            warmup_url_fetcher.proxy_server_last().scheme());
  // success_response_last() should be true since the response contains the via
  // header.
  EXPECT_EQ(WarmupURLFetcher::FetchResult::kSuccessful,
            warmup_url_fetcher.success_response_last());
  warmup_url_fetcher.VerifyStateCleanedUp();
}

TEST(WarmupURLFetcherTest, TestFetchTimeoutIncreasing) {
  // Must remain in sync with warmup_url_fetcher.cc.
  constexpr base::TimeDelta kMinTimeout = base::TimeDelta::FromSeconds(30);
  constexpr base::TimeDelta kMaxTimeout = base::TimeDelta::FromSeconds(60);

  base::HistogramTester histogram_tester;

  base::test::SingleThreadTaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;

  DataReductionProxyServer proxy_server(net::ProxyServer::Direct());
  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());

  EXPECT_EQ(kMinTimeout, warmup_url_fetcher.GetFetchTimeout());

  base::TimeDelta http_rtt = base::TimeDelta::FromSeconds(2);
  warmup_url_fetcher.SetHttpRttOverride(http_rtt);
  EXPECT_EQ(kMinTimeout, warmup_url_fetcher.GetFetchTimeout());

  warmup_url_fetcher.FetchWarmupURL(1, proxy_server);
  EXPECT_EQ(http_rtt * 24, warmup_url_fetcher.GetFetchTimeout());

  warmup_url_fetcher.FetchWarmupURL(2, proxy_server);
  EXPECT_EQ(kMaxTimeout, warmup_url_fetcher.GetFetchTimeout());

  http_rtt = base::TimeDelta::FromSeconds(5);
  warmup_url_fetcher.SetHttpRttOverride(http_rtt);
  EXPECT_EQ(kMaxTimeout, warmup_url_fetcher.GetFetchTimeout());

  warmup_url_fetcher.FetchWarmupURL(0, proxy_server);
  EXPECT_EQ(http_rtt * 12, warmup_url_fetcher.GetFetchTimeout());
}

TEST(WarmupURLFetcherTest, TestFetchWaitTime) {
  base::HistogramTester histogram_tester;

  base::test::SingleThreadTaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;

  DataReductionProxyServer proxy_server(net::ProxyServer::Direct());
  WarmupURLFetcherTest warmup_url_fetcher(&test_url_loader_factory);
  EXPECT_FALSE(warmup_url_fetcher.IsFetchInFlight());

  warmup_url_fetcher.FetchWarmupURL(1, proxy_server);
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            warmup_url_fetcher.GetFetchWaitTime());

  warmup_url_fetcher.FetchWarmupURL(2, proxy_server);
  EXPECT_EQ(base::TimeDelta::FromSeconds(30),
            warmup_url_fetcher.GetFetchWaitTime());

  warmup_url_fetcher.FetchWarmupURL(1, proxy_server);
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            warmup_url_fetcher.GetFetchWaitTime());
}

}  // namespace

}  // namespace data_reduction_proxy
