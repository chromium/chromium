// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/token_manager_impl.h"

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

namespace private_ai::phosphor {

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

  std::unique_ptr<TokenManagerImpl> token_manager_;
  raw_ptr<MockTokenFetcher> mock_fetcher_;
};

TEST_F(TokenManagerImplTest, GetAuthToken) {
  // Request a token. This should trigger a fetch for a batch of tokens.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kTerminalLayer,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  {
    base::test::TestFuture<std::optional<BlindSignedAuthToken>> future;
    token_manager_->GetAuthToken(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
    EXPECT_TRUE(future.Get().has_value());
  }

  // The rest of the batch should be available from the cache. A prefetch will
  // be triggered when the cache runs low.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kTerminalLayer,
      TokenBatch(expected_batch_size_, kFutureExpiration));
  for (int i = 0; i < expected_batch_size_ - 1; ++i) {
    base::test::TestFuture<std::optional<BlindSignedAuthToken>> future;
    token_manager_->GetAuthToken(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    ASSERT_TRUE(future.Get().has_value());
  }

  ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());

  // A token should be available from the new batch.
  {
    base::test::TestFuture<std::optional<BlindSignedAuthToken>> future;
    token_manager_->GetAuthToken(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    ASSERT_TRUE(future.Get().has_value());
  }
}

TEST_F(TokenManagerImplTest, GetAuthTokenForProxy) {
  // Request a token. This should trigger a fetch for a batch of tokens.
  mock_fetcher_->ExpectGetAuthnTokensCall(
      expected_batch_size_, quiche::ProxyLayer::kProxyB,
      TokenBatch(expected_batch_size_, kFutureExpiration));

  {
    base::test::TestFuture<std::optional<BlindSignedAuthToken>> future;
    token_manager_->GetAuthTokenForProxy(future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    ASSERT_TRUE(mock_fetcher_->GotAllExpectedMockCalls());
    EXPECT_TRUE(future.Get().has_value());
  }
}

}  // namespace
}  // namespace private_ai::phosphor
