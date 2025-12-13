// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/phosphor/token_fetcher_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "base/time/time.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/mock_blind_sign_auth.h"
#include "components/legion/phosphor/token_fetcher_helper.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace legion::phosphor {
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
// `net::features::kLegionBackoffJitter`.
MATCHER_P(IsNearWithJitter, expected, "") {
  if (arg == base::TimeDelta::Max() && (expected) == base::TimeDelta::Max()) {
    return true;
  }

  const auto jitter = legion::kLegionBackoffJitter.Get();
  const auto lower_bound = (expected) * (1.0 - jitter);
  const auto upper_bound = (expected) * (1.0 + jitter);
  if (arg >= lower_bound && arg <= upper_bound) {
    return true;
  }

  *result_listener << "which is outside the expected range [" << lower_bound
                   << ", " << upper_bound << "]";
  return false;
}

// A mock delegate for use in testing the fetcher.
struct MockTokenFetcherImplDelegate : public TokenFetcherImpl::Delegate {
  bool IsTokenFetchEnabled() override { return is_token_fetch_enabled; }
  void RequestOAuthToken(RequestOAuthTokenCallback callback) override {
    std::move(callback).Run(response_result, std::move(response_access_token));
  }
  std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override {
    return std::move(bsa_to_return);
  }

  bool is_token_fetch_enabled = true;
  GetAuthnTokensResult response_result = GetAuthnTokensResult::kSuccess;
  std::optional<std::string> response_access_token = "access_token";
  std::unique_ptr<quiche::BlindSignAuthInterface> bsa_to_return;
};

class TokenFetcherImplTest : public testing::Test {
 protected:
  using GetAuthnTokensFuture = base::test::TestFuture<
      const std::optional<std::vector<BlindSignedAuthToken>>,
      std::optional<base::Time>>;

  TokenFetcherImplTest()
      : expiration_time_(base::Time::Now() + base::Hours(1)),
        default_transient_backoff_(
            legion::kLegionTryGetAuthTokensTransientBackoff.Get()),
        default_bug_backoff_(legion::kLegionTryGetAuthTokensBugBackoff.Get()),
        default_not_eligible_backoff_(
            legion::kLegionTryGetAuthTokensNotEligibleBackoff.Get()) {
    feature_list_.InitAndEnableFeatureWithParameters(
        legion::kLegion, {{"LegionBackoffJitter", "0.25"}});
    auto bsa = std::make_unique<MockBlindSignAuth>();
    bsa_ = bsa.get();
    delegate_.bsa_to_return = std::move(bsa);
    fetcher_ = std::make_unique<TokenFetcherImpl>(
        &delegate_, test_url_loader_factory_.GetSafeWeakWrapper()->Clone());
  }

  // Call `GetAuthnTokens()` and run until it completes.
  void GetAuthnTokens(int num_tokens) {
    fetcher_->GetAuthnTokens(num_tokens, tokens_future_.GetCallback());

    ASSERT_TRUE(tokens_future_.Wait()) << "GetAuthnTokens did not call back";
  }

  // Expect that the GetAuthnTokens call returned the given tokens.
  void ExpectGetAuthnTokensResult(
      std::vector<BlindSignedAuthToken> bsa_tokens) {
    EXPECT_EQ(std::get<0>(tokens_future_.Get()), bsa_tokens);
  }

  // Expect that the GetAuthnTokens call returned nullopt, with
  // `try_again_after` within the expected range.
  void ExpectGetAuthnTokensResultFailed(base::TimeDelta try_again_delta) {
    auto& [bsa_tokens, try_again_after] = tokens_future_.Get();
    EXPECT_EQ(bsa_tokens, std::nullopt);
    if (!bsa_tokens) {
      EXPECT_THAT(*try_again_after - base::Time::Now(),
                  IsNearWithJitter(try_again_delta));
    }
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
  GetAuthnTokensFuture tokens_future_;

  base::test::TracingEnvironment tracing_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  base::Time expiration_time_;

  // Delegate for the fetcher under test.
  MockTokenFetcherImplDelegate delegate_;

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
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  std::vector<BlindSignedAuthToken> expected;
  expected.push_back(
      CreateMockBlindSignedAuthTokenForTesting("single-use-1", expiration_time_)
          .value());
  expected.push_back(
      CreateMockBlindSignedAuthTokenForTesting("single-use-2", expiration_time_)
          .value());
  ExpectGetAuthnTokensResult(std::move(expected));
}

// GetAuthnTokens() is called with a non-positive batch size.
TEST_F(TokenFetcherImplTest, NonPositiveBatchSize) {
  GetAuthnTokens(0);
  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(default_bug_backoff_);

  GetAuthnTokens(-1);
  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(default_bug_backoff_ * 2);
}

// BSA returns no tokens.
TEST_F(TokenFetcherImplTest, NoTokens) {
  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// BSA returns malformed tokens.
TEST_F(TokenFetcherImplTest, MalformedTokens) {
  bsa_->set_tokens({{"invalid-token-proto-data",
                     absl::FromTimeT(expiration_time_.ToTimeT()),
                     {}}});

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
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
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
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
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
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
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// BSA returns a 400 error.
TEST_F(TokenFetcherImplTest, BlindSignedTokenError400) {
  bsa_->set_status(absl::InvalidArgumentError("uhoh"));

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_bug_backoff_);
}

// BSA returns a 401 error.
TEST_F(TokenFetcherImplTest, BlindSignedTokenError401) {
  bsa_->set_status(absl::UnauthenticatedError("uhoh"));

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_bug_backoff_);
}

// BSA returns a 403 error.
TEST_F(TokenFetcherImplTest, BlindSignedTokenError403) {
  bsa_->set_status(absl::PermissionDeniedError("uhoh"));

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_not_eligible_backoff_);
}

// BSA returns some other error.
TEST_F(TokenFetcherImplTest, BlindSignedTokenErrorOther) {
  bsa_->set_status(absl::UnknownError("uhoh"));

  GetAuthnTokens(1);

  EXPECT_TRUE(bsa_->GetTokensCalledInDifferentThread());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), "access_token");
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// Fetching OAuth token returns a transient error.
TEST_F(TokenFetcherImplTest, AuthTokenTransientError) {
  delegate_.response_access_token = std::nullopt;
  delegate_.response_result = GetAuthnTokensResult::kFailedOAuthTokenTransient;
  GetAuthnTokens(1);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(default_transient_backoff_);
}

// Fetching OAuth token returns a persistent error.
TEST_F(TokenFetcherImplTest, AuthTokenPersistentError) {
  delegate_.response_access_token = std::nullopt;
  delegate_.response_result = GetAuthnTokensResult::kFailedOAuthTokenPersistent;

  GetAuthnTokens(1);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectGetAuthnTokensResultFailed(base::TimeDelta::Max());
}

// No primary account.
TEST_F(TokenFetcherImplTest, NoAccount) {
  delegate_.response_access_token = std::nullopt;
  delegate_.response_result = GetAuthnTokensResult::kFailedNoAccount;

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
  feature_list_.InitAndEnableFeatureWithParameters(
      legion::kLegion, {{"LegionBackoffJitter", "0.0"}});

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

}  // namespace legion::phosphor
