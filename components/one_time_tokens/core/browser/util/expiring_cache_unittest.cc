// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/util/expiring_cache.h"

#include <optional>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

namespace {

// Time after which items expire from the cache.
constexpr base::TimeDelta kMaxAge = base::Seconds(10);

// A lightweight test struct representing a token for `ExpiringCacheTest`.
// Used in place of the production `OneTimeToken` because `OneTimeToken` no
// longer defines `operator==`, decoupling generic `ExpiringCache` tests from
// the domain class while verifying caching behavior with a type that provides
// its own equality logic (matching by token value, ignoring arrival time).
struct TestOneTimeToken {
  std::string value;
  base::TimeTicks timestamp;

  // Compares items by value only, ignoring timestamp.
  bool operator==(const TestOneTimeToken& other) const {
    return value == other.value;
  }
};

// A test struct without an `operator==` defined, used to verify that
// `ExpiringCache` can manage and deduplicate items via a custom projection
// function when the cached type does not support equality comparison.
struct ItemWithoutEquality {
  std::string key;
  int payload;
  base::TimeTicks timestamp;
};

// A composite key struct and a test item struct without `operator==`. This
// mirrors the production usage of `CacheKey` and `CacheProjection` in
// `OneTimeTokenServiceImpl`, where equality is determined by a composite key
// of token attributes (e.g. type, value, sender) while explicitly excluding
// the arrival timestamp.
struct CompositeKey {
  std::string type;
  std::string value;
  std::optional<std::string> sender;

  bool operator==(const CompositeKey&) const = default;
};

struct ItemWithCompositeKey {
  std::string type;
  std::string value;
  std::optional<std::string> sender;
  base::TimeTicks arrival_time;
};

}  // namespace

class ExpiringCacheTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ExpiringCache<TestOneTimeToken, decltype(&TestOneTimeToken::timestamp)>
      cache_{kMaxAge, &TestOneTimeToken::timestamp};
};

// Ensure that a new item can be added.
TEST_F(ExpiringCacheTest, PurgeExpiredAndAdd_AddNewItem) {
  TestOneTimeToken item{"token1", base::TimeTicks::Now()};
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item));
  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(), testing::ElementsAre(item));
}

// Ensure that an item is not added a second time.
TEST_F(ExpiringCacheTest, PurgeExpiredAndAdd_AddExistingItem) {
  base::TimeTicks now = base::TimeTicks::Now();
  TestOneTimeToken item{"token1", now};
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item));
  EXPECT_FALSE(cache_.PurgeExpiredAndAdd(item));
  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(), testing::ElementsAre(item));
}

// Ensure that an item is not added a second time, if everything but the
// timestamp exists in the cache already.
TEST_F(ExpiringCacheTest,
       PurgeExpiredAndAdd_AddExistingItemWithDifferentTimestamp) {
  base::TimeTicks first_time = base::TimeTicks::Now();
  TestOneTimeToken item{"token1", first_time};
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item));

  task_environment_.FastForwardBy(base::Seconds(1));

  base::TimeTicks second_time = base::TimeTicks::Now();
  TestOneTimeToken item2{"token1", second_time};
  EXPECT_FALSE(cache_.PurgeExpiredAndAdd(item2));
  const auto& items = cache_.PurgeExpiredAndGetItems();
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(items.front().timestamp, first_time);
  EXPECT_EQ(items.front().value, "token1");
}

// Ensure that PurgeExpiredAndAdd expires outdated items.
TEST_F(ExpiringCacheTest, PurgeExpiredAndAdd_AddItemAfterExpired) {
  TestOneTimeToken item1{"token1", base::TimeTicks::Now()};
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item1));

  task_environment_.FastForwardBy(kMaxAge + base::Seconds(1));

  TestOneTimeToken item2{"token2", base::TimeTicks::Now()};
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
  TestOneTimeToken item1{"token1", base::TimeTicks::Now()};
  cache_.PurgeExpiredAndAdd(item1);

  task_environment_.FastForwardBy(base::Seconds(1));

  TestOneTimeToken item2{"token2", base::TimeTicks::Now()};
  cache_.PurgeExpiredAndAdd(item2);

  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(),
              testing::ElementsAre(item1, item2));
}

// Ensure that PurgeExpiredAndGetItems purges expired items.
TEST_F(ExpiringCacheTest, PurgeExpiredAndGetItems_WithExpiredItems) {
  TestOneTimeToken item1{"token1", base::TimeTicks::Now()};
  cache_.PurgeExpiredAndAdd(item1);

  task_environment_.FastForwardBy(base::Seconds(5));
  TestOneTimeToken item2{"token2", base::TimeTicks::Now()};
  cache_.PurgeExpiredAndAdd(item2);

  task_environment_.FastForwardBy(base::Seconds(6));

  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(), testing::ElementsAre(item2));
}

// Ensure that items are sorted by time.
TEST_F(ExpiringCacheTest, ItemsAreSortedByTime) {
  base::TimeTicks now = base::TimeTicks::Now();
  TestOneTimeToken item2{"token2", now};
  TestOneTimeToken item3{"token3", now + base::Seconds(1)};
  TestOneTimeToken item1{"token1", now - base::Seconds(1)};

  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item2));
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item3));
  EXPECT_TRUE(cache_.PurgeExpiredAndAdd(item1));

  EXPECT_THAT(cache_.PurgeExpiredAndGetItems(),
              testing::ElementsAre(item1, item2, item3));
}

// Ensure that GetItems() does not purge expired items.
TEST_F(ExpiringCacheTest, Items_DoesNotPurgeExpiredItems) {
  TestOneTimeToken item1{"token1", base::TimeTicks::Now()};
  cache_.PurgeExpiredAndAdd(item1);

  task_environment_.FastForwardBy(base::Seconds(2));
  TestOneTimeToken item2{"token2", base::TimeTicks::Now()};
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
  TestOneTimeToken item1{"token1", base::TimeTicks::Now()};
  cache_.PurgeExpiredAndAdd(item1);

  task_environment_.FastForwardBy(base::Seconds(2));
  TestOneTimeToken item2{"token2", base::TimeTicks::Now()};
  cache_.PurgeExpiredAndAdd(item2);

  // Fast forward so item1 is expired, but item2 is not expired.
  task_environment_.FastForwardBy(kMaxAge - base::Seconds(1));

  // TakeItems should return only item2, as item1 is expired.
  std::list<TestOneTimeToken> consumed_items = cache_.TakeItems();
  EXPECT_THAT(consumed_items, testing::ElementsAre(item2));

  // The cache should be empty now.
  EXPECT_TRUE(cache_.GetItems().empty());
}

// Ensure that TakeItems() works on an empty cache.
TEST_F(ExpiringCacheTest, TakeItems_Empty) {
  std::list<TestOneTimeToken> consumed_items = cache_.TakeItems();
  EXPECT_TRUE(consumed_items.empty());
  EXPECT_TRUE(cache_.GetItems().empty());
}

// Ensure that custom projections work for items that do not define an equality
// operator.
TEST_F(ExpiringCacheTest, CustomProjectionWithoutEquality) {
  auto projection = [](const ItemWithoutEquality& item) { return item.key; };
  ExpiringCache<ItemWithoutEquality, decltype(&ItemWithoutEquality::timestamp),
                decltype(projection)>
      cache(kMaxAge, &ItemWithoutEquality::timestamp, projection);

  ItemWithoutEquality item1{"k1", 1, base::TimeTicks::Now()};
  EXPECT_TRUE(cache.PurgeExpiredAndAdd(item1));

  ItemWithoutEquality item2{"k1", 2, base::TimeTicks::Now()};
  EXPECT_FALSE(cache.PurgeExpiredAndAdd(item2));

  ItemWithoutEquality item3{"k2", 3, base::TimeTicks::Now()};
  EXPECT_TRUE(cache.PurgeExpiredAndAdd(item3));
}

// Ensure that a composite projection with multiple fields can be used for
// deduplication, ignoring non-key fields such as the arrival timestamp.
TEST_F(ExpiringCacheTest, CompositeProjectionExcludingTimestamp) {
  auto projection = [](const ItemWithCompositeKey& item) -> CompositeKey {
    return {item.type, item.value, item.sender};
  };
  ExpiringCache<ItemWithCompositeKey,
                decltype(&ItemWithCompositeKey::arrival_time),
                decltype(projection)>
      cache(kMaxAge, &ItemWithCompositeKey::arrival_time, projection);

  base::TimeTicks now = base::TimeTicks::Now();
  ItemWithCompositeKey token1{"sms", "123456", "Google", now};
  EXPECT_TRUE(cache.PurgeExpiredAndAdd(token1));

  // Same key attributes but a different arrival timestamp should be treated as
  // a duplicate.
  ItemWithCompositeKey token1_different_time{"sms", "123456", "Google",
                                             now + base::Seconds(1)};
  EXPECT_FALSE(cache.PurgeExpiredAndAdd(token1_different_time));

  // Same type and value but different sender should not be a duplicate.
  ItemWithCompositeKey token_different_sender{"sms", "123456", "OtherSender",
                                              now + base::Seconds(2)};
  EXPECT_TRUE(cache.PurgeExpiredAndAdd(token_different_sender));

  // Different value should not be a duplicate.
  ItemWithCompositeKey token_different_value{"sms", "654321", "Google",
                                             now + base::Seconds(3)};
  EXPECT_TRUE(cache.PurgeExpiredAndAdd(token_different_value));

  // Different type should not be a duplicate.
  ItemWithCompositeKey token_different_type{"email", "123456", "Google",
                                            now + base::Seconds(4)};
  EXPECT_TRUE(cache.PurgeExpiredAndAdd(token_different_type));

  const auto& items = cache.PurgeExpiredAndGetItems();
  EXPECT_EQ(items.size(), 4u);
}

}  // namespace one_time_tokens
