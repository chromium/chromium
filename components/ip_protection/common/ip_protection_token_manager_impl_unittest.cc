// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_manager_impl.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

constexpr char kGeoChangeTokenPresence[] =
    "NetworkService.IpProtection.GeoChangeTokenPresence";

constexpr char kGetAuthTokenResultHistogram[] =
    "NetworkService.IpProtection.GetAuthTokenResult";
constexpr char kProxyATokenSpendRateHistogram[] =
    "NetworkService.IpProtection.ProxyA.TokenSpendRate";
constexpr char kProxyATokenExpirationRateHistogram[] =
    "NetworkService.IpProtection.ProxyA.TokenExpirationRate";
constexpr char kProxyBTokenSpendRateHistogram[] =
    "NetworkService.IpProtection.ProxyB.TokenSpendRate";
constexpr char kProxyBTokenExpirationRateHistogram[] =
    "NetworkService.IpProtection.ProxyB.TokenExpirationRate";
constexpr char kTokenBatchGenerationTimeHistogram[] =
    "NetworkService.IpProtection.TokenBatchGenerationTime";
constexpr char kGetAuthTokenResultForGeoHistogram[] =
    "NetworkService.IpProtection.GetAuthTokenResultForGeo";

constexpr base::TimeDelta kTokenLimitExceededDelay = base::Minutes(10);
constexpr base::TimeDelta kTokenRateMeasurementInterval = base::Minutes(5);

const bool kEnableTokenCacheByGeo = true;
const bool kDisableTokenCacheByGeo = false;

const GeoHint kMountainViewGeo = {.country_code = "US",
                                  .iso_region = "US-CA",
                                  .city_name = "MOUNTAIN VIEW"};
const std::string kMountainViewGeoId = GetGeoIdFromGeoHint(kMountainViewGeo);

const GeoHint kSunnyvaleGeo = {.country_code = "US",
                               .iso_region = "US-CA",
                               .city_name = "SUNNYVALE"};
const std::string kSunnyvaleGeoId = GetGeoIdFromGeoHint(kSunnyvaleGeo);

struct ExpectedTryGetAuthTokensCall {
  // The expected batch_size argument for the call.
  uint32_t batch_size;
  // The response to the call.
  std::optional<std::vector<BlindSignedAuthToken>> bsa_tokens;
  std::optional<base::Time> try_again_after;
};

class MockIpProtectionConfigGetter : public IpProtectionConfigGetter {
 public:
  ~MockIpProtectionConfigGetter() override = default;

  // Register an expectation of a call to `TryGetAuthTokens()` returning the
  // given tokens.
  void ExpectTryGetAuthTokensCall(
      uint32_t batch_size,
      std::vector<BlindSignedAuthToken> bsa_tokens) {
    expected_try_get_auth_token_calls_.emplace_back(
        ExpectedTryGetAuthTokensCall{
            .batch_size = batch_size,
            .bsa_tokens = std::move(bsa_tokens),
            .try_again_after = std::nullopt,
        });
  }

  // Register an expectation of a call to `TryGetAuthTokens()` returning no
  // tokens and the given `try_again_after`.
  void ExpectTryGetAuthTokensCall(uint32_t batch_size,
                                  base::Time try_again_after) {
    expected_try_get_auth_token_calls_.emplace_back(
        ExpectedTryGetAuthTokensCall{
            .batch_size = batch_size,
            .bsa_tokens = std::nullopt,
            .try_again_after = try_again_after,
        });
  }

  // True if all expected `TryGetAuthTokens` calls have occurred.
  bool GotAllExpectedMockCalls() {
    return expected_try_get_auth_token_calls_.empty();
  }

  // Reset all test expectations.
  void Reset() { expected_try_get_auth_token_calls_.clear(); }

  bool IsAvailable() override { return true; }

  void TryGetAuthTokens(uint32_t batch_size,
                        ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    ASSERT_FALSE(expected_try_get_auth_token_calls_.empty())
        << "Unexpected call to TryGetAuthTokens";
    auto& exp = expected_try_get_auth_token_calls_.front();
    EXPECT_EQ(batch_size, exp.batch_size);
    std::move(callback).Run(std::move(exp.bsa_tokens), exp.try_again_after);
    expected_try_get_auth_token_calls_.pop_front();
  }

  void GetProxyList(GetProxyListCallback callback) override { NOTREACHED(); }

 protected:
  std::deque<ExpectedTryGetAuthTokensCall> expected_try_get_auth_token_calls_;
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

struct HistogramState {
  // Number of successful calls to GetAuthToken (true).
  int success;
  // Number of failed calls to GetAuthToken (false).
  int failure;
  // Number of successful token batch generations.
  int generated;
};

class IpProtectionTokenManagerImplTest : public testing::Test {
 protected:
  IpProtectionTokenManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        mock_() {}

  // In order to test geo caching feature, the initialization of the token cache
  // manager must be after the feature value is set.
  void SetUpIpProtectionTokenManager(bool enable_cache_by_geo) {
    // Set token caching by geo param value.
    std::map<std::string, std::string> parameters;
    parameters[net::features::kIpPrivacyCacheTokensByGeo.name] =
        enable_cache_by_geo ? "true" : "false";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy, std::move(parameters));

    // Default behavior for `GeoObserved`. The default is defined here
    // (instead of in the mock) to allow access to the local instances of the
    // token cache managers.
    ON_CALL(mock_core_, GeoObserved(testing::_))
        .WillByDefault([this](const std::string& geo_id) {
          if (ipp_proxy_a_token_manager_->CurrentGeo() != geo_id) {
            ipp_proxy_a_token_manager_->SetCurrentGeo(geo_id);
          }
          if (ipp_proxy_b_token_manager_->CurrentGeo() != geo_id) {
            ipp_proxy_b_token_manager_->SetCurrentGeo(geo_id);
          }
        });

    ipp_proxy_a_token_manager_ = std::make_unique<IpProtectionTokenManagerImpl>(
        &mock_core_, &mock_, ProxyLayer::kProxyA,
        /* disable_cache_management_for_testing=*/true);
    ipp_proxy_b_token_manager_ = std::make_unique<IpProtectionTokenManagerImpl>(
        &mock_core_, &mock_, ProxyLayer::kProxyB,
        /* disable_cache_management_for_testing=*/true);

    // Default to disabling token expiration fuzzing.
    ipp_proxy_a_token_manager_->EnableTokenExpirationFuzzingForTesting(false);
    ipp_proxy_b_token_manager_->EnableTokenExpirationFuzzingForTesting(false);
  }

  void ExpectHistogramState(HistogramState state) {
    histogram_tester_.ExpectBucketCount(kGetAuthTokenResultHistogram, true,
                                        state.success);
    histogram_tester_.ExpectBucketCount(kGetAuthTokenResultHistogram, false,
                                        state.failure);
    histogram_tester_.ExpectTotalCount(kTokenBatchGenerationTimeHistogram,
                                       state.generated);
  }

  // Create a batch of tokens.
  std::vector<BlindSignedAuthToken> TokenBatch(
      int count,
      base::Time expiration,
      GeoHint geo_hint = kMountainViewGeo) {
    std::vector<BlindSignedAuthToken> tokens;
    for (int i = 0; i < count; i++) {
      tokens.emplace_back(
          BlindSignedAuthToken{.token = base::StringPrintf("token-%d", i),
                               .expiration = expiration,
                               .geo_hint = geo_hint});
    }
    return tokens;
  }

  void CallTryGetAuthTokensAndWait(ProxyLayer proxy_layer) {
    if (proxy_layer == ProxyLayer::kProxyA) {
      ipp_proxy_a_token_manager_->SetOnTryGetAuthTokensCompletedForTesting(
          task_environment_.QuitClosure());
      ipp_proxy_a_token_manager_->CallTryGetAuthTokensForTesting();
    } else {
      ipp_proxy_b_token_manager_->SetOnTryGetAuthTokensCompletedForTesting(
          task_environment_.QuitClosure());
      ipp_proxy_b_token_manager_->CallTryGetAuthTokensForTesting();
    }
    task_environment_.RunUntilQuit();
  }

  // Wait until the cache fills itself.
  void WaitForTryGetAuthTokensCompletion(ProxyLayer proxy_layer) {
    if (proxy_layer == ProxyLayer::kProxyA) {
      ipp_proxy_a_token_manager_->SetOnTryGetAuthTokensCompletedForTesting(
          task_environment_.QuitClosure());
    } else {
      ipp_proxy_b_token_manager_->SetOnTryGetAuthTokensCompletedForTesting(
          task_environment_.QuitClosure());
    }
    task_environment_.RunUntilQuit();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  int expected_batch_size_ =
      net::features::kIpPrivacyAuthTokenCacheBatchSize.Get();
  int cache_low_water_mark_ =
      net::features::kIpPrivacyAuthTokenCacheLowWaterMark.Get();

  // Expiration times with respect to the TaskEnvironment's mock time.
  const base::Time kFutureExpiration = base::Time::Now() + base::Hours(1);
  const base::Time kPastExpiration = base::Time::Now() - base::Hours(1);

  MockIpProtectionConfigGetter mock_;

  testing::NiceMock<MockIpProtectionCore> mock_core_;

  std::unique_ptr<IpProtectionTokenManagerImpl> ipp_proxy_a_token_manager_;

  std::unique_ptr<IpProtectionTokenManagerImpl> ipp_proxy_b_token_manager_;

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// `IsAuthTokenAvailable()` returns false on an empty cache. Geo Caching
// disabled.
TEST_F(IpProtectionTokenManagerImplTest, IsAuthTokenAvailableFalseEmpty) {
  SetUpIpProtectionTokenManager(kDisableTokenCacheByGeo);
  EXPECT_FALSE(ipp_proxy_a_token_manager_->IsAuthTokenAvailable());
}

// `IsAuthTokenAvailable()` returns false on an empty cache. Geo Caching
// enabled.
TEST_F(IpProtectionTokenManagerImplTest,
       IsAuthTokenAvailableFalseEmptyGeoCachingEnabled) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(1, kFutureExpiration, kSunnyvaleGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Looking for geo that is not found in the token. In this case, only
  // sunnyvale tokens are available, thus, a check for Mountain View will be
  // false.
  EXPECT_FALSE(
      ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));
}

// `IsAuthTokenAvailable()` returns true on a cache containing unexpired
// tokens.
TEST_F(IpProtectionTokenManagerImplTest, IsAuthTokenAvailableTrue) {
  SetUpIpProtectionTokenManager(kDisableTokenCacheByGeo);

  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(1, kFutureExpiration));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  EXPECT_TRUE(ipp_proxy_a_token_manager_->IsAuthTokenAvailable());
}

// `IsAuthTokenAvailable()` returns true on a cache containing unexpired
// tokens. Geo caching is enabled.
TEST_F(IpProtectionTokenManagerImplTest,
       IsAuthTokenAvailableTrueGeoCachingEnabled) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(1, kFutureExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  EXPECT_TRUE(
      ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));
}

// `IsAuthTokenAvailable()` returns false on a cache containing expired
// tokens.
TEST_F(IpProtectionTokenManagerImplTest, IsAuthTokenAvailableFalseExpired) {
  SetUpIpProtectionTokenManager(kDisableTokenCacheByGeo);

  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(1, kPastExpiration));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_FALSE(ipp_proxy_a_token_manager_->IsAuthTokenAvailable());
}

// `IsAuthTokenAvailable()` returns false on a geo's cache containing expired
// tokens.
TEST_F(IpProtectionTokenManagerImplTest,
       IsAuthTokenAvailableGeoCachingExpiration) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // Expired tokens added
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(1, kPastExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  EXPECT_FALSE(
      ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));
}

// `GetAuthToken()` returns nullopt on an empty cache.
TEST_F(IpProtectionTokenManagerImplTest, GetAuthTokenEmpty) {
  SetUpIpProtectionTokenManager(kDisableTokenCacheByGeo);

  EXPECT_FALSE(ipp_proxy_a_token_manager_->GetAuthToken());
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
  // No sample taken since geo caching is disabled.
  histogram_tester_.ExpectTotalCount(kGetAuthTokenResultForGeoHistogram, 0);
}

// `GetAuthToken()` returns nullopt on an empty cache for specified geo.
TEST_F(IpProtectionTokenManagerImplTest, GetAuthTokenEmptyForGeo) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  EXPECT_FALSE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
  ExpectHistogramState(HistogramState{.success = 0, .failure = 1});
  histogram_tester_.ExpectUniqueSample(
      kGetAuthTokenResultForGeoHistogram,
      AuthTokenResultForGeo::kUnavailableCacheEmpty, 1);
}

// `GetAuthToken()` returns a token on a cache containing unexpired tokens.
TEST_F(IpProtectionTokenManagerImplTest, GetAuthTokenSuccessful) {
  SetUpIpProtectionTokenManager(kDisableTokenCacheByGeo);

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(1, kFutureExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  std::optional<BlindSignedAuthToken> token =
      ipp_proxy_a_token_manager_->GetAuthToken();
  ASSERT_TRUE(token);
  EXPECT_EQ(token->token, "token-0");
  EXPECT_EQ(token->expiration, kFutureExpiration);
  EXPECT_EQ(token->geo_hint, kMountainViewGeo);
  ExpectHistogramState(
      HistogramState{.success = 1, .failure = 0, .generated = 1});
  // No sample taken since geo caching is disabled.
  histogram_tester_.ExpectTotalCount(kGetAuthTokenResultForGeoHistogram, 0);
}

// `GetAuthToken()` returns a token cached by geo.
TEST_F(IpProtectionTokenManagerImplTest, GetAuthTokenForGeoSuccessful) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(1, kFutureExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  std::optional<BlindSignedAuthToken> token =
      ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId);

  ASSERT_TRUE(token);
  EXPECT_EQ(token->token, "token-0");
  EXPECT_EQ(token->expiration, kFutureExpiration);
  EXPECT_EQ(token->geo_hint, kMountainViewGeo);
  ExpectHistogramState(
      HistogramState{.success = 1, .failure = 0, .generated = 1});
  histogram_tester_.ExpectUniqueSample(
      kGetAuthTokenResultForGeoHistogram,
      AuthTokenResultForGeo::kAvailableForCurrentGeo, 1);
}

// `GetAuthToken()` requested for geo not available while other tokens are
// available.
TEST_F(IpProtectionTokenManagerImplTest, GetAuthTokenForUnavailableGeo) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  // Cache contains Mountain view geo tokens.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(1, kFutureExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Requesting token from geo that is not Mountain View.
  std::optional<BlindSignedAuthToken> token =
      ipp_proxy_a_token_manager_->GetAuthToken(kSunnyvaleGeoId);

  ASSERT_FALSE(token);
  ExpectHistogramState(
      HistogramState{.success = 0, .failure = 1, .generated = 1});
  histogram_tester_.ExpectUniqueSample(
      kGetAuthTokenResultForGeoHistogram,
      AuthTokenResultForGeo::kUnavailableButCacheContainsTokens, 1);
}

// `GetAuthToken()` returns nullopt on a cache containing expired tokens.
TEST_F(IpProtectionTokenManagerImplTest, GetAuthTokenFalseExpired) {
  SetUpIpProtectionTokenManager(kDisableTokenCacheByGeo);

  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   TokenBatch(1, kPastExpiration));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_FALSE(ipp_proxy_a_token_manager_->GetAuthToken());
  ExpectHistogramState(
      HistogramState{.success = 0, .failure = 1, .generated = 1});
}

// `GetAuthToken()` returns nullopt for particular geo where tokens are
// expired.
TEST_F(IpProtectionTokenManagerImplTest, GetAuthTokenForGeoFalseExpired) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // Expired tokens added.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(1, kPastExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_FALSE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
  ExpectHistogramState(
      HistogramState{.success = 0, .failure = 1, .generated = 1});
}

// `CurrentGeo()` returns "EARTH" when token caching by geo is disabled.
TEST_F(IpProtectionTokenManagerImplTest, CurrentGeoNoGeoCachingReturnsEarth) {
  SetUpIpProtectionTokenManager(kDisableTokenCacheByGeo);

  EXPECT_FALSE(ipp_proxy_a_token_manager_->GetAuthToken());
  EXPECT_EQ(ipp_proxy_a_token_manager_->CurrentGeo(), "EARTH");
}

// `CurrentGeo()` should return empty if no tokens have been requested yet and
// token caching by geo is enabled.
TEST_F(IpProtectionTokenManagerImplTest,
       CurrentGeoTokensNotRequestedGeoCachingEnabledReturnsEmpty) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);
  // If no tokens have been added, this should not be called.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(0);

  EXPECT_EQ(ipp_proxy_a_token_manager_->CurrentGeo(), "");
}

// `CurrentGeo()` should return the current geo of the most recently returned
// tokens.
TEST_F(IpProtectionTokenManagerImplTest,
       CurrentGeoTokensFilledGeoCachingEnabledReturnsGeo) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(1, kFutureExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  EXPECT_EQ(ipp_proxy_a_token_manager_->CurrentGeo(), kMountainViewGeoId);
}

// If `TryGetAuthTokens()` returns a batch smaller than the low-water mark,
// the cache does not immediately refill.
TEST_F(IpProtectionTokenManagerImplTest, SmallBatch) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(cache_low_water_mark_ - 1, kFutureExpiration));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  ASSERT_TRUE(
      ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));

  ASSERT_TRUE(
      ipp_proxy_a_token_manager_->try_get_auth_tokens_after_for_testing() >
      base::Time::Now());
  ExpectHistogramState(
      HistogramState{.success = 1, .failure = 0, .generated = 1});
}

// If `TryGetAuthTokens()` returns an backoff due to an error, the cache
// remains empty.
TEST_F(IpProtectionTokenManagerImplTest, ErrorBatch) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(0);

  const base::TimeDelta kBackoff = base::Seconds(10);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_,
                                   base::Time::Now() + kBackoff);
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  ASSERT_FALSE(
      ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));
  ASSERT_FALSE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
  ExpectHistogramState(
      HistogramState{.success = 0, .failure = 1, .generated = 0});
}

// `GetAuthToken()` skips expired tokens and returns a non-expired token, if
// one is found in the cache.
TEST_F(IpProtectionTokenManagerImplTest, SkipExpiredTokens) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  std::vector<BlindSignedAuthToken> tokens =
      TokenBatch(10, kPastExpiration, kMountainViewGeo);
  tokens.emplace_back(BlindSignedAuthToken{.token = "good-token",
                                           .expiration = kFutureExpiration,
                                           .geo_hint = kMountainViewGeo});
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, std::move(tokens));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  auto got_token = ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId);
  EXPECT_EQ(got_token.value().token, "good-token");
  EXPECT_EQ(got_token.value().expiration, kFutureExpiration);
  EXPECT_EQ(got_token.value().geo_hint, kMountainViewGeo);
  ExpectHistogramState(
      HistogramState{.success = 1, .failure = 0, .generated = 1});
}

TEST_F(IpProtectionTokenManagerImplTest, TokenExpirationFuzzed) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);
  ipp_proxy_a_token_manager_->EnableTokenExpirationFuzzingForTesting(true);

  std::vector<BlindSignedAuthToken> tokens =
      TokenBatch(1, kFutureExpiration, kMountainViewGeo);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, std::move(tokens));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  auto got_token = ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId);
  EXPECT_EQ(got_token.value().token, "token-0");
  EXPECT_LT(got_token.value().expiration, kFutureExpiration);
  EXPECT_EQ(got_token.value().geo_hint, kMountainViewGeo);
  base::TimeDelta fuzz_limit = net::features::kIpPrivacyExpirationFuzz.Get();
  EXPECT_GE(got_token.value().expiration, kFutureExpiration - fuzz_limit);
}

// If the `IpProtectionConfigGetter` is nullptr, no tokens are gotten, but
// things don't crash.
TEST_F(IpProtectionTokenManagerImplTest, NullGetter) {
  MockIpProtectionCore core;
  auto ipp_token_manager = IpProtectionTokenManagerImpl(
      &core, nullptr, ProxyLayer::kProxyA,
      /* disable_cache_management_for_testing=*/true);

  EXPECT_FALSE(ipp_token_manager.IsAuthTokenAvailable(kMountainViewGeoId));

  auto token = ipp_token_manager.GetAuthToken(kMountainViewGeoId);
  ASSERT_FALSE(token);
  ExpectHistogramState(
      HistogramState{.success = 0, .failure = 1, .generated = 0});
}

// Verify that the token spend rate for ProxyA is measured correctly.
TEST_F(IpProtectionTokenManagerImplTest, ProxyATokenSpendRate) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);
  std::vector<BlindSignedAuthToken> tokens;

  // Fill the cache with 5 tokens.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(5, kFutureExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Get four tokens from the batch.
  for (int i = 0; i < 4; i++) {
    auto got_token =
        ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId);
    EXPECT_EQ(got_token.value().token, base::StringPrintf("token-%d", i));
    EXPECT_EQ(got_token.value().expiration, kFutureExpiration);
  }

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Four tokens in five minutes is a rate of 36 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kProxyATokenSpendRateHistogram, 48, 1);

  // Get the remaining token in the batch.
  auto got_token = ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId);
  EXPECT_EQ(got_token.value().token, "token-4");
  EXPECT_EQ(got_token.value().expiration, kFutureExpiration);

  // Fast-forward to run the measurement timer again, for another interval.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // One token in five minutes is a rate of 12 tokens per hour.
  histogram_tester_.ExpectBucketCount(kProxyATokenSpendRateHistogram, 12, 1);
  histogram_tester_.ExpectTotalCount(kProxyATokenSpendRateHistogram, 2);
}

// Verify that the token expiration rate for ProxyA is measured correctly.
TEST_F(IpProtectionTokenManagerImplTest, ProxyATokenExpirationRate) {
  std::vector<BlindSignedAuthToken> tokens;
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // Fill the cache with 1024 expired tokens. An entire batch expiring
  // in one 5-minute interval is a very likely event.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(1024, kPastExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Try to get a token, which will incidentally record the expired tokens.
  auto got_token = ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId);
  EXPECT_FALSE(got_token);

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // 1024 tokens in five minutes is a rate of 12288 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kProxyATokenExpirationRateHistogram,
                                       12288, 1);

  // Fast-forward to run the measurement timer again.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Zero tokens expired in this interval.
  histogram_tester_.ExpectBucketCount(kProxyATokenExpirationRateHistogram, 0,
                                      1);
  histogram_tester_.ExpectTotalCount(kProxyATokenExpirationRateHistogram, 2);
}

// Verify that the token spend rate for ProxyB is measured correctly.
TEST_F(IpProtectionTokenManagerImplTest, ProxyBTokenSpendRate) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);
  std::vector<BlindSignedAuthToken> tokens;

  // Fill the cache with 5 tokens.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_, TokenBatch(5, kFutureExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyB);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Get four tokens from the batch.
  for (int i = 0; i < 4; i++) {
    auto got_token =
        ipp_proxy_b_token_manager_->GetAuthToken(kMountainViewGeoId);
    EXPECT_EQ(got_token.value().token, base::StringPrintf("token-%d", i));
    EXPECT_EQ(got_token.value().expiration, kFutureExpiration);
  }

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Four tokens in five minutes is a rate of 36 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kProxyBTokenSpendRateHistogram, 48, 1);

  // Get the remaining token in the batch.
  auto got_token = ipp_proxy_b_token_manager_->GetAuthToken(kMountainViewGeoId);
  EXPECT_EQ(got_token.value().token, "token-4");
  EXPECT_EQ(got_token.value().expiration, kFutureExpiration);

  // Fast-forward to run the measurement timer again, for another interval.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // One token in five minutes is a rate of 12 tokens per hour.
  histogram_tester_.ExpectBucketCount(kProxyBTokenSpendRateHistogram, 12, 1);
  histogram_tester_.ExpectTotalCount(kProxyBTokenSpendRateHistogram, 2);
}

// Verify that the token expiration rate for ProxyB is measured correctly.
TEST_F(IpProtectionTokenManagerImplTest, ProxyBTokenExpirationRate) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);
  std::vector<BlindSignedAuthToken> tokens;

  // Fill the cache with 1024 expired tokens. An entire batch expiring
  // in one 5-minute interval is a very likely event.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(1024, kPastExpiration, kMountainViewGeo));
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyB);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Try to get a token, which will incidentally record the expired tokens.
  auto got_token = ipp_proxy_b_token_manager_->GetAuthToken(kMountainViewGeoId);
  EXPECT_FALSE(got_token);

  // Fast-forward to run the measurement timer.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // 1024 tokens in five minutes is a rate of 12288 tokens per hour.
  histogram_tester_.ExpectUniqueSample(kProxyBTokenExpirationRateHistogram,
                                       12288, 1);

  // Fast-forward to run the measurement timer again.
  task_environment_.FastForwardBy(kTokenRateMeasurementInterval);

  // Zero tokens expired in this interval.
  histogram_tester_.ExpectBucketCount(kProxyBTokenExpirationRateHistogram, 0,
                                      1);
  histogram_tester_.ExpectTotalCount(kProxyBTokenExpirationRateHistogram, 2);
}

// The cache will pre-fill itself with a batch of tokens after a startup delay
TEST_F(IpProtectionTokenManagerImplTest, Prefill) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  EXPECT_TRUE(
      ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));

  // Histogram should have no samples for a prefill.
  histogram_tester_.ExpectTotalCount(kGeoChangeTokenPresence, 0);
}

// The cache will initiate a refill when it reaches the low-water mark.
TEST_F(IpProtectionTokenManagerImplTest, RefillLowWaterMark) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // A refill with tokens from the same geo should not trigger this function a
  // second time.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Spend tokens down to (but not below) the low-water mark.
  for (int i = expected_batch_size_ - 1; i > cache_low_water_mark_; i--) {
    ASSERT_TRUE(
        ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));
    ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
    ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  }

  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));

  // Next call to `GetAuthToken()` should call `MaybeRefillCache()`.
  ipp_proxy_a_token_manager_->SetOnTryGetAuthTokensCompletedForTesting(
      task_environment_.QuitClosure());
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
  task_environment_.RunUntilQuit();

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
}

// If a fill results in a backoff request, the cache will try again after that
// time.
TEST_F(IpProtectionTokenManagerImplTest, RefillAfterBackoff) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  base::Time try_again_at = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, try_again_at);
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  base::Time try_again_at_2 = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, try_again_at_2);
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  EXPECT_EQ(base::Time::Now(), try_again_at);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  base::Time try_again_at_3 = base::Time::Now() + base::Seconds(20);
  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, try_again_at_3);
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  EXPECT_EQ(base::Time::Now(), try_again_at_2);
}

// When enough tokens expire to bring the cache size below the low water mark,
// it will automatically refill.
TEST_F(IpProtectionTokenManagerImplTest, RefillAfterExpiration) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // A refill with tokens from the same geo should not trigger this function a
  // second time.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(1);

  // Make a batch of tokens almost all with `expiration2`, except one expiring
  // sooner and the one expiring later. These are returned in incorrect order
  // to verify that the cache sorts by expiration time.
  std::vector<BlindSignedAuthToken> tokens;
  base::Time expiration1 = base::Time::Now() + base::Minutes(10);
  base::Time expiration2 = base::Time::Now() + base::Minutes(15);
  base::Time expiration3 = base::Time::Now() + base::Minutes(20);

  for (int i = 0; i < expected_batch_size_ - 2; i++) {
    tokens.emplace_back(BlindSignedAuthToken{.token = "exp2",
                                             .expiration = expiration2,
                                             .geo_hint = kMountainViewGeo});
  }
  tokens.emplace_back(BlindSignedAuthToken{.token = "exp3",
                                           .expiration = expiration3,
                                           .geo_hint = kMountainViewGeo});
  tokens.emplace_back(BlindSignedAuthToken{.token = "exp1",
                                           .expiration = expiration1,
                                           .geo_hint = kMountainViewGeo});

  mock_.ExpectTryGetAuthTokensCall(expected_batch_size_, std::move(tokens));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // After the first expiration, tokens should still be available and no
  // refill should have begun (which would have caused an error).
  task_environment_.FastForwardBy(expiration1 - base::Time::Now());
  ASSERT_TRUE(
      ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));

  // After the second expiration, tokens should still be available, and
  // a second batch should have been requested.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  task_environment_.FastForwardBy(expiration2 - base::Time::Now());
  ASSERT_TRUE(
      ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));

  // The un-expired token should be returned.
  auto got_token = ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId);
  EXPECT_EQ(got_token.value().token, "exp3");

  // Histogram should have no samples because after the initial fill there was
  // no geo change.
  histogram_tester_.ExpectTotalCount(kGeoChangeTokenPresence, 0);
}

// Once a geo changes, the new geo will have a key in the cache map meaning
// tokens are now available to be retrieved for new geo.
TEST_F(IpProtectionTokenManagerImplTest, GeoChangeNewGeoAvailableForGetToken) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // We have to re-mock this behavior b/c the default behavior mocks both proxy
  // A and B which would lead to incorrect histogram sampling.
  // A geo change means this is called twice: once for prefill and once for
  // second batch.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_))
      .Times(2)
      .WillRepeatedly([this](const std::string& geo_id) {
        if (ipp_proxy_a_token_manager_->CurrentGeo() != geo_id) {
          ipp_proxy_a_token_manager_->SetCurrentGeo(geo_id);
        }
      });

  // First batch should contain tokens for Mountain View geo.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Spend tokens down to (but not below) the low-water mark.
  for (int i = expected_batch_size_ - 1; i > cache_low_water_mark_; i--) {
    ASSERT_TRUE(
        ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));
    ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
    ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  }

  // New Geo (Sunnyvale) that should be retrieved once the next
  // `GetAuthToken`is called.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kSunnyvaleGeo));

  // Triggers new token retrieval.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));

  // Tokens should contain the new geo.
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // New geo should return a valid token.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kSunnyvaleGeoId));

  // There was a single geo change, but the new geo did not have any preexisting
  // tokens in the cache.
  histogram_tester_.ExpectUniqueSample(kGeoChangeTokenPresence, false, 1);
}

// Once a geo changes, the map will contain multiple geo's. Tokens from a
// prior geo are still valid to use as long as they are not expired.
TEST_F(IpProtectionTokenManagerImplTest, GeoChangeOldGeoTokensStillUsable) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // A geo change means this is called twice: once for prefill and once for
  // second batch.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(2);

  // First geo will be Mountain View.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Spend tokens down to (but not below) the low-water mark.
  for (int i = expected_batch_size_ - 1; i > cache_low_water_mark_; i--) {
    ASSERT_TRUE(
        ipp_proxy_a_token_manager_->IsAuthTokenAvailable(kMountainViewGeoId));
    ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
    ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  }

  // New Geo (Sunnyvale).
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kSunnyvaleGeo));

  // Triggers token refill.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));

  // New tokens should contain new geo.
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Old Geo can still be used if tokens are available.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));

  // Metric should only be counted when the geo requested is not the current
  // geo.
  histogram_tester_.ExpectBucketCount(
      kGetAuthTokenResultForGeoHistogram,
      AuthTokenResultForGeo::kAvailableForOtherCachedGeo, 1);
}

// Existing state with valid non-expired tokens. `SetCurrentGeo` is called
// with geo not in current cache. cache should attempt to refill and will
// contain tokens from the new geo.
TEST_F(IpProtectionTokenManagerImplTest,
       SetCurrentGeoDifferentGeoRetrievesNewTokens) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // We have to re-mock this behavior b/c the default behavior mocks both proxy
  // A and B which would lead to incorrect histogram sampling.
  // The geo change to Sunnyvale occurs through a call to `SetCurrentGeo`
  // which means there will not be an additional call to `GeoObserved`
  // aside from the first one during the prefill.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_))
      .Times(1)
      .WillRepeatedly([this](const std::string& geo_id) {
        if (ipp_proxy_a_token_manager_->CurrentGeo() != geo_id) {
          ipp_proxy_a_token_manager_->SetCurrentGeo(geo_id);
        }
      });

  // Original geo will be Mountain View.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Contains Valid Tokens.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));

  // New geo introduced by `SetCurrentGeo` (Sunnyvale).
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kSunnyvaleGeo));

  // Trigger refill.
  ipp_proxy_a_token_manager_->SetCurrentGeo(kSunnyvaleGeoId);

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // New geo should return a valid token.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kSunnyvaleGeoId));

  // There was a single geo change, but the new geo did not have any preexisting
  // tokens in the cache.
  histogram_tester_.ExpectUniqueSample(kGeoChangeTokenPresence, false, 1);
}

// If setting new geo causes overflow of tokens in cache for certain geo,
// an extended backoff timer should stop more refills until the system can
// stabilize.
TEST_F(IpProtectionTokenManagerImplTest, SetCurrentGeoNewTokensHaveSameGeo) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // Expected 2 times:
  // 1. Original geo observed when first batch of tokens are filled ("" ->
  //    Mountain View).
  // 2. `SetCurrentGeo("Sunnyvale")` causes refill and current geo to change.
  //    Refill however returns "Mountain View" which causes an additional
  //    `GeoObserved`.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_)).Times(2);

  // Mountain View geo that will be maintained from token refill requests.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Cache should have valid tokens now. New Sunnyvale geo will be set to
  // trigger token refill. Next batch of token will still return the Mountain
  // View geo in this test case.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->SetCurrentGeo(kSunnyvaleGeoId);

  // New tokens will still contain the old geo.
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Cache should have valid tokens now. New Sunnyvale geo will be set to
  // trigger token refill. Next batch of token will still return the Mountain
  // View geo in this test case.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->SetCurrentGeo(kSunnyvaleGeoId);

  // Due to the repeated triggers to refill tokens, the token limit exceeded
  // delay would have prevented an additional call to get more tokens. Thus,
  // this should return false.
  ASSERT_FALSE(mock_.GotAllExpectedMockCalls());
  ASSERT_TRUE(
      ipp_proxy_a_token_manager_->try_get_auth_tokens_after_for_testing() -
          base::Time::Now() ==
      kTokenLimitExceededDelay);
}

// When the feature for geo caching is disabled, setting the current geo does
// not change the underlying current geo.
TEST_F(IpProtectionTokenManagerImplTest,
       GeoCachingFeatureDisabledSetCurrentGeoShortCircuits) {
  SetUpIpProtectionTokenManager(kDisableTokenCacheByGeo);

  ipp_proxy_a_token_manager_->SetCurrentGeo(kMountainViewGeoId);
  ASSERT_NE(ipp_proxy_a_token_manager_->CurrentGeo(), kMountainViewGeoId);
}

// Testing the existence of tokens in the cache when a new geo matches a
// previous geo that was cached. This test mimics a geo change introduced from
// a token refill from within the class.
TEST_F(IpProtectionTokenManagerImplTest,
       GeoChangeFromWithinTokenManagerNewGeoAlreadyHasTokensPresent) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // We have to re-mock this behavior b/c the default behavior mocks both proxy
  // A and B which would lead to incorrect histogram sampling.
  // A geo change means this is called three times: once for prefill and twice
  // for the second and third batch.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_))
      .Times(3)
      .WillRepeatedly([this](const std::string& geo_id) {
        if (ipp_proxy_a_token_manager_->CurrentGeo() != geo_id) {
          ipp_proxy_a_token_manager_->SetCurrentGeo(geo_id);
        }
      });

  // First batch should contain tokens for Mountain View geo.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Spend tokens down to (but not below) the low-water mark.
  for (int i = expected_batch_size_ - 1; i > cache_low_water_mark_; i--) {
    ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
    ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  }

  // New Geo (Sunnyvale) that should be retrieved once the next
  // `GetAuthToken`is called.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kSunnyvaleGeo));
  // Triggers new token retrieval.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));
  // Tokens should contain the new geo.
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Spend tokens down to (but not below) the low-water mark.
  for (int i = expected_batch_size_ - 1; i > cache_low_water_mark_; i--) {
    ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kSunnyvaleGeoId));
    ASSERT_TRUE(mock_.GotAllExpectedMockCalls());
  }

  // New Geo (Mountain View) that should be retrieved once the next
  // `GetAuthToken`is called.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  // Triggers new token retrieval.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kSunnyvaleGeoId));
  // Tokens should contain the new geo.
  CallTryGetAuthTokensAndWait(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // There was a two geo changes not counting the prefill: Mountain View ->
  // Sunnyvale and Sunnyvale -> Mountain View. When the geo changed to Sunnyvale
  // there were no tokens matching in the cache, but the change back to Mountain
  // View contained previously cached tokens.
  histogram_tester_.ExpectBucketCount(kGeoChangeTokenPresence, true, 1);
  histogram_tester_.ExpectBucketCount(kGeoChangeTokenPresence, false, 1);
}

// Testing the existence of tokens in the cache when a new geo matches a
// previous geo that was cached. This test mimics a geo change introduced from
// outside of this class.
TEST_F(IpProtectionTokenManagerImplTest,
       GeoChangeFromOutsideTokenManagerNewGeoAlreadyHasTokensPresent) {
  SetUpIpProtectionTokenManager(kEnableTokenCacheByGeo);

  // We have to re-mock this behavior b/c the default behavior mocks both proxy
  // A and B which would lead to incorrect histogram sampling.
  // The geo change to Sunnyvale and Mountain View (second time) occurs through
  // a call to `SetCurrentGeo` which means there will not be an additional call
  // to `GeoObserved` aside from the first one during the prefill.
  EXPECT_CALL(mock_core_, GeoObserved(testing::_))
      .Times(1)
      .WillRepeatedly([this](const std::string& geo_id) {
        if (ipp_proxy_a_token_manager_->CurrentGeo() != geo_id) {
          ipp_proxy_a_token_manager_->SetCurrentGeo(geo_id);
        }
      });

  // Original geo will be Mountain View.
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));
  ipp_proxy_a_token_manager_->EnableCacheManagementForTesting();
  WaitForTryGetAuthTokensCompletion(ProxyLayer::kProxyA);
  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // Contains Valid Tokens.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));

  // New geo introduced by `SetCurrentGeo` (Sunnyvale).
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kSunnyvaleGeo));

  // Trigger refill.
  ipp_proxy_a_token_manager_->SetCurrentGeo(kSunnyvaleGeoId);

  ASSERT_TRUE(mock_.GotAllExpectedMockCalls());

  // New geo should return a valid token.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kSunnyvaleGeoId));

  // Another new geo introduced by `SetCurrentGeo` but this time, the geo was
  // previously stored in our cache (Mountain View).
  mock_.ExpectTryGetAuthTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration, kMountainViewGeo));

  // Trigger refill.
  ipp_proxy_a_token_manager_->SetCurrentGeo(kMountainViewGeoId);

  // New geo should return a valid token.
  ASSERT_TRUE(ipp_proxy_a_token_manager_->GetAuthToken(kMountainViewGeoId));

  // There was a two geo changes not counting the prefill: Mountain View ->
  // Sunnyvale and Sunnyvale -> Mountain View. When the geo changed to Sunnyvale
  // there were no tokens matching in the cache, but the change back to Mountain
  // View contained previously cached tokens.
  histogram_tester_.ExpectBucketCount(kGeoChangeTokenPresence, true, 1);
  histogram_tester_.ExpectBucketCount(kGeoChangeTokenPresence, false, 1);
}
}  // namespace
}  // namespace ip_protection
