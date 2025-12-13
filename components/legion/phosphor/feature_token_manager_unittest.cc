// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/phosphor/feature_token_manager.h"

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

namespace legion::phosphor::internal {

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

class FeatureTokenManagerTest : public testing::Test {
 protected:
  FeatureTokenManagerTest() {
    owned_mock_fetcher_ = std::make_unique<MockTokenFetcher>();
    mock_fetcher_ = owned_mock_fetcher_.get();
    feature_token_manager_ = std::make_unique<FeatureTokenManager>(
        mock_fetcher_, legion::kLegionAuthTokenCacheBatchSize.Get(),
        legion::kLegionAuthTokenCacheLowWaterMark.Get());
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

  std::unique_ptr<MockTokenFetcher> owned_mock_fetcher_;
  raw_ptr<MockTokenFetcher, DanglingUntriaged> mock_fetcher_;
  std::unique_ptr<FeatureTokenManager> feature_token_manager_;
};

TEST_F(FeatureTokenManagerTest, GetAuthToken) {
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  // The first call to `GetAuthToken` will return nullopt and trigger a token
  // fetch.
  EXPECT_FALSE(feature_token_manager_->GetAuthToken());

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(feature_token_manager_->IsAuthTokenAvailable());

  // The second call should succeed.
  auto token = feature_token_manager_->GetAuthToken();
  ASSERT_TRUE(token.has_value());
}

TEST_F(FeatureTokenManagerTest, ExpiredToken) {
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, base::Time::Now() + base::Seconds(1)));

  // This will trigger the first fetch.
  EXPECT_FALSE(feature_token_manager_->GetAuthToken());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // A token should be available.
  EXPECT_TRUE(feature_token_manager_->IsAuthTokenAvailable());

  // Another fetch should be triggered automatically when the token expires.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  // Advance time so the token expires and the timer for refill fires.
  task_environment_.FastForwardBy(base::Seconds(2));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // Now a token should be available.
  EXPECT_TRUE(feature_token_manager_->IsAuthTokenAvailable());
}

TEST_F(FeatureTokenManagerTest, FetchError_BacksOff) {
  base::Time try_again_after = base::Time::Now() + base::Seconds(10);
  mock_fetcher_->ExpectGetAuthnTokensCall(expected_batch_size_,
                                          try_again_after);

  // This will trigger the first fetch, which will fail.
  EXPECT_FALSE(feature_token_manager_->GetAuthToken());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // No token should be available.
  EXPECT_FALSE(feature_token_manager_->IsAuthTokenAvailable());

  // Calling GetAuthToken again should not trigger a new fetch due to backoff.
  EXPECT_FALSE(feature_token_manager_->GetAuthToken());
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // Expect a new fetch to be triggered automatically after the backoff period.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  // Advance time past the backoff period.
  task_environment_.FastForwardBy(base::Seconds(11));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(feature_token_manager_->IsAuthTokenAvailable());
}

}  // namespace
}  // namespace legion::phosphor::internal
