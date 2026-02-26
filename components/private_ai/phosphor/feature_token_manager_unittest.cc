// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/feature_token_manager.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/token_fetcher.h"
#include "net/base/features.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai::phosphor::internal {

namespace {

struct ExpectedGetAuthnTokensCall {
  // The expected batch_size argument for the call.
  int batch_size;
  // The expected proxy_layer argument for the call.
  quiche::ProxyLayer proxy_layer;
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
                                quiche::ProxyLayer proxy_layer,
                                std::vector<BlindSignedAuthToken> bsa_tokens) {
    expected_get_authn_token_calls_.emplace_back(ExpectedGetAuthnTokensCall{
        .batch_size = batch_size,
        .proxy_layer = proxy_layer,
        .bsa_tokens = std::move(bsa_tokens),
        .try_again_after = std::nullopt,
    });
  }

  // Register an expectation of a call to `GetAuthnTokens()` returning no
  // tokens and the given `try_again_after`.
  void ExpectGetAuthnTokensCall(int batch_size,
                                quiche::ProxyLayer proxy_layer,
                                base::Time try_again_after) {
    expected_get_authn_token_calls_.emplace_back(ExpectedGetAuthnTokensCall{
        .batch_size = batch_size,
        .proxy_layer = proxy_layer,
        .bsa_tokens = std::nullopt,
        .try_again_after = try_again_after,
    });
  }

  // True if all expected `GetAuthnTokens` calls have occurred.
  bool GotAllExpectedMockCalls() {
    return expected_get_authn_token_calls_.empty();
  }

  void GetAuthnTokens(int batch_size,
                      quiche::ProxyLayer proxy_layer,
                      GetAuthnTokensCallback callback) override {
    CHECK(!expected_get_authn_token_calls_.empty())
        << "Unexpected call to GetAuthnTokens";
    auto exp = std::move(expected_get_authn_token_calls_.front());
    expected_get_authn_token_calls_.pop_front();
    EXPECT_EQ(batch_size, exp.batch_size);
    EXPECT_EQ(proxy_layer, exp.proxy_layer);

    base::expected<std::vector<BlindSignedAuthToken>, base::Time> result;
    if (exp.bsa_tokens) {
      result = base::ok(std::move(*exp.bsa_tokens));
    } else {
      result = base::unexpected(*exp.try_again_after);
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
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
        mock_fetcher_, quiche::ProxyLayer::kTerminalLayer,
        kPrivateAiAuthTokenCacheBatchSize.Get(),
        kPrivateAiAuthTokenCacheLowWaterMark.Get());
  }

  // Create a batch of tokens.
  std::vector<BlindSignedAuthToken> TokenBatch(int count,
                                               base::Time expiration) {
    std::vector<BlindSignedAuthToken> tokens;
    for (int i = 0; i < count; i++) {
      tokens.emplace_back(
          BlindSignedAuthToken{.token = "token-" + base::NumberToString(i),
                               .encoded_extensions = "ext",
                               .expiration = expiration});
    }
    return tokens;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  int expected_batch_size_ = kPrivateAiAuthTokenCacheBatchSize.Get();

  // Expiration times with respect to the TaskEnvironment's mock time.
  const base::Time kFutureExpiration = base::Time::Now() + base::Hours(1);

  std::unique_ptr<MockTokenFetcher> owned_mock_fetcher_;
  raw_ptr<MockTokenFetcher, DanglingUntriaged> mock_fetcher_;
  std::unique_ptr<FeatureTokenManager> feature_token_manager_;
};

TEST_F(FeatureTokenManagerTest, GetAuthToken) {
  base::HistogramTester histogram_tester;
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kTerminalLayer,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  // The first call to `GetAuthToken` will be asynchronous.
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future;
  feature_token_manager_->GetAuthToken(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // The future should have completed with a token.
  EXPECT_TRUE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrivateAi.Phosphor.FeatureTokenManager.TokensFetched",
      expected_batch_size_, 1);

  histogram_tester.ExpectBucketCount(
      "PrivateAi.Phosphor.FeatureTokenManager.ServedFromCache", false, 1);

  // The second call should succeed asynchronously from the cache.
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future2;
  feature_token_manager_->GetAuthToken(future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
  EXPECT_TRUE(future2.Get().has_value());

  histogram_tester.ExpectBucketCount(
      "PrivateAi.Phosphor.FeatureTokenManager.ServedFromCache", true, 1);
}

TEST_F(FeatureTokenManagerTest, OnGotAuthTokens_FewerTokensThanCallbacks) {
  // The first fetch will return only 2 tokens.
  mock_fetcher_->ExpectGetAuthnTokensCall(expected_batch_size_,
                                          quiche::ProxyLayer::kTerminalLayer,
                                          TokenBatch(2, kFutureExpiration));
  // The pending callback will trigger another fetch.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kTerminalLayer,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  // Queue 3 token requests.
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future1;
  feature_token_manager_->GetAuthToken(future1.GetCallback());
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future2;
  feature_token_manager_->GetAuthToken(future2.GetCallback());
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future3;
  feature_token_manager_->GetAuthToken(future3.GetCallback());

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // All 3 futures should have completed with a token.
  EXPECT_TRUE(future1.IsReady());
  EXPECT_TRUE(future1.Get().has_value());
  EXPECT_TRUE(future2.IsReady());
  EXPECT_TRUE(future2.Get().has_value());
  EXPECT_TRUE(future3.IsReady());
  EXPECT_TRUE(future3.Get().has_value());
}

TEST_F(FeatureTokenManagerTest, PrefetchAuthTokens) {
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kTerminalLayer,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  feature_token_manager_->PrefetchAuthTokens();
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // A subsequent call to GetAuthToken should succeed asynchronously from the
  // cache.
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future;
  feature_token_manager_->GetAuthToken(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Get().has_value());
}

TEST_F(FeatureTokenManagerTest, ExpiredToken) {
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kTerminalLayer,
      TokenBatch(expected_batch_size_, base::Time::Now() + base::Seconds(1)));

  // This will trigger the first fetch.
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future;
  feature_token_manager_->GetAuthToken(future.GetCallback());
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_TRUE(future.Get().has_value());

  // A token should be available.
  // Another fetch should be triggered automatically when the token expires.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kTerminalLayer,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  // Advance time so the token expires and the timer for refill fires.
  task_environment_.FastForwardBy(base::Seconds(2));
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // Now a token should be available.
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future2;
  feature_token_manager_->GetAuthToken(future2.GetCallback());
  EXPECT_FALSE(future2.IsReady());
  EXPECT_TRUE(future2.Get().has_value());
}

TEST_F(FeatureTokenManagerTest, FetchError_BacksOff) {
  base::HistogramTester histogram_tester;
  base::Time try_again_after = base::Time::Now() + base::Seconds(10);
  mock_fetcher_->ExpectGetAuthnTokensCall(expected_batch_size_,
                                          quiche::ProxyLayer::kTerminalLayer,
                                          try_again_after);

  // This will trigger the first fetch, which will fail.
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future;
  feature_token_manager_->GetAuthToken(future.GetCallback());
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_FALSE(future.Get().has_value());

  histogram_tester.ExpectUniqueSample(
      "PrivateAi.Phosphor.FeatureTokenManager.TokensFetched", 0, 1);

  // No token should be available.
  // Calling GetAuthToken again should not trigger a new fetch due to backoff.
  base::test::TestFuture<std::optional<BlindSignedAuthToken>> future2;
  feature_token_manager_->GetAuthToken(future2.GetCallback());
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
  EXPECT_FALSE(future2.IsReady());

  // Expect a new fetch to be triggered automatically after the backoff period.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kTerminalLayer,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  // Advance time past the backoff period.
  task_environment_.FastForwardBy(base::Seconds(11));
  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  EXPECT_TRUE(future2.Get().has_value());
}

}  // namespace
}  // namespace private_ai::phosphor::internal
