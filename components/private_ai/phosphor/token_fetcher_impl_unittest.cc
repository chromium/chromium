// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/token_fetcher_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "base/time/time.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory_impl.h"
#include "components/private_ai/phosphor/config_http.h"
#include "components/private_ai/phosphor/mock_blind_sign_auth.h"
#include "components/private_ai/phosphor/oauth_token_provider.h"
#include "components/private_ai/phosphor/token_fetcher_helper.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace private_ai::phosphor {
namespace {

// Creates a `quiche::BlindSignToken()` in the format that the BSA library
// will return them.
quiche::BlindSignToken CreateBlindSignTokenForTesting(std::string token_value,
                                                      base::Time expiration) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;

  // The PrivacyPassTokenData values get base64-encoded by BSA, so simulate that
  // here.
  std::string encoded_token_value = base::Base64Encode(token_value);
  std::string encoded_extension_value =
      base::Base64Encode("mock-extension-value");

  privacy_pass_token_data.set_token(std::move(encoded_token_value));
  privacy_pass_token_data.set_encoded_extensions(
      std::move(encoded_extension_value));

  quiche::BlindSignToken blind_sign_token;
  blind_sign_token.token = privacy_pass_token_data.SerializeAsString();
  blind_sign_token.expiration = absl::FromTimeT(expiration.ToTimeT());

  return blind_sign_token;
}

// Converts a mock token value and expiration time into the struct that will
// be passed to the network service.
std::optional<BlindSignedAuthToken> CreateMockBlindSignedAuthTokenForTesting(
    std::string token_value,
    base::Time expiration) {
  quiche::BlindSignToken blind_sign_token =
      CreateBlindSignTokenForTesting(token_value, expiration);
  return TokenFetcherHelper::CreateBlindSignedAuthToken(
      std::move(blind_sign_token));
}
}  // namespace

// A Gmock matcher for a `base::TimeDelta` within the jitter range defined by
// `net::features::kPrivateAiBackoffJitter`.
MATCHER_P(IsNearWithJitter, expected, "") {
  if (arg == base::TimeDelta::Max() && (expected) == base::TimeDelta::Max()) {
    return true;
  }

  const auto jitter = kPrivateAiBackoffJitter.Get();
  const auto lower_bound = (expected) * (1.0 - jitter);
  const auto upper_bound = (expected) * (1.0 + jitter);
  if (arg >= lower_bound && arg <= upper_bound) {
    return true;
  }

  *result_listener << "which is outside the expected range [" << lower_bound
                   << ", " << upper_bound << "]";
  return false;
}

// A mock OAuthTokenProvider for use in testing the fetcher.
struct MockOAuthTokenProvider : public OAuthTokenProvider {
  bool IsTokenFetchEnabled() override { return is_token_fetch_enabled; }
  void RequestOAuthToken(RequestOAuthTokenCallback callback) override {
    std::move(callback).Run(response_result, std::move(response_access_token));
  }

  bool is_token_fetch_enabled = true;
  GetAuthnTokensResult response_result = GetAuthnTokensResult::kSuccess;
  std::optional<std::string> response_access_token = "access_token";
};

class TokenFetcherImplTest : public testing::Test {
 protected:
  using GetAuthnTokensFuture = base::test::TestFuture<
      base::expected<std::vector<BlindSignedAuthToken>, base::Time>>;

  TokenFetcherImplTest()
      : expiration_time_(base::Time::Now() + base::Hours(1)),
        default_transient_backoff_(
            kPrivateAiTryGetAuthTokensTransientBackoff.Get()),
        default_bug_backoff_(kPrivateAiTryGetAuthTokensBugBackoff.Get()),
        default_not_eligible_backoff_(
            kPrivateAiTryGetAuthTokensNotEligibleBackoff.Get()) {
    feature_list_.InitAndEnableFeatureWithParameters(
        kPrivateAi, {{"backoff-jitter", "0.25"}});
    auto bsa = std::make_unique<MockBlindSignAuth>();
    bsa_ = bsa.get();
    fetcher_ = std::make_unique<TokenFetcherImpl>(&oauth_token_provider_,
                                                  std::move(bsa));
  }

  // Call `GetAuthnTokens()` and run until it completes.
  void GetAuthnTokens(int num_tokens) {
    fetcher_->GetAuthnTokens(num_tokens, quiche::ProxyLayer::kTerminalLayer,
                             tokens_future_.GetCallback());

    CHECK(tokens_future_.Wait()) << "GetAuthnTokens did not call back";
  }

  // Expect that the GetAuthnTokens call returned the given tokens.
  void ExpectGetAuthnTokensResult(
      std::vector<BlindSignedAuthToken> bsa_tokens) {
    auto& result = tokens_future_.Get();
    CHECK(result.has_value());
    EXPECT_EQ(result.value(), bsa_tokens);
  }

  // Expect that the GetAuthnTokens call returned an error, with
  // `try_again_after` within the expected range.
  void ExpectGetAuthnTokensResultFailed(base::TimeDelta try_again_delta) {
    auto& result = tokens_future_.Get();
    CHECK(!result.has_value());
    EXPECT_THAT(result.error() - base::Time::Now(),
                IsNearWithJitter(try_again_delta));
    // Clear future so it can be reused and accept new tokens.
    tokens_future_.Clear();
  }

  std::optional<base::TimeDelta> CallCalculateBackoff(
      GetAuthnTokensResult result) {
    return fetcher_->CalculateBackoff(result);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  GetAuthnTokensFuture tokens_future_;

  base::test::TracingEnvironment tracing_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  base::Time expiration_time_;

  // Mock providers for the fetcher under test.
  MockOAuthTokenProvider oauth_token_provider_;

  // Fetcher under test.
  std::unique_ptr<TokenFetcherImpl> fetcher_;

  // quiche::BlindSignAuthInterface owned and used by the fetcher.
  raw_ptr<MockBlindSignAuth> bsa_;

  // Default backoff times applied for calculating `try_again_after`.
  base::TimeDelta default_transient_backoff_;
  base::TimeDelta default_bug_backoff_;
  base::TimeDelta default_not_eligible_backoff_;
};

TEST_F(TokenFetcherImplTest, Success) {
  bsa_->set_tokens(
      {CreateBlindSignTokenForTesting("single-use-1", expiration_time_),
       CreateBlindSignTokenForTesting("single-use-2", expiration_time_)});

  GetAuthnTokens(2);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->service_type(),
            quiche::BlindSignAuthServiceType::kChromePrivateAratea);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(
      CreateMockBlindSignedAuthTokenForTesting("single-use-1", expiration_time_)
          .value());
  expected.push_back(
      CreateMockBlindSignedAuthTokenForTesting("single-use-2", expiration_time_)
          .value());
  ExpectGetAuthnTokensResult(std::move(expected));

  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.Phosphor.TokenFetcher.GetAuthnTokens.Result",
      GetAuthnTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.Phosphor.TokenFetcher.OAuthTokenFetchLatency", 1);
}

// GetAuthnTokens() is called with a non-positive batch size.
TEST_F(TokenFetcherImplTest, NonPositiveBatchSize) {
  GetAuthnTokens(0);
  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(default_bug_backoff_);
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.Phosphor.TokenFetcher.GetAuthnTokens.Result",
      GetAuthnTokensResult::kFailedBSA400, 1);

  GetAuthnTokens(-1);
  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(default_bug_backoff_ * 2);
  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.Phosphor.TokenFetcher.GetAuthnTokens.Result",
      GetAuthnTokensResult::kFailedBSA400, 2);
}

// BSA returns no tokens.
TEST_F(TokenFetcherImplTest, NoTokens) {
  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);

  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.Phosphor.TokenFetcher.GetAuthnTokens.Result",
      GetAuthnTokensResult::kFailedBSAOther, 1);
}

// BSA returns malformed tokens.
TEST_F(TokenFetcherImplTest, MalformedTokens) {
  bsa_->set_tokens({{"invalid-token-proto-data",
                     absl::FromTimeT(expiration_time_.ToTimeT()),
                     {}}});

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// BSA returns a token with malformed extensions.
TEST_F(TokenFetcherImplTest, MalformedTokenExtensions) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
  privacy_pass_token_data.set_token("some_token");
  privacy_pass_token_data.set_encoded_extensions("invalid-chars-%$#");

  quiche::BlindSignToken bsa_token;
  bsa_token.token = privacy_pass_token_data.SerializeAsString();
  bsa_token.expiration = absl::FromTimeT(expiration_time_.ToTimeT());

  bsa_->set_tokens({bsa_token});

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// BSA returns a token with an empty token value.
TEST_F(TokenFetcherImplTest, MalformedTokenEmptyTokenValue) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
  privacy_pass_token_data.set_encoded_extensions("some-extensions");

  quiche::BlindSignToken bsa_token;
  bsa_token.token = privacy_pass_token_data.SerializeAsString();
  bsa_token.expiration = absl::FromTimeT(expiration_time_.ToTimeT());

  bsa_->set_tokens({bsa_token});

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// BSA returns a token with an empty extensions value.
TEST_F(TokenFetcherImplTest, MalformedTokenEmptyExtensionsValue) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
  privacy_pass_token_data.set_token("some-token");

  quiche::BlindSignToken bsa_token;
  bsa_token.token = privacy_pass_token_data.SerializeAsString();
  bsa_token.expiration = absl::FromTimeT(expiration_time_.ToTimeT());

  bsa_->set_tokens({bsa_token});

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// BSA returns a 400 error.
TEST_F(TokenFetcherImplTest, BlindSignedTokenError400) {
  bsa_->set_status(absl::InvalidArgumentError("uhoh"));

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_bug_backoff_);

  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.Phosphor.TokenFetcher.GetAuthnTokens.Result",
      GetAuthnTokensResult::kFailedBSA400, 1);
}

// BSA returns a 401 error.
TEST_F(TokenFetcherImplTest, BlindSignedTokenError401) {
  bsa_->set_status(absl::UnauthenticatedError("uhoh"));

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_bug_backoff_);
}

// BSA returns a 403 error.
TEST_F(TokenFetcherImplTest, BlindSignedTokenError403) {
  bsa_->set_status(absl::PermissionDeniedError("uhoh"));

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_not_eligible_backoff_);
}

// BSA returns some other error.
TEST_F(TokenFetcherImplTest, BlindSignedTokenErrorOther) {
  bsa_->set_status(absl::UnknownError("uhoh"));

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kTerminalLayer);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// Fetching OAuth token returns a transient error.
TEST_F(TokenFetcherImplTest, AuthTokenTransientError) {
  oauth_token_provider_.response_access_token = std::nullopt;
  oauth_token_provider_.response_result =
      GetAuthnTokensResult::kFailedOAuthTokenTransient;
  GetAuthnTokens(1);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);

  histogram_tester_.ExpectUniqueSample(
      "PrivateAi.Phosphor.TokenFetcher.GetAuthnTokens.Result",
      GetAuthnTokensResult::kFailedOAuthTokenTransient, 1);
  histogram_tester_.ExpectTotalCount(
      "PrivateAi.Phosphor.TokenFetcher.OAuthTokenFetchLatency", 1);
}

// Fetching OAuth token returns a persistent error.
TEST_F(TokenFetcherImplTest, AuthTokenPersistentError) {
  oauth_token_provider_.response_access_token = std::nullopt;
  oauth_token_provider_.response_result =
      GetAuthnTokensResult::kFailedOAuthTokenPersistent;

  GetAuthnTokens(1);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(base::TimeDelta::Max());
}

// No primary account.
TEST_F(TokenFetcherImplTest, NoAccount) {
  oauth_token_provider_.response_access_token = std::nullopt;
  oauth_token_provider_.response_result =
      GetAuthnTokensResult::kFailedNoAccount;

  GetAuthnTokens(1);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(base::TimeDelta::Max());
}

// Backoff calculations.
TEST_F(TokenFetcherImplTest, CalculateBackoff) {
  using enum GetAuthnTokensResult;

  // Check that the backoff is as expected, and that it doubles on subsequent
  // calls if the result is exponential.
  auto check_fn = [&](GetAuthnTokensResult result,
                      std::optional<base::TimeDelta> backoff,
                      bool exponential) {
    SCOPED_TRACE(::testing::Message()
                 << "result: " << static_cast<int>(result));
    if (backoff) {
      EXPECT_THAT(CallCalculateBackoff(result),
                  testing::Optional(IsNearWithJitter(*backoff)));
    } else {
      EXPECT_EQ(CallCalculateBackoff(result), std::nullopt);
    }

    if (backoff && exponential) {
      EXPECT_THAT(CallCalculateBackoff(result),
                  testing::Optional(IsNearWithJitter(*backoff * 2)));
      EXPECT_THAT(CallCalculateBackoff(result),
                  testing::Optional(IsNearWithJitter(*backoff * 4)));
    } else {
      if (backoff) {
        EXPECT_THAT(CallCalculateBackoff(result),
                    testing::Optional(IsNearWithJitter(*backoff)));
      } else {
        EXPECT_EQ(CallCalculateBackoff(result), std::nullopt);
      }
    }
  };

  check_fn(kSuccess, std::nullopt, false);
  check_fn(kFailedNotEligible, default_not_eligible_backoff_, false);
  check_fn(kFailedBSA400, default_bug_backoff_, true);
  check_fn(kFailedBSA401, default_bug_backoff_, true);
  check_fn(kFailedBSA403, default_not_eligible_backoff_, false);
  check_fn(kFailedBSAOther, default_transient_backoff_, true);
  check_fn(kFailedOAuthTokenTransient, default_transient_backoff_, true);

  check_fn(kFailedNoAccount, base::TimeDelta::Max(), false);
  // The account-related backoffs should not be changed except by account change
  // events.
  check_fn(kFailedBSA400, base::TimeDelta::Max(), false);
  fetcher_->AccountStatusChanged(true);
  check_fn(kFailedBSA400, default_bug_backoff_, true);
}

// Backoff calculations with jitter disabled.
TEST_F(TokenFetcherImplTest, CalculateBackoffNoJitter) {
  // Disable jitter.
  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(kPrivateAi,
                                                   {{"backoff-jitter", "0.0"}});

  using enum GetAuthnTokensResult;

  // Check that the backoff is as expected, and that it doubles on subsequent
  // calls if the result is exponential.
  // We check that it perfectly matches the expected backoff, with no jitter.
  auto check_fn = [&](GetAuthnTokensResult result,
                      std::optional<base::TimeDelta> backoff,
                      bool exponential) {
    SCOPED_TRACE(::testing::Message()
                 << "result: " << static_cast<int>(result));
    if (backoff) {
      EXPECT_THAT(CallCalculateBackoff(result),
                  testing::Optional(testing::Eq(*backoff)));
    } else {
      EXPECT_EQ(CallCalculateBackoff(result), std::nullopt);
    }

    if (backoff && exponential) {
      EXPECT_THAT(CallCalculateBackoff(result),
                  testing::Optional(testing::Eq(*backoff * 2)));
      EXPECT_THAT(CallCalculateBackoff(result),
                  testing::Optional(testing::Eq(*backoff * 4)));
    } else {
      if (backoff) {
        EXPECT_THAT(CallCalculateBackoff(result),
                    testing::Optional(testing::Eq(*backoff)));
      } else {
        EXPECT_EQ(CallCalculateBackoff(result), std::nullopt);
      }
    }
  };

  check_fn(kSuccess, std::nullopt, false);
  check_fn(kFailedNotEligible, default_not_eligible_backoff_, false);
  check_fn(kFailedBSA400, default_bug_backoff_, true);
  check_fn(kFailedBSA401, default_bug_backoff_, true);
  check_fn(kFailedBSA403, default_not_eligible_backoff_, false);
  check_fn(kFailedBSAOther, default_transient_backoff_, true);
  check_fn(kFailedOAuthTokenTransient, default_transient_backoff_, true);

  check_fn(kFailedNoAccount, base::TimeDelta::Max(), false);
  // The account-related backoffs should not be changed except by account change
  // events.
  check_fn(kFailedBSA400, base::TimeDelta::Max(), false);
  fetcher_->AccountStatusChanged(true);
  check_fn(kFailedBSA400, default_bug_backoff_, true);
}

class ProdBlindSignAuthTokenFetcherImplTest : public testing::Test {
 public:
  ProdBlindSignAuthTokenFetcherImplTest() = default;
  ~ProdBlindSignAuthTokenFetcherImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;
  MockOAuthTokenProvider oauth_token_provider_;
};

// Tests that TokenFetcherImpl fails gracefully when the blind sign auth token
// fetch fails. This is an integration test of TokenFetcherImpl,
// BlindSignAuthFactoryImpl, and ConfigHttp.
TEST_F(ProdBlindSignAuthTokenFetcherImplTest, FetchFails) {
  BlindSignAuthFactoryImpl bsa_factory;

  auto bsa = bsa_factory.CreateBlindSignAuth(
      test_url_loader_factory_.GetSafeWeakWrapper()->Clone());

  TokenFetcherImpl fetcher(&oauth_token_provider_, std::move(bsa));

  // Set up BSA to fail.
  GURL::Replacements replacements;
  const std::string path = ConfigHttp::GetInitialDataPath();
  replacements.SetPathStr(path);
  GURL expected_url =
      ConfigHttp::GetServerUrl().ReplaceComponents(replacements);
  test_url_loader_factory_.AddResponse(
      expected_url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_FAILED));

  base::test::TestFuture<
      base::expected<std::vector<BlindSignedAuthToken>, base::Time>>
      tokens_future;
  fetcher.GetAuthnTokens(1, quiche::ProxyLayer::kTerminalLayer,
                         tokens_future.GetCallback());

  ASSERT_TRUE(tokens_future.Wait());
  auto& result = tokens_future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_THAT(
      result.error() - base::Time::Now(),
      IsNearWithJitter(kPrivateAiTryGetAuthTokensTransientBackoff.Get()));
}

}  // namespace private_ai::phosphor
