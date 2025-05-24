// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_direct_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_token_fetcher_helper.h"
#include "components/ip_protection/common/mock_blind_sign_auth.h"
#include "net/base/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ip_protection {

constexpr char kTryGetAuthTokensResultHistogram[] =
    "NetworkService.IpProtection.TryGetAuthTokensResult";
constexpr char kOAuthTokenFetchHistogram[] =
    "NetworkService.IpProtection.OAuthTokenFetchTime";
constexpr char kTryGetAuthTokensErrorHistogram[] =
    "NetworkService.IpProtection.TryGetAuthTokensErrors";
constexpr char kTokenBatchHistogram[] =
    "NetworkService.IpProtection.TokenBatchRequestTime";
const GeoHint kMountainViewGeo = {.country_code = "US",
                                  .iso_region = "US-CA",
                                  .city_name = "MOUNTAIN VIEW"};
const std::string kMountainViewGeoId = GetGeoIdFromGeoHint(kMountainViewGeo);

// A mock delegate for use in testing the fetcher.
struct MockIpProtectionTokenDirectFetcherDelegate
    : public IpProtectionTokenDirectFetcher::Delegate {
  bool IsTokenFetchEnabled() override { return is_ip_protection_enabled; }
  void RequestOAuthToken(RequestOAuthTokenCallback callback) override {
    std::move(callback).Run(response_result, std::move(response_access_token));
  }

  bool is_ip_protection_enabled = true;
  TryGetAuthTokensResult response_result = TryGetAuthTokensResult::kSuccess;
  std::optional<std::string> response_access_token = "access_token";
};

class IpProtectionTokenDirectFetcherTest : public testing::Test {
 protected:
  IpProtectionTokenDirectFetcherTest()
      : expiration_time_(base::Time::Now() + base::Hours(1)),
        geo_hint_({.country_code = "US",
                   .iso_region = "US-AL",
                   .city_name = "ALABASTER"}),
        token_server_get_proxy_config_url_(GURL(base::StrCat(
            {net::features::kIpPrivacyTokenServer.Get(),
             net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()}))),
        default_transient_backoff_(
            net::features::kIpPrivacyTryGetAuthTokensTransientBackoff.Get()),
        default_bug_backoff_(
            net::features::kIpPrivacyTryGetAuthTokensBugBackoff.Get()),
        default_not_eligible_backoff_(
            net::features::kIpPrivacyTryGetAuthTokensNotEligibleBackoff.Get()) {
    auto bsa = std::make_unique<MockBlindSignAuth>();
    bsa_ = bsa.get();
    fetcher_ = std::make_unique<IpProtectionTokenDirectFetcher>(
        &delegate_, test_url_loader_factory_.GetSafeWeakWrapper()->Clone(),
        std::move(bsa));
  }

  // Call `TryGetAuthTokens()` and run until it completes.
  void TryGetAuthTokens(int num_tokens, ProxyLayer proxy_layer) {
    fetcher_->TryGetAuthTokens(num_tokens, proxy_layer,
                               tokens_future_.GetCallback());

    ASSERT_TRUE(tokens_future_.Wait()) << "TryGetAuthTokens did not call back";
  }

  // Expect that the TryGetAuthTokens call returned the given tokens.
  void ExpectTryGetAuthTokensResult(
      std::vector<BlindSignedAuthToken> bsa_tokens) {
    EXPECT_EQ(std::get<0>(tokens_future_.Get()), bsa_tokens);
  }

  // Expect that the TryGetAuthTokens call returned nullopt, with
  // `try_again_after` at the given delta from the current time.
  void ExpectTryGetAuthTokensResultFailed(base::TimeDelta try_again_delta) {
    auto& [bsa_tokens, try_again_after] = tokens_future_.Get();
    EXPECT_EQ(bsa_tokens, std::nullopt);
    if (!bsa_tokens) {
      EXPECT_EQ(*try_again_after, base::Time::Now() + try_again_delta);
    }
    // Clear future so it can be reused and accept new tokens.
    tokens_future_.Clear();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::TestFuture<const std::optional<std::vector<BlindSignedAuthToken>>,
                         std::optional<base::Time>>
      tokens_future_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  base::Time expiration_time_;

  // A convenient geo hint for fake tokens.
  GeoHint geo_hint_;

  // URL at which getProxyConfig is invoked.
  GURL token_server_get_proxy_config_url_;

  // Delegate for the fetcher under test.
  MockIpProtectionTokenDirectFetcherDelegate delegate_;

  // Fetcher under test.
  std::unique_ptr<IpProtectionTokenDirectFetcher> fetcher_;

  base::HistogramTester histogram_tester_;

  // quiche::BlindSignAuthInterface owned and used by the fetcher.
  raw_ptr<MockBlindSignAuth> bsa_;

  // Default backoff times applied for calculating `try_again_after`.
  base::TimeDelta default_transient_backoff_;
  base::TimeDelta default_bug_backoff_;
  base::TimeDelta default_not_eligible_backoff_;
};

TEST_F(IpProtectionTokenDirectFetcherTest, Success) {
  bsa_->set_tokens(
      {IpProtectionTokenFetcherHelper::CreateBlindSignTokenForTesting(
           "single-use-1", expiration_time_, geo_hint_),
       IpProtectionTokenFetcherHelper::CreateBlindSignTokenForTesting(
           "single-use-2", expiration_time_, geo_hint_)});

  TryGetAuthTokens(2, ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(
      IpProtectionTokenFetcherHelper::CreateMockBlindSignedAuthTokenForTesting(
          "single-use-1", expiration_time_, geo_hint_)
          .value());
  expected.push_back(
      IpProtectionTokenFetcherHelper::CreateMockBlindSignedAuthTokenForTesting(
          "single-use-2", expiration_time_, geo_hint_)
          .value());
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// BSA returns no tokens.
TEST_F(IpProtectionTokenDirectFetcherTest, NoTokens) {
  TryGetAuthTokens(1, ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kFailedBSAOther,
                                       1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns malformed tokens.
TEST_F(IpProtectionTokenDirectFetcherTest, MalformedTokens) {
  auto geo_hint = anonymous_tokens::GeoHint{
      .geo_hint = "US,US-CA,MOUNTAIN VIEW",
      .country_code = "US",
      .region = "US-CA",
      .city = "MOUNTAIN VIEW",
  };
  bsa_->set_tokens(
      {{"invalid-token-proto-data", absl::Now() + absl::Hours(1), geo_hint}});

  TryGetAuthTokens(1, ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kFailedBSAOther,
                                       1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

TEST_F(IpProtectionTokenDirectFetcherTest, TokenGeoHintContainsOnlyCountry) {
  GeoHint geo_hint_country;
  geo_hint_country.country_code = "US";
  bsa_->set_tokens(
      {IpProtectionTokenFetcherHelper::CreateBlindSignTokenForTesting(
           "single-use-1", expiration_time_, geo_hint_country),
       IpProtectionTokenFetcherHelper::CreateBlindSignTokenForTesting(
           "single-use-2", expiration_time_, geo_hint_country)});

  TryGetAuthTokens(2, ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(
      IpProtectionTokenFetcherHelper::CreateMockBlindSignedAuthTokenForTesting(
          "single-use-1", expiration_time_, geo_hint_country)
          .value());
  expected.push_back(
      IpProtectionTokenFetcherHelper::CreateMockBlindSignedAuthTokenForTesting(
          "single-use-2", expiration_time_, geo_hint_country)
          .value());
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

TEST_F(IpProtectionTokenDirectFetcherTest, TokenHasMissingGeoHint) {
  GeoHint geo_hint;
  bsa_->set_tokens(
      {IpProtectionTokenFetcherHelper::CreateBlindSignTokenForTesting(
          "single-use-1", expiration_time_, geo_hint)});

  TryGetAuthTokens(1, ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kFailedBSAOther,
                                       1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 400 error.
TEST_F(IpProtectionTokenDirectFetcherTest, BlindSignedTokenError400) {
  bsa_->set_status(absl::InvalidArgumentError("uhoh"));

  TryGetAuthTokens(1, ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_bug_backoff_);
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kFailedBSA400,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensErrorHistogram,
      4043967578,  // base::PersistentHash("INVALID_ARGUMENT: uhoh")
      1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 401 error.
TEST_F(IpProtectionTokenDirectFetcherTest, BlindSignedTokenError401) {
  bsa_->set_status(absl::UnauthenticatedError("uhoh"));

  TryGetAuthTokens(1, ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_bug_backoff_);
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kFailedBSA401,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensErrorHistogram,
      4264091263,  // base::PersistentHash("UNAUTHENTICATED: uhoh")
      1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 403 error.
TEST_F(IpProtectionTokenDirectFetcherTest, BlindSignedTokenError403) {
  bsa_->set_status(absl::PermissionDeniedError("uhoh"));

  TryGetAuthTokens(1, ProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_not_eligible_backoff_);
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kFailedBSA403,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensErrorHistogram,
      4104528123,  // base::PersistentHash("PERMISSION_DENIED: uhoh")
      1);
  // Failed to parse GetInitialDataResponse
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns some other error.
TEST_F(IpProtectionTokenDirectFetcherTest, BlindSignedTokenErrorOther) {
  bsa_->set_status(absl::UnknownError("uhoh"));

  TryGetAuthTokens(1, ProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kFailedBSAOther,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensErrorHistogram,
      2844845398,  // base::PersistentHash("UNKNOWN: uhoh")
      1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// Fetching OAuth token returns a transient error.
TEST_F(IpProtectionTokenDirectFetcherTest, AuthTokenTransientError) {
  delegate_.response_access_token = std::nullopt;
  delegate_.response_result =
      TryGetAuthTokensResult::kFailedOAuthTokenTransient;
  TryGetAuthTokens(1, ProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(default_transient_backoff_);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      TryGetAuthTokensResult::kFailedOAuthTokenTransient, 1);
}

// Fetching OAuth token returns a persistent error.
TEST_F(IpProtectionTokenDirectFetcherTest, AuthTokenPersistentError) {
  delegate_.response_access_token = std::nullopt;
  delegate_.response_result =
      TryGetAuthTokensResult::kFailedOAuthTokenPersistent;

  TryGetAuthTokens(1, ProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      TryGetAuthTokensResult::kFailedOAuthTokenPersistent, 1);
}

// No primary account.
TEST_F(IpProtectionTokenDirectFetcherTest, NoAccount) {
  delegate_.response_access_token = std::nullopt;
  delegate_.response_result = TryGetAuthTokensResult::kFailedNoAccount;

  TryGetAuthTokens(1, ProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(kTryGetAuthTokensResultHistogram,
                                       TryGetAuthTokensResult::kFailedNoAccount,
                                       1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// TryGetAuthTokens() fails because IP Protection is disabled by user settings.
TEST_F(IpProtectionTokenDirectFetcherTest, IpProtectionDisabled) {
  delegate_.is_ip_protection_enabled = false;
  TryGetAuthTokens(1, ProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      TryGetAuthTokensResult::kFailedDisabledByUser, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// Backoff calculations.
TEST_F(IpProtectionTokenDirectFetcherTest, CalculateBackoff) {
  using enum TryGetAuthTokensResult;

  auto check = [&](TryGetAuthTokensResult result,
                   std::optional<base::TimeDelta> backoff, bool exponential) {
    SCOPED_TRACE(::testing::Message()
                 << "result: " << static_cast<int>(result));
    EXPECT_EQ(fetcher_->CalculateBackoff(result), backoff);
    if (backoff && exponential) {
      EXPECT_EQ(fetcher_->CalculateBackoff(result), (*backoff) * 2);
      EXPECT_EQ(fetcher_->CalculateBackoff(result), (*backoff) * 4);
    } else {
      EXPECT_EQ(fetcher_->CalculateBackoff(result), backoff);
    }
  };

  check(kSuccess, std::nullopt, false);
  check(kFailedNotEligible, default_not_eligible_backoff_, false);
  check(kFailedBSA400, default_bug_backoff_, true);
  check(kFailedBSA401, default_bug_backoff_, true);
  check(kFailedBSA403, default_not_eligible_backoff_, false);
  check(kFailedBSAOther, default_transient_backoff_, true);
  check(kFailedOAuthTokenTransient, default_transient_backoff_, true);

  check(kFailedNoAccount, base::TimeDelta::Max(), false);
  // The account-related backoffs should not be changed except by account change
  // events.
  check(kFailedBSA400, base::TimeDelta::Max(), false);
  fetcher_->AccountStatusChanged(true);
  check(kFailedBSA400, default_bug_backoff_, true);
}

}  // namespace ip_protection
