// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/util/expiring_cache.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {
// Time after which items expire from the cache.
constexpr base::TimeDelta kMaxAge = base::Seconds(10);
}  // namespace

class ExpiringCacheTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ExpiringCache<OneTimeToken, decltype(&OneTimeToken::on_device_arrival_time)>
      cache_{kMaxAge, &OneTimeToken::on_device_arrival_time};
};

// Ensure that a new item can be added.
TEST_F(ExpiringCacheTest, PurgeExpiredAndAdd_AddNewItem) {
  OneTimeToken item(OneTimeTokenType::kSmsOtp, "token1",
                    base::TimeTicks::Now());
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item));
  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(), testing::ElementsAre(item));
}

// Ensure that an item is not added a second time.
TEST_F(ExpiringCacheTest, PurgeExpiredAndAdd_AddExistingItem) {
  base::TimeTicks now = base::TimeTicks::Now();
  OneTimeToken item(OneTimeTokenType::kSmsOtp, "token1", now);
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item));
  EXPECT_FALSE(cache_.PurgeExpiredAndAdd(item));
  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(), testing::ElementsAre(item));
}

// Ensure that an item is not added a second time, if everything but the
// timestamp exists in the cache already.
TEST_F(ExpiringCacheTest,
       PurgeExpiredAndAdd_AddExistingItemWithDifferentTimestamp) {
  base::TimeTicks first_time = base::TimeTicks::Now();
  OneTimeToken item(OneTimeTokenType::kSmsOtp, "token1", first_time);
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item));

  task_environment_.FastForwardBy(base::Seconds(1));

  base::TimeTicks second_time = base::TimeTicks::Now();
  OneTimeToken item2(OneTimeTokenType::kSmsOtp, "token1", second_time);
  EXPECT_FALSE(cache_.PurgeExpiredAndAdd(item2));
  const auto& items = cache_.PurgeExpiredAndGetItems();
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(items.front().on_device_arrival_time(), first_time);
  EXPECT_EQ(items.front().value(), "token1");
  EXPECT_EQ(items.front().type(), OneTimeTokenType::kSmsOtp);
}

// Ensure that PurgeExpiredAndAdd expires outdated items.
TEST_F(ExpiringCacheTest, PurgeExpiredAndAdd_AddItemAfterExpired) {
  OneTimeToken item1(OneTimeTokenType::kSmsOtp, "token1",
                     base::TimeTicks::Now());
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item1));

  task_environment_.FastForwardBy(kMaxAge + base::Seconds(1));

  OneTimeToken item2(OneTimeTokenType::kSmsOtp, "token2",
                     base::TimeTicks::Now());
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item2));
  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(), testing::ElementsAre(item2));
}

// Ensure that the PurgeExpiredAndGetItems works correctly on an empty cache.
TEST_F(ExpiringCacheTest, PurgeExpiredAndGetItems_Empty) {
  const auto& items = cache_.PurgeExpiredAndGetItems();
  EXPECT_TRUE(items.empty());
}

// Ensure that PurgeExpiredAndGetItems can return multiple items.
TEST_F(ExpiringCacheTest, PurgeExpiredAndGetItems_WithItems) {
  OneTimeToken item1(OneTimeTokenType::kSmsOtp, "token1",
                     base::TimeTicks::Now());
  cache_.PurgeExpiredAndAdd(item1);

  task_environment_.FastForwardBy(base::Seconds(1));

  OneTimeToken item2(OneTimeTokenType::kSmsOtp, "token2",
                     base::TimeTicks::Now());
  cache_.PurgeExpiredAndAdd(item2);

  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(),
              testing::ElementsAre(item1, item2));
}

// Ensure that PurgeExpiredAndGetItems purges expired items.
TEST_F(ExpiringCacheTest, PurgeExpiredAndGetItems_WithExpiredItems) {
  OneTimeToken item1(OneTimeTokenType::kSmsOtp, "token1",
                     base::TimeTicks::Now());
  cache_.PurgeExpiredAndAdd(item1);

  task_environment_.FastForwardBy(base::Seconds(5));
  OneTimeToken item2(OneTimeTokenType::kSmsOtp, "token2",
                     base::TimeTicks::Now());
  cache_.PurgeExpiredAndAdd(item2);

  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(), testing::ElementsAre(item2));
}

// Ensure that items are sorted by time.
TEST_F(ExpiringCacheTest, ItemsAreSortedByTime) {
  base::TimeTicks now = base::TimeTicks::Now();
  OneTimeToken item2(OneTimeTokenType::kSmsOtp, "token2", now);
  OneTimeToken item3(OneTimeTokenType::kSmsOtp, "token3",
                     now + base::Seconds(1));
  OneTimeToken item1(OneTimeTokenType::kSmsOtp, "token1",
                     now - base::Seconds(1));

  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item2));
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item3));
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item1));

  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(),
              testing::ElementsAre(item1, item2, item3));
}

// Ensure that GetItems() does not purge expired items.
TEST_F(ExpiringCacheTest, Items_DoesNotPurgeExpiredItems) {
  OneTimeToken item1(OneTimeTokenType::kSmsOtp, "token1",
                     base::TimeTicks::Now());
  cache_.PurgeExpiredAndAdd(item1);

  task_environment_.FastForwardBy(base::Seconds(2));
  OneTimeToken item2(OneTimeTokenType::kSmsOtp, "token2",
                     base::TimeTicks::Now());
  cache_.PurgeExpiredAndAdd(item2);

  // Fast forward so item1 is expired, but item2 is not expired
  task_environment_.FastForwardBy(kMaxAge - base::Seconds(1));

  // At this point, item1 should be expired, but items() should still return
  // it because no purging method was called.
  const auto& items = cache_.GetItems();
  EXPECT_THAT(items, testing::ElementsAre(item1, item2));

  // Verify that the items are still in the cache after another items() call.
  const auto& items_after_second_call = cache_.GetItems();
  ASSERT_EQ(2u, items_after_second_call.size());
  EXPECT_THAT(items_after_second_call, testing::ElementsAre(item1, item2));

  // Only a purging method should remove expired items.
  const auto& purged_items = cache_.PurgeExpiredAndGetItems();
  ASSERT_EQ(1u, purged_items.size());
  EXPECT_THAT(purged_items, testing::ElementsAre(item2));
}

// Ensure that TakeItems() returns only non-expired items and clears the
// cache.
TEST_F(ExpiringCacheTest, TakeItems) {
  OneTimeToken item1(OneTimeTokenType::kSmsOtp, "token1",
                     base::TimeTicks::Now());
  cache_.PurgeExpiredAndAdd(item1);

  task_environment_.FastForwardBy(base::Seconds(2));
  OneTimeToken item2(OneTimeTokenType::kSmsOtp, "token2",
                     base::TimeTicks::Now());
  cache_.PurgeExpiredAndAdd(item2);

  // Fast forward so item1 is expired, but item2 is not expired.
  task_environment_.FastForwardBy(kMaxAge - base::Seconds(1));

  // TakeItems should return only item2, as item1 is expired.
  std::list<OneTimeToken> consumed_items = cache_.TakeItems();
  EXPECT_THAT(consumed_items, testing::ElementsAre(item2));

  // The cache should be empty now.
  EXPECT_TRUE(cache_.GetItems().empty());
}

// Ensure that TakeItems() works on an empty cache.
TEST_F(ExpiringCacheTest, TakeItems_Empty) {
  std::list<OneTimeToken> consumed_items = cache_.TakeItems();
  EXPECT_TRUE(consumed_items.empty());
  EXPECT_TRUE(cache_.GetItems().empty());
}

}  // namespace one_time_tokens
