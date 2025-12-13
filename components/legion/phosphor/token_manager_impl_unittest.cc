// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/phosphor/token_manager_impl.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/token_fetcher.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace legion::phosphor {

namespace {

struct ExpectedGetAuthnTokensCall {
  // The expected batch_size argument for the call.
  int batch_size;
  // The response to the call.
  std::optional<std::vector<BlindSignedAuthToken>> bsa_tokens;
  std::optional<base::Time> try_again_after;
};

class MockTokenFetcher : public TokenFetcher {
 public:
  ~MockTokenFetcher() override = default;

  // Register an expectation of a call to `GetAuthnTokens()` returning the
  // given tokens.
  void ExpectGetAuthnTokensCall(int batch_size,
                                std::vector<BlindSignedAuthToken> bsa_tokens) {
    expected_get_authn_token_calls_.emplace_back(ExpectedGetAuthnTokensCall{
        .batch_size = batch_size,
        .bsa_tokens = std::move(bsa_tokens),
        .try_again_after = std::nullopt,
    });
  }

  // Register an expectation of a call to `GetAuthnTokens()` returning no
  // tokens and the given `try_again_after`.
  void ExpectGetAuthnTokensCall(int batch_size, base::Time try_again_after) {
    expected_get_authn_token_calls_.emplace_back(ExpectedGetAuthnTokensCall{
        .batch_size = batch_size,
        .bsa_tokens = std::nullopt,
        .try_again_after = try_again_after,
    });
  }

  // True if all expected `GetAuthnTokens` calls have occurred.
  bool GotAllExpectedMockCalls() {
    return expected_get_authn_token_calls_.empty();
  }

  void GetAuthnTokens(int batch_size,
                      GetAuthnTokensCallback callback) override {
    ASSERT_FALSE(expected_get_authn_token_calls_.empty())
        << "Unexpected call to GetAuthnTokens";
    auto& exp = expected_get_authn_token_calls_.front();
    EXPECT_EQ(batch_size, exp.batch_size);
    std::move(callback).Run(std::move(exp.bsa_tokens), exp.try_again_after);
    expected_get_authn_token_calls_.pop_front();
  }

 protected:
  std::deque<ExpectedGetAuthnTokensCall> expected_get_authn_token_calls_;
};

class TokenManagerImplTest : public testing::Test {
 protected:
  TokenManagerImplTest() {
    auto mock_fetcher = std::make_unique<MockTokenFetcher>();
    mock_fetcher_ = mock_fetcher.get();
    token_manager_ =
        std::make_unique<TokenManagerImpl>(std::move(mock_fetcher));
  }

  // Create a batch of tokens.
  std::vector<BlindSignedAuthToken> TokenBatch(int count,
                                               base::Time expiration) {
    std::vector<BlindSignedAuthToken> tokens;
    for (int i = 0; i < count; i++) {
      tokens.emplace_back(
          BlindSignedAuthToken{.token = "token-" + base::NumberToString(i),
                               .expiration = expiration});
    }
    return tokens;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  int expected_batch_size_ = legion::kLegionAuthTokenCacheBatchSize.Get();

  // Expiration times with respect to the TaskEnvironment's mock time.
  const base::Time kFutureExpiration = base::Time::Now() + base::Hours(1);

  std::unique_ptr<TokenManagerImpl> token_manager_;
  raw_ptr<MockTokenFetcher> mock_fetcher_;
};

TEST_F(TokenManagerImplTest, MultipleFeatures) {
  // Request a token for feature 1.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));
  EXPECT_FALSE(token_manager_->GetAuthToken(
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(token_manager_->IsAuthTokenAvailable(
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT));

  // Request a token for feature 2.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));
  EXPECT_FALSE(token_manager_->GetAuthToken(
      proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(token_manager_->IsAuthTokenAvailable(
      proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION));

  // Tokens should be available for both.
  EXPECT_TRUE(token_manager_->IsAuthTokenAvailable(
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT));
  EXPECT_TRUE(token_manager_->IsAuthTokenAvailable(
      proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION));

  // Take a token for feature 1.
  auto token1 = token_manager_->GetAuthToken(
      proto::FeatureName::FEATURE_NAME_DEMO_GEMINI_GENERATE_CONTENT);
  ASSERT_TRUE(token1.has_value());

  // A token should still be available for feature 2.
  EXPECT_TRUE(token_manager_->IsAuthTokenAvailable(
      proto::FeatureName::FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION));
}

}  // namespace
}  // namespace legion::phosphor
