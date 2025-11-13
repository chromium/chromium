// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"

#include <cstddef>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_fetcher.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
  #include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

constexpr char kGetProxyListResultHistogram[] =
    "NetworkService.IpProtection.GetProxyListResult";
constexpr char kProxyListRefreshTimeHistogram[] =
    "NetworkService.IpProtection.ProxyListRefreshTime";

constexpr char kMountainViewGeoId[] = "US,US-CA,MOUNTAIN VIEW";
constexpr char kSunnyvaleGeoId[] = "US,US-CA,SUNNYVALE";

struct GetProxyConfigCall {
  std::optional<std::vector<net::ProxyChain>> proxy_chains;
  std::string geo_id;
};

class MockIpProtectionProxyConfigFetcher
    : public IpProtectionProxyConfigFetcher {
 public:
  ~MockIpProtectionProxyConfigFetcher() override = default;

  // Register an expectation of a call to `GetIpProtectionProxyList()`,
  // returning the given proxy list manager.
  void ExpectGetProxyConfigCall(const GetProxyConfigCall& expected_call) {
    expected_get_proxy_list_calls_.push_back(expected_call);
  }

  // Register an expectation of a call to `GetProxyConfig()`, returning nullopt.
  void ExpectGetProxyConfigCallFailure() {
    GetProxyConfigCall failure_call{.proxy_chains = std::nullopt, .geo_id = ""};
    expected_get_proxy_list_calls_.push_back(failure_call);
  }

  // True if all expected `TryGetAuthTokens` calls have occurred.
  bool GotAllExpectedMockCalls() {
    return expected_get_proxy_list_calls_.empty();
  }

  // Reset all test expectations.
  void Reset() { expected_get_proxy_list_calls_.clear(); }

  void GetProxyConfig(GetProxyConfigCallback callback) override {
    ASSERT_FALSE(expected_get_proxy_list_calls_.empty())
        << "Unexpected call to GetProxyConfig";
    auto& expected_call = expected_get_proxy_list_calls_.front();

    std::move(callback).Run(
        expected_call.proxy_chains,
        GetGeoHintFromGeoIdForTesting(expected_call.geo_id));

    expected_get_proxy_list_calls_.pop_front();
  }

 protected:
  std::deque<GetProxyConfigCall> expected_get_proxy_list_calls_;
};

class MockIpProtectionCore : public IpProtectionCore {
 public:
  MOCK_METHOD(void, GeoObserved, (const std::string& geo_id), (override));
  MOCK_METHOD(void, RecordTokenDemand, (size_t chain_index), (override));

  // Dummy implementations for functions not tested in this file.
  bool IsMdlPopulated() override { return false; }
  bool RequestShouldBeProxied(
      const GURL& request_url,
      const net::NetworkAnonymizationKey& network_anonymization_key) override {
    return false;
  }
  bool IsIpProtectionEnabled() override { return true; }
  bool AreAuthTokensAvailable() override { return false; }
  bool WereTokenCachesEverFilled() override { return false; }
  std::optional<BlindSignedAuthToken> GetAuthToken(
      size_t chain_index) override {
    return std::nullopt;
  }
  bool IsProxyListAvailable() override { return false; }
  void QuicProxiesFailed() override {}
  std::vector<net::ProxyChain> GetProxyChainList() override { return {}; }
  void RequestRefreshProxyList() override {}
  bool HasTrackingProtectionException(
      const GURL& first_party_url) const override {
    return false;
  }
  void SetTrackingProtectionContentSetting(
      const ContentSettingsForOneType& settings) override {}
};

class IpProtectionProxyConfigManagerImplTest : public testing::Test {
 protected:
  IpProtectionProxyConfigManagerImplTest() {
    // Default behavior for `GeoObserved`. The default is defined here
    // (instead of in the mock) to allow access to the local instances of the
    // proxy list manager.
    ON_CALL(mock_core_, GeoObserved(testing::_))
        .WillByDefault([this](const std::string& geo_id) {
          if (ipp_proxy_list_->CurrentGeo() != geo_id) {
            ipp_proxy_list_->RequestRefreshProxyList();
          }
        });

    auto mock_fetcher = std::make_unique<MockIpProtectionProxyConfigFetcher>();
    mock_fetcher_ = mock_fetcher.get();
    ipp_proxy_list_ = std::make_unique<IpProtectionProxyConfigManagerImpl>(
        &mock_core_, std::move(mock_fetcher),
        /* disable_proxy_refreshing_for_testing=*/true);

    // Disable proxy list fetch interval fuzzing for testing.
    ipp_proxy_list_->EnableProxyListFetchIntervalFuzzingForTesting(
        /*enable=*/false);
  }

  // Wait until the proxy list is refreshed.
  void QuitClosureOnRefresh() {
    // TODO(abhijithnair): Understand what QuitClosure and RunUntilQuit does
    // and refactor these tests.
    ipp_proxy_list_->SetOnProxyListRefreshedForTesting(
        task_environment_.QuitClosure());
  }

  void WaitTillClosureQuit() { task_environment_.RunUntilQuit(); }

  // Shortcut to create a ProxyChain from hostnames.
  net::ProxyChain MakeChain(std::vector<std::string> hostnames) {
    std::vector<net::ProxyServer> servers;
    for (auto& hostname : hostnames) {
      servers.push_back(net::ProxyServer::FromSchemeHostAndPort(
          net::ProxyServer::SCHEME_HTTPS, hostname, std::nullopt));
    }
    return net::ProxyChain::ForIpProtection(servers);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  testing::NiceMock<MockIpProtectionCore> mock_core_;

  // The IpProtectionProxyListImpl being tested.
  std::unique_ptr<IpProtectionProxyConfigManagerImpl> ipp_proxy_list_;

  raw_ptr<MockIpProtectionProxyConfigFetcher> mock_fetcher_;

  base::HistogramTester histogram_tester_;
};

// The manager gets the proxy list on startup and once again on schedule.
TEST_F(IpProtectionProxyConfigManagerImplTest, ProxyListOnStartup) {
  // Called twice. Once for startup and once for the proxy list refresh.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(2);

  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableAndTriggerProxyListRefreshingForTesting();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  base::Time start = base::Time::Now();

  expected_call =
      GetProxyConfigCall{.proxy_chains = std::vector{MakeChain({"b-proxy"})},
                         .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  WaitTillClosureQuit();
  base::TimeDelta delay = net::features::kIpPrivacyProxyListFetchInterval.Get();
  EXPECT_EQ(base::Time::Now() - start, delay);

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

// The manager should continue scheduling refreshes even if the most recent
// fails.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListRefreshScheduledIfRefreshFails) {
  // Called once for the proxy list refresh after the first refresh fails.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  mock_fetcher_->ExpectGetProxyConfigCallFailure();
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableAndTriggerProxyListRefreshingForTesting();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // First refresh was failure, but next refresh should occur on schedule.
  EXPECT_FALSE(ipp_proxy_list_->IsProxyListAvailable());

  base::Time start = base::Time::Now();

  GetProxyConfigCall expected_call =
      GetProxyConfigCall{.proxy_chains = std::vector{MakeChain({"b-proxy"})},
                         .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  WaitTillClosureQuit();
  base::TimeDelta delay = net::features::kIpPrivacyProxyListFetchInterval.Get();
  EXPECT_EQ(base::Time::Now() - start, delay);

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // After a successful call, the proxy list should be available.
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

// If a failure occurs, the minimum time between fetches does not apply.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListMinTimeIgnoredWhenRefreshFails) {
  // Called once for the proxy list refresh after the first refresh fails.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  mock_fetcher_->ExpectGetProxyConfigCallFailure();
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // First refresh was failure, but next refresh should occur when requested.
  EXPECT_FALSE(ipp_proxy_list_->IsProxyListAvailable());

  GetProxyConfigCall expected_call =
      GetProxyConfigCall{.proxy_chains = std::vector{MakeChain({"b-proxy"})},
                         .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // After a successful call, the proxy list should be available.
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

// The manager refreshes the proxy list on demand, but only once even if
// `RequestRefreshProxyList()` is called repeatedly.
TEST_F(IpProtectionProxyConfigManagerImplTest, ProxyListRefresh) {
  // Repeated calls should not impact how many times the geo change occurs.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableProxyListRefreshingForTesting();
  ipp_proxy_list_->RequestRefreshProxyList();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

TEST_F(IpProtectionProxyConfigManagerImplTest,
       IsProxyListAvailableEvenIfEmpty) {
  mock_fetcher_->ExpectGetProxyConfigCall(GetProxyConfigCall{
      .proxy_chains = std::vector<net::ProxyChain>{},  // Empty ProxyList
  });
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  // Should show available even if geo is present and list is empty.
  mock_fetcher_->ExpectGetProxyConfigCall(GetProxyConfigCall{
      .proxy_chains = std::vector<net::ProxyChain>{},  // Empty ProxyList
      .geo_id = kMountainViewGeoId});
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
}

// The manager keeps its existing proxy list if it fails to fetch a new one.
TEST_F(IpProtectionProxyConfigManagerImplTest, ProxyListKeptAfterFailure) {
  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  mock_fetcher_->ExpectGetProxyConfigCallFailure();
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  // GeoHint is returned but ProxyChain is failure.
  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  GetProxyConfigCall expected_call_fail{.proxy_chains = std::nullopt,
                                        .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call_fail);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

TEST_F(IpProtectionProxyConfigManagerImplTest, GetProxyConfigFailureRecorded) {
  mock_fetcher_->ExpectGetProxyConfigCallFailure();
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  histogram_tester_.ExpectUniqueSample(kGetProxyListResultHistogram,
                                       GetProxyListResult::kFailed, 1);
  histogram_tester_.ExpectTotalCount(kProxyListRefreshTimeHistogram, 0);
}

TEST_F(IpProtectionProxyConfigManagerImplTest, GotEmptyProxyListRecorded) {
  mock_fetcher_->ExpectGetProxyConfigCall(GetProxyConfigCall{
      .proxy_chains = std::vector<net::ProxyChain>{},  // Empty ProxyList
  });
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  histogram_tester_.ExpectUniqueSample(kGetProxyListResultHistogram,
                                       GetProxyListResult::kEmptyList, 1);
  histogram_tester_.ExpectTotalCount(kProxyListRefreshTimeHistogram, 1);
}

TEST_F(IpProtectionProxyConfigManagerImplTest, GotPopulatedProxyListRecorded) {
  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  histogram_tester_.ExpectUniqueSample(kGetProxyListResultHistogram,
                                       GetProxyListResult::kPopulatedList, 1);
  histogram_tester_.ExpectTotalCount(kProxyListRefreshTimeHistogram, 1);
}

TEST_F(IpProtectionProxyConfigManagerImplTest, CurrentGeo) {
  // Current Geo is not set on initialization, so empty geo should be
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), "");

  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

TEST_F(IpProtectionProxyConfigManagerImplTest, RequestRefreshProxyList) {
  // Current Geo is not set on initialization, so empty geo should be
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), "");

  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  // Simulate `IpProtectionCore.GeoObserved` being called from
  // outside this class which results in `RequestRefreshProxyList` being
  // called. Expected call will contain a different geo.
  expected_call = GetProxyConfigCall{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kSunnyvaleGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  // Refresh will contain new geo which will set the current geo.
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kSunnyvaleGeoId);
}

// If `RequestRefreshProxyList` is called multiple times, the refresh is
// only requested once within the default interval.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       RequestRefreshProxyList_MultipleCalls_RefreshOnlyOnce) {
  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();

  ipp_proxy_list_->RequestRefreshProxyList();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

// If a proxy list refresh returns the same geo as the current geo, no callbacks
// to `GeoObserved` are made.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       RequestRefreshProxyList_SameGeo_NoGeoObserved) {
  // Each refresh will result in a new call.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(2);

  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  // An additional refresh is needed. This refresh contains the same geo, so
  // there should not be a call to `IpProtectionCore.GeoObserved`.
  expected_call = GetProxyConfigCall{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListRefreshFetchIntervalFuzzed) {
  GetProxyConfigCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableProxyListFetchIntervalFuzzingForTesting(
      /*enable=*/true);
  ipp_proxy_list_->EnableAndTriggerProxyListRefreshingForTesting();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  base::Time start = base::Time::Now();
  expected_call =
      GetProxyConfigCall{.proxy_chains = std::vector{MakeChain({"b-proxy"})},
                         .geo_id = kMountainViewGeoId};
  mock_fetcher_->ExpectGetProxyConfigCall(expected_call);
  QuitClosureOnRefresh();
  WaitTillClosureQuit();

  base::TimeDelta delay = net::features::kIpPrivacyProxyListFetchInterval.Get();
  base::TimeDelta fuzz_range =
      net::features::kIpPrivacyProxyListFetchIntervalFuzz.Get();
  // Add 10s buffer to account for possible delays before the check
  EXPECT_LE(base::Time::Now() - start, delay + fuzz_range + base::Seconds(10));
  EXPECT_GE(base::Time::Now() - start, delay - fuzz_range - base::Seconds(10));
}

}  // namespace
}  // namespace ip_protection
