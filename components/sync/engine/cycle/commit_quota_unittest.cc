// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/commit_quota.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class CommitQuotaTest : public ::testing::Test {
 public:
  CommitQuotaTest() = default;

  void ConsumeTokensAndExpectDepleted(CommitQuota* quota, int n) {
    while (n > 0) {
      EXPECT_TRUE(quota->HasTokensAvailable());
      quota->ConsumeToken();
      --n;
    }
    EXPECT_FALSE(quota->HasTokensAvailable());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(CommitQuotaTest, NoTokensAvailableWhenDepleted) {
  CommitQuota quota(/*initial_tokens=*/2, /*refill_interval=*/base::Seconds(1));
  ConsumeTokensAndExpectDepleted(&quota, 2);
}

TEST_F(CommitQuotaTest, TokensRefill) {
  CommitQuota quota(/*initial_tokens=*/2, /*refill_interval=*/base::Seconds(1));
  ConsumeTokensAndExpectDepleted(&quota, 2);

  task_environment_.FastForwardBy(base::Milliseconds(1500));
  ConsumeTokensAndExpectDepleted(&quota, 1);

  task_environment_.FastForwardBy(base::Milliseconds(501));
  ConsumeTokensAndExpectDepleted(&quota, 1);
}

TEST_F(CommitQuotaTest, TokensCannotGetBelowZero) {
  CommitQuota quota(/*initial_tokens=*/2, /*refill_interval=*/base::Seconds(1));
  ConsumeTokensAndExpectDepleted(&quota, 2);

  // Try to consume another token. This has no effect.
  quota.ConsumeToken();

  // After one second, there's another token available.
  task_environment_.FastForwardBy(base::Milliseconds(1001));
  ConsumeTokensAndExpectDepleted(&quota, 1);
}

TEST_F(CommitQuotaTest, RefillPostponedWhenConsumingAtZero) {
  CommitQuota quota(/*initial_tokens=*/2, /*refill_interval=*/base::Seconds(1));
  ConsumeTokensAndExpectDepleted(&quota, 2);

  // After half a second, there are still no tokens.
  task_environment_.FastForwardBy(base::Milliseconds(501));
  EXPECT_FALSE(quota.HasTokensAvailable());

  // When consuming with zero tokens, the next refill gets postponed.
  quota.ConsumeToken();
  task_environment_.FastForwardBy(base::Milliseconds(501));
  EXPECT_FALSE(quota.HasTokensAvailable());
  task_environment_.FastForwardBy(base::Milliseconds(501));
  ConsumeTokensAndExpectDepleted(&quota, 1);
}

TEST_F(CommitQuotaTest, TokensRefillUpToInitialTokens) {
  CommitQuota quota(/*initial_tokens=*/2, /*refill_interval=*/base::Seconds(1));
  ConsumeTokensAndExpectDepleted(&quota, 2);

  task_environment_.FastForwardBy(base::Milliseconds(3001));
  // Waiting longer does not help -- we still end up with initial tokens.
  ConsumeTokensAndExpectDepleted(&quota, 2);
}

TEST_F(CommitQuotaTest, TokensStayAtInitialTokens) {
  CommitQuota quota(/*initial_tokens=*/2, /*refill_interval=*/base::Seconds(1));
  // The quota is full, waiting has no effect.
  task_environment_.FastForwardBy(base::Days(1));
  ConsumeTokensAndExpectDepleted(&quota, 2);

  task_environment_.FastForwardBy(base::Milliseconds(1001));
  // It also has no effect later, the quota does not fill up faster now.
  ConsumeTokensAndExpectDepleted(&quota, 1);
}

}  // namespace
}  // namespace syncer
