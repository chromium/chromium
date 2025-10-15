// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_cache.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {
// Time after which tokens expire from the cache.
constexpr base::TimeDelta kMaxAge = base::Seconds(10);
}  // namespace

class OneTimeTokenCacheTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  OneTimeTokenCache cache_{kMaxAge};
};

// Ensure that a new token can be added.
TEST_F(OneTimeTokenCacheTest, PurgeExpiredAndAdd_AddNewToken) {
  OneTimeToken token(OneTimeTokenType::kSmsOtp, "token1", base::Time::Now());
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(token));
  const auto& tokens = cache_.PurgeExpiredAndGetCache();
  ASSERT_EQ(1u, tokens.size());
  EXPECT_EQ(token, tokens.front());
}

// Ensure that a token is not added a second time.
TEST_F(OneTimeTokenCacheTest, PurgeExpiredAndAdd_AddExistingToken) {
  base::Time now = base::Time::Now();
  OneTimeToken token(OneTimeTokenType::kSmsOtp, "token1", now);
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(token));
  EXPECT_FALSE(cache_.PurgeExpiredAndAdd(token));
  const auto& tokens = cache_.PurgeExpiredAndGetCache();
  EXPECT_EQ(1u, tokens.size());
}

// Ensure that a token is not added a second time, if everything but the
// timestamp exists in the cache already.
TEST_F(OneTimeTokenCacheTest,
       PurgeExpiredAndAdd_AddExistingTokenWithDifferentTimestamp) {
  base::Time first_time = base::Time::Now();
  OneTimeToken token(OneTimeTokenType::kSmsOtp, "token1", first_time);
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(token));

  task_environment_.FastForwardBy(base::Seconds(1));

  base::Time second_time = base::Time::Now();
  OneTimeToken token2(OneTimeTokenType::kSmsOtp, "token1", second_time);
  EXPECT_FALSE(cache_.PurgeExpiredAndAdd(token2));
  const auto& tokens = cache_.PurgeExpiredAndGetCache();
  ASSERT_EQ(1u, tokens.size());
  EXPECT_EQ(tokens.front().on_device_arrival_time(), first_time);
  EXPECT_EQ(tokens.front().value(), "token1");
  EXPECT_EQ(tokens.front().type(), OneTimeTokenType::kSmsOtp);
}

// Ensure that PurgeExpiredAndAdd expires outdated tokens.
TEST_F(OneTimeTokenCacheTest, PurgeExpiredAndAdd_AddTokenAfterExpired) {
  OneTimeToken token1(OneTimeTokenType::kSmsOtp, "token1", base::Time::Now());
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(token1));

  task_environment_.FastForwardBy(kMaxAge + base::Seconds(1));

  OneTimeToken token2(OneTimeTokenType::kSmsOtp, "token2", base::Time::Now());
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(token2));

  const auto& tokens = cache_.PurgeExpiredAndGetCache();
  ASSERT_EQ(1u, tokens.size());
  EXPECT_EQ(token2, tokens.front());
}

// Ensure that the PurgeExpiredAndGetCache works correctly on an empty cache.
TEST_F(OneTimeTokenCacheTest, PurgeExpiredAndGetCache_Empty) {
  const auto& tokens = cache_.PurgeExpiredAndGetCache();
  EXPECT_TRUE(tokens.empty());
}

// Ensure that PurgeExpiredAndGetCache_WithTokens can return multiple tokens.
TEST_F(OneTimeTokenCacheTest, PurgeExpiredAndGetCache_WithTokens) {
  OneTimeToken token1(OneTimeTokenType::kSmsOtp, "token1", base::Time::Now());
  cache_.PurgeExpiredAndAdd(token1);

  task_environment_.FastForwardBy(base::Seconds(1));

  OneTimeToken token2(OneTimeTokenType::kSmsOtp, "token2", base::Time::Now());
  cache_.PurgeExpiredAndAdd(token2);

  const auto& tokens = cache_.PurgeExpiredAndGetCache();
  ASSERT_EQ(2u, tokens.size());
  // The most recent token goes to the end.
  EXPECT_EQ(token1, tokens.front());
  EXPECT_EQ(token2, tokens.back());
}

// Ensure that PurgeExpiredAndGetCache_WithTokens purges expired tokens.
TEST_F(OneTimeTokenCacheTest, PurgeExpiredAndGetCache_WithExpiredTokens) {
  OneTimeToken token1(OneTimeTokenType::kSmsOtp, "token1", base::Time::Now());
  cache_.PurgeExpiredAndAdd(token1);

  task_environment_.FastForwardBy(base::Seconds(5));
  OneTimeToken token2(OneTimeTokenType::kSmsOtp, "token2", base::Time::Now());
  cache_.PurgeExpiredAndAdd(token2);

  task_environment_.FastForwardBy(base::Seconds(6));

  const auto& tokens = cache_.PurgeExpiredAndGetCache();
  ASSERT_EQ(1u, tokens.size());
  EXPECT_EQ(token2, tokens.front());
}

// Ensure that tokens are sorted by time.
TEST_F(OneTimeTokenCacheTest, TokensAreSortedByTime) {
  base::Time now = base::Time::Now();
  OneTimeToken token2(OneTimeTokenType::kSmsOtp, "token2", now);
  OneTimeToken token3(OneTimeTokenType::kSmsOtp, "token3",
                      now + base::Seconds(1));
  OneTimeToken token1(OneTimeTokenType::kSmsOtp, "token1",
                      now - base::Seconds(1));

  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(token2));
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(token3));
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(token1));

  EXPECT_THAT(cache_.PurgeExpiredAndGetCache(),
              testing::ElementsAre(token1, token2, token3));
}

}  // namespace one_time_tokens
