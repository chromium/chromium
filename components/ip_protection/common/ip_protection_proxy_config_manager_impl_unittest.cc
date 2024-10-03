// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

constexpr char kGetProxyListResultHistogram[] =
    "NetworkService.IpProtection.GetProxyListResult";
constexpr char kProxyListRefreshTimeHistogram[] =
    "NetworkService.IpProtection.ProxyListRefreshTime";

constexpr char kDefaultGeoId[] = "EARTH";
constexpr char kMountainViewGeoId[] = "US,US-CA,MOUNTAIN VIEW";
constexpr char kSunnyvaleGeoId[] = "US,US-CA,SUNNYVALE";

constexpr bool kEnableTokenCacheByGeo = true;
constexpr bool kDisableTokenCacheByGeo = false;

struct GetProxyListCall {
  std::optional<std::vector<net::ProxyChain>> proxy_chains;
  std::string geo_id;
};

class MockIpProtectionConfigGetter : public IpProtectionConfigGetter {
 public:
  ~MockIpProtectionConfigGetter() override = default;

  // Register an expectation of a call to `GetIpProtectionProxyList()`,
  // returning the given proxy list manager.
  void ExpectGetProxyListCall(const GetProxyListCall& expected_call) {
    expected_get_proxy_list_calls_.push_back(expected_call);
  }

  // Register an expectation of a call to `GetProxyList()`, returning nullopt.
  void ExpectGetProxyListCallFailure() {
    GetProxyListCall failure_call{.proxy_chains = std::nullopt, .geo_id = ""};
    expected_get_proxy_list_calls_.push_back(failure_call);
  }

  // True if all expected `TryGetAuthTokens` calls have occurred.
  bool GotAllExpectedMockCalls() {
    return expected_get_proxy_list_calls_.empty();
  }

  // Reset all test expectations.
  void Reset() { expected_get_proxy_list_calls_.clear(); }

  bool IsAvailable() override { return true; }

  void TryGetAuthTokens(uint32_t batch_size,
                        ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    NOTREACHED();
  }

  void GetProxyList(GetProxyListCallback callback) override {
    ASSERT_FALSE(expected_get_proxy_list_calls_.empty())
        << "Unexpected call to GetProxyList";
    auto& expected_call = expected_get_proxy_list_calls_.front();

    std::move(callback).Run(
        expected_call.proxy_chains,
        GetGeoHintFromGeoIdForTesting(expected_call.geo_id));

    expected_get_proxy_list_calls_.pop_front();
  }

 protected:
  std::deque<GetProxyListCall> expected_get_proxy_list_calls_;
};

class MockIpProtectionCore : public IpProtectionCore {
 public:
  MOCK_METHOD(void, GeoObserved, (const std::string& geo_id), (override));

  // Dummy implementations for functions not tested in this file.
  bool IsIpProtectionEnabled() override { return true; }
  bool AreAuthTokensAvailable() override { return false; }
  std::optional<BlindSignedAuthToken> GetAuthToken(
      size_t chain_index) override {
    return std::nullopt;
  }
  bool IsProxyListAvailable() override { return false; }
  void QuicProxiesFailed() override {}
  std::vector<net::ProxyChain> GetProxyChainList() override { return {}; }
  void RequestRefreshProxyList() override {}
};

class IpProtectionProxyConfigManagerImplTest : public testing::Test {
 protected:
  IpProtectionProxyConfigManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        mock_() {}

  // In order to test the geo caching feature, the initialization of the proxy
  // list manager must be after the feature value is set.
  void SetUpIpProtectionProxyConfigManager(bool enable_cache_by_geo) {
    // Set token caching by geo param value.
    std::map<std::string, std::string> parameters;
    parameters[net::features::kIpPrivacyCacheTokensByGeo.name] =
        enable_cache_by_geo ? "true" : "false";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy, std::move(parameters));

    // Default behavior for `GeoObserved`. The default is defined here
    // (instead of in the mock) to allow access to the local instances of the
    // proxy list manager.
    ON_CALL(mock_core_, GeoObserved(testing::_))
        .WillByDefault([this](const std::string& geo_id) {
          if (ipp_proxy_list_->CurrentGeo() != geo_id) {
            ipp_proxy_list_->RefreshProxyListForGeoChange();
          }
        });

    ipp_proxy_list_ = std::make_unique<IpProtectionProxyConfigManagerImpl>(
        &mock_core_, mock_,
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

  MockIpProtectionConfigGetter mock_;

  testing::NiceMock<MockIpProtectionCore> mock_core_;

  // The IpProtectionProxyListImpl being tested.
  std::unique_ptr<IpProtectionProxyConfigManagerImpl> ipp_proxy_list_;

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The manager gets the proxy list on startup and once again on schedule.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListOnStartupGeoCachingDisabled) {
  SetUpIpProtectionProxyConfigManager(kDisableTokenCacheByGeo);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableAndTriggerProxyListRefreshingForTesting();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  // When the token caching by geo feature is enabled, the default geo will be
  // returned.
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);

  base::Time start = base::Time::Now();

  expected_call =
      GetProxyListCall{.proxy_chains = std::vector{MakeChain({"b-proxy"})},
                       .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  WaitTillClosureQuit();
  base::TimeDelta delay = net::features::kIpPrivacyProxyListFetchInterval.Get();
  EXPECT_EQ(base::Time::Now() - start, delay);

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  // When the token caching by geo feature is enabled, the default geo will be
  // returned.
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);
}

// The manager gets the proxy list on startup and once again on schedule.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListOnStartupGeoCachingEnabled) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  // Called twice. Once for startup and once for the proxy list refresh.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(2);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableAndTriggerProxyListRefreshingForTesting();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  base::Time start = base::Time::Now();

  expected_call =
      GetProxyListCall{.proxy_chains = std::vector{MakeChain({"b-proxy"})},
                       .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  WaitTillClosureQuit();
  base::TimeDelta delay = net::features::kIpPrivacyProxyListFetchInterval.Get();
  EXPECT_EQ(base::Time::Now() - start, delay);

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

// The manager should continue scheduling refreshes even if the most recent
// fails.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListRefreshScheduledIfRefreshFails) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  // Called once for the proxy list refresh after the first refresh fails.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  mock_.ExpectGetProxyListCallFailure();
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableAndTriggerProxyListRefreshingForTesting();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // First refresh was failure, but next refresh should occur on schedule.
  EXPECT_FALSE(ipp_proxy_list_->IsProxyListAvailable());

  base::Time start = base::Time::Now();

  GetProxyListCall expected_call =
      GetProxyListCall{.proxy_chains = std::vector{MakeChain({"b-proxy"})},
                       .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  WaitTillClosureQuit();
  base::TimeDelta delay = net::features::kIpPrivacyProxyListFetchInterval.Get();
  EXPECT_EQ(base::Time::Now() - start, delay);

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // After a successful call, the proxy list should be available.
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

// The manager refreshes the proxy list on demand, but only once even if
// `RequestRefreshProxyList()` is called repeatedly.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListRefreshGeoCachingDisabled) {
  SetUpIpProtectionProxyConfigManager(kDisableTokenCacheByGeo);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableProxyListRefreshingForTesting();
  ipp_proxy_list_->RequestRefreshProxyList();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  // When the token caching by geo feature is enabled, the default geo will be
  // returned.
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);
}

// The manager refreshes the proxy list on demand, but only once even if
// `RequestRefreshProxyList()` is called repeatedly.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListRefreshGeoCachingEnabled) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  // Repeated calls should not impact how many times the geo change occurs.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableProxyListRefreshingForTesting();
  ipp_proxy_list_->RequestRefreshProxyList();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

TEST_F(IpProtectionProxyConfigManagerImplTest,
       IsProxyListAvailableEvenIfEmptyGeoCachingDisabled) {
  SetUpIpProtectionProxyConfigManager(kDisableTokenCacheByGeo);

  mock_.ExpectGetProxyListCall(GetProxyListCall{
      .proxy_chains = std::vector<net::ProxyChain>{},  // Empty ProxyList
      .geo_id = ""                                     // Empty Geo Id
  });
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  // Should show available even if geo is present and list is empty.
  mock_.ExpectGetProxyListCall(GetProxyListCall{
      .proxy_chains = std::vector<net::ProxyChain>{},  // Empty ProxyList
      .geo_id = kMountainViewGeoId});
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
}

TEST_F(IpProtectionProxyConfigManagerImplTest,
       IsProxyListAvailableEvenIfEmptyGeoCachingEnabled) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  mock_.ExpectGetProxyListCall(GetProxyListCall{
      .proxy_chains = std::vector<net::ProxyChain>{},  // Empty ProxyList
  });
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  // Should show available even if geo is present and list is empty.
  mock_.ExpectGetProxyListCall(GetProxyListCall{
      .proxy_chains = std::vector<net::ProxyChain>{},  // Empty ProxyList
      .geo_id = kMountainViewGeoId});
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
}

// The manager keeps its existing proxy list if it fails to fetch a new one.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListKeptAfterFailureGeoCachingDisabled) {
  SetUpIpProtectionProxyConfigManager(kDisableTokenCacheByGeo);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  // When the token caching by geo feature is enabled, the default geo will be
  // returned.
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  mock_.ExpectGetProxyListCallFailure();
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  // When the token caching by geo feature is enabled, the default geo will be
  // returned.
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);

  // GeoHint is returned but ProxyChain is failure.
  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  GetProxyListCall expected_call_fail{.proxy_chains = std::nullopt,
                                      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call_fail);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);
}

// The manager keeps its existing proxy list if it fails to fetch a new one.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListKeptAfterFailureGeoCachingEnabled) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  mock_.ExpectGetProxyListCallFailure();
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  // GeoHint is returned but ProxyChain is failure.
  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  GetProxyListCall expected_call_fail{.proxy_chains = std::nullopt,
                                      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call_fail);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

TEST_F(IpProtectionProxyConfigManagerImplTest, GetProxyListFailureRecorded) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  mock_.ExpectGetProxyListCallFailure();
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  histogram_tester_.ExpectUniqueSample(kGetProxyListResultHistogram,
                                       GetProxyListResult::kFailed, 1);
  histogram_tester_.ExpectTotalCount(kProxyListRefreshTimeHistogram, 0);
}

TEST_F(IpProtectionProxyConfigManagerImplTest, GotEmptyProxyListRecorded) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  mock_.ExpectGetProxyListCall(GetProxyListCall{
      .proxy_chains = std::vector<net::ProxyChain>{},  // Empty ProxyList
  });
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  histogram_tester_.ExpectUniqueSample(kGetProxyListResultHistogram,
                                       GetProxyListResult::kEmptyList, 1);
  histogram_tester_.ExpectTotalCount(kProxyListRefreshTimeHistogram, 1);
}

TEST_F(IpProtectionProxyConfigManagerImplTest, GotPopulatedProxyListRecorded) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  histogram_tester_.ExpectUniqueSample(kGetProxyListResultHistogram,
                                       GetProxyListResult::kPopulatedList, 1);
  histogram_tester_.ExpectTotalCount(kProxyListRefreshTimeHistogram, 1);
}

TEST_F(IpProtectionProxyConfigManagerImplTest,
       CurrentGeoCachingByGeoDisabledReturnsDefault) {
  SetUpIpProtectionProxyConfigManager(kDisableTokenCacheByGeo);

  // Current Geo is set immediately to default if feature is disabled.
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);

  // A refreshed list with a geo present should not change the current geo.
  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);
}

TEST_F(IpProtectionProxyConfigManagerImplTest,
       CurrentGeoCachingByGeoEnabledReturnsGeoOfProxyList) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  // Current Geo is not set on initialization, so empty geo should be
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), "");

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

// If the geo caching feature is disabled, setting the geo should have no effect
// and should continue returning the default geo.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       RefreshProxyListForGeoChangeCachingByGeoDisabledNoRefresh) {
  SetUpIpProtectionProxyConfigManager(kDisableTokenCacheByGeo);

  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);

  ipp_proxy_list_->RefreshProxyListForGeoChange();

  // A refresh does not occur since the feature is disabled.
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
}

TEST_F(IpProtectionProxyConfigManagerImplTest,
       RefreshProxyListForGeoChangeCachingByGeoEnabledGeoChanged) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  // Current Geo is not set on initialization, so empty geo should be
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), "");

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  // Simulate `IpProtectionCore.GeoObserved` being called from
  // outside this class which results in `RefreshProxyListForGeoChange` being
  // called. Expected call will contain a different geo.
  expected_call = GetProxyListCall{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kSunnyvaleGeoId};
  mock_.ExpectGetProxyListCall(expected_call);

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  QuitClosureOnRefresh();
  ipp_proxy_list_->RefreshProxyListForGeoChange();
  WaitTillClosureQuit();

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  // Refresh will contain new geo which will set the current geo.
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kSunnyvaleGeoId);
}

// If `RefreshProxyListForGeoChange` is called multiple times, the refresh is
// only requested once within the default interval.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       RefreshProxyListForGeoChangeCachingByGeoEnabledOnlyObservesGeo) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();

  ipp_proxy_list_->RefreshProxyListForGeoChange();
  ipp_proxy_list_->RefreshProxyListForGeoChange();
  WaitTillClosureQuit();

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

// If a proxy list refresh returns the same geo as the current geo, no callbacks
// to `GeoObserved` are made.
TEST_F(IpProtectionProxyConfigManagerImplTest,
       CachingByGeoNoGeoObservedWhenNewGeoMatchesCurrent) {
  SetUpIpProtectionProxyConfigManager(kEnableTokenCacheByGeo);

  // Each refresh will result in a new call.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(2);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);

  // An additional refresh is needed. This refresh contains the same geo, so
  // there should not be a call to `IpProtectionCore.GeoObserved`.
  expected_call = GetProxyListCall{
      .proxy_chains = std::vector{MakeChain({"a-proxy", "b-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);

  // Advance the clock by the min refresh interval, so that the test does not
  // hang.
  task_environment_.FastForwardBy(
      net::features::kIpPrivacyProxyListMinFetchInterval.Get());

  QuitClosureOnRefresh();
  ipp_proxy_list_->RequestRefreshProxyList();
  WaitTillClosureQuit();

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  ASSERT_EQ(ipp_proxy_list_->CurrentGeo(), kMountainViewGeoId);
}

TEST_F(IpProtectionProxyConfigManagerImplTest,
       ProxyListRefreshFetchIntervalFuzzed) {
  SetUpIpProtectionProxyConfigManager(kDisableTokenCacheByGeo);

  GetProxyListCall expected_call{
      .proxy_chains = std::vector{MakeChain({"a-proxy"})},
      .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
  QuitClosureOnRefresh();
  ipp_proxy_list_->EnableProxyListFetchIntervalFuzzingForTesting(
      /*enable=*/true);
  ipp_proxy_list_->EnableAndTriggerProxyListRefreshingForTesting();
  WaitTillClosureQuit();
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(ipp_proxy_list_->IsProxyListAvailable());
  EXPECT_EQ(ipp_proxy_list_->ProxyList(), expected_call.proxy_chains);
  EXPECT_EQ(ipp_proxy_list_->CurrentGeo(), kDefaultGeoId);

  base::Time start = base::Time::Now();
  expected_call =
      GetProxyListCall{.proxy_chains = std::vector{MakeChain({"b-proxy"})},
                       .geo_id = kMountainViewGeoId};
  mock_.ExpectGetProxyListCall(expected_call);
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
