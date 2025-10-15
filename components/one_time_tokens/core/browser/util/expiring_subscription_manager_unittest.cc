// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

class ExpiringSubscriptionTest : public testing::Test {
 protected:
  using ExpiringSubscriptionManagerType =
      ExpiringSubscriptionManager<void(int)>;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Verifies that a subscription notifies subscribers.
TEST_F(ExpiringSubscriptionTest, Notification) {
  base::test::TestFuture<int> callback;

  base::Time now = base::Time::Now();
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscription subscription = subscription_manager.Subscribe(
      now + base::Minutes(5), callback.GetRepeatingCallback());
  EXPECT_TRUE(subscription_manager.Exists(subscription.handle()));

  subscription_manager.Notify(123);
  EXPECT_EQ(callback.Get<0>(), 123);
}

// Verifies that a subscription expires correctly.
TEST_F(ExpiringSubscriptionTest, Expiration) {
  base::test::TestFuture<int> callback;

  base::Time now = base::Time::Now();
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscription subscription = subscription_manager.Subscribe(
      now + base::Minutes(5), callback.GetRepeatingCallback());
  EXPECT_TRUE(subscription_manager.Exists(subscription.handle()));

  task_environment_.FastForwardBy(base::Minutes(5));

  EXPECT_FALSE(subscription_manager.Exists(subscription.handle()));

  // No more notifications after expiration.
  subscription_manager.Notify(456);
  EXPECT_FALSE(callback.IsReady());
}

// Verifies that a subscription can be canceled.
TEST_F(ExpiringSubscriptionTest, Cancel) {
  base::test::TestFuture<int> callback;

  base::Time now = base::Time::Now();
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscription subscription = subscription_manager.Subscribe(
      now + base::Minutes(5), callback.GetRepeatingCallback());
  EXPECT_TRUE(subscription_manager.Exists(subscription.handle()));

  subscription_manager.Cancel(subscription.handle());
  EXPECT_FALSE(subscription_manager.Exists(subscription.handle()));

  // No notification.
  subscription_manager.Notify(123);
  EXPECT_FALSE(callback.IsReady());
}

// Verifies that a subscription can be canceled via the subscription object.
TEST_F(ExpiringSubscriptionTest, SubscriptionCancel) {
  base::test::TestFuture<int> callback;

  base::Time now = base::Time::Now();
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscription subscription = subscription_manager.Subscribe(
      now + base::Minutes(5), callback.GetRepeatingCallback());
  EXPECT_TRUE(subscription.IsAlive());
  EXPECT_TRUE(subscription_manager.Exists(subscription.handle()));

  subscription.Cancel();
  EXPECT_FALSE(subscription.IsAlive());
  EXPECT_FALSE(subscription_manager.Exists(subscription.handle()));

  // No notification.
  subscription_manager.Notify(123);
  EXPECT_FALSE(callback.IsReady());
}

// Verifies behavior with multiple subscribers.
TEST_F(ExpiringSubscriptionTest, MultipleSubscribers) {
  base::test::TestFuture<int> callback1;
  base::test::TestFuture<int> callback2;

  base::Time now = base::Time::Now();
  ExpiringSubscriptionManagerType subscription_manager;

  // Note: the two subscriptions have 5 and 10 minutes expirations.
  ExpiringSubscription subscription1 = subscription_manager.Subscribe(
      now + base::Minutes(5), callback1.GetRepeatingCallback());
  ExpiringSubscription subscription2 = subscription_manager.Subscribe(
      now + base::Minutes(10), callback2.GetRepeatingCallback());

  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 2u);

  subscription_manager.Notify(123);
  EXPECT_EQ(callback1.Get<0>(), 123);
  EXPECT_EQ(callback2.Get<0>(), 123);

  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(subscription_manager.Exists(subscription1.handle()));
  EXPECT_TRUE(subscription_manager.Exists(subscription2.handle()));
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);

  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(subscription_manager.Exists(subscription2.handle()));
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 0u);
}

// Verifies that a notification callback can cancel its own subscription.
TEST_F(ExpiringSubscriptionTest, NotifyCallbackCancelsSelf) {
  ExpiringSubscriptionManagerType subscription_manager;

  int call_count = 0;
  ExpiringSubscription subscription;
  auto callback = base::BindLambdaForTesting([&](int) {
    call_count++;
    subscription_manager.Cancel(subscription.handle());
  });

  subscription = subscription_manager.Subscribe(
      base::Time::Now() + base::Minutes(5), std::move(callback));
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);

  subscription_manager.Notify(123);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 0u);
  EXPECT_FALSE(subscription_manager.Exists(subscription.handle()));

  // Second notification should not trigger callback.
  subscription_manager.Notify(123);
  EXPECT_EQ(call_count, 1);
}

// Verifies that a notification callback can cancel another subscription. If
// this happens during a callback, the next notification won't happen anymore.
TEST_F(ExpiringSubscriptionTest, NotifyCallbackCancelsOther) {
  ExpiringSubscriptionManagerType subscription_manager;

  int callback1_count = 0;
  int callback2_count = 0;

  ExpiringSubscription subscription1;
  ExpiringSubscription subscription2;
  auto callback1 = base::BindLambdaForTesting([&](int) {
    callback1_count++;
    subscription_manager.Cancel(subscription2.handle());
  });

  auto callback2 = base::BindLambdaForTesting([&](int) {
    callback2_count++;
    subscription_manager.Cancel(subscription1.handle());
  });

  subscription1 = subscription_manager.Subscribe(
      base::Time::Now() + base::Minutes(5), std::move(callback1));
  subscription2 = subscription_manager.Subscribe(
      base::Time::Now() + base::Minutes(5), std::move(callback2));

  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 2u);

  subscription_manager.Notify(123);

  // The first of the two callbacks cancels the other.
  if (callback1_count == 1) {
    EXPECT_EQ(callback1_count, 1);
    EXPECT_EQ(callback2_count, 0);
    EXPECT_TRUE(subscription_manager.Exists(subscription1.handle()));
    EXPECT_FALSE(subscription_manager.Exists(subscription2.handle()));
    EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);
  } else {
    EXPECT_EQ(callback1_count, 0);
    EXPECT_EQ(callback2_count, 1);
    EXPECT_FALSE(subscription_manager.Exists(subscription1.handle()));
    EXPECT_TRUE(subscription_manager.Exists(subscription2.handle()));
    EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);
  }
}

// Verifies that a notification callback can add a new subscription.
TEST_F(ExpiringSubscriptionTest, NotifyCallbackAddsSubscription) {
  ExpiringSubscriptionManagerType subscription_manager;

  int callback1_count = 0;
  int new_callback_count = 0;

  auto new_callback =
      base::BindLambdaForTesting([&](int) { new_callback_count++; });

  std::vector<ExpiringSubscription> new_subscriptions;
  auto callback1 = base::BindLambdaForTesting([&](int) {
    callback1_count++;
    new_subscriptions.emplace_back(subscription_manager.Subscribe(
        base::Time::Now() + base::Minutes(5), new_callback));
  });

  ExpiringSubscription subscription = subscription_manager.Subscribe(
      base::Time::Now() + base::Minutes(5), std::move(callback1));
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);

  subscription_manager.Notify(123);
  EXPECT_EQ(callback1_count, 1);
  // The new subscription is added during notification, but since we iterate
  // over a copy of handles from before notifications started, it won't be
  // notified in this round.
  EXPECT_EQ(new_callback_count, 0);
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 2u);

  subscription_manager.Notify(123);
  EXPECT_EQ(callback1_count, 2);
  EXPECT_EQ(new_callback_count, 1);
  // Another subscription was added in the first callback.
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 3u);
}

// Verifies that a subscription can be extended.
TEST_F(ExpiringSubscriptionTest, ExtendExpirationTime) {
  base::test::TestFuture<int> callback;

  base::Time now = base::Time::Now();
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscription subscription = subscription_manager.Subscribe(
      now + base::Minutes(5), callback.GetRepeatingCallback());
  EXPECT_TRUE(subscription_manager.Exists(subscription.handle()));

  subscription_manager.SetExpirationTime(subscription.handle(),
                                         now + base::Minutes(10));
  task_environment_.FastForwardBy(base::Minutes(8));

  EXPECT_TRUE(subscription_manager.Exists(subscription.handle()));

  // Notification should still work.
  subscription_manager.Notify(123);
  EXPECT_EQ(callback.Get<0>(), 123);

  // Now let it expire for real.
  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(subscription_manager.Exists(subscription.handle()));
}

// Verifies SetExpirationTime works as expected.
TEST_F(ExpiringSubscriptionTest, ShortenExpirationTime) {
  ExpiringSubscriptionManagerType subscription_manager;

  base::Time now = base::Time::Now();
  ExpiringSubscription subscription1 = subscription_manager.Subscribe(
      now + base::Minutes(10), base::DoNothing());

  // Shorten expiration.
  subscription_manager.SetExpirationTime(subscription1.handle(),
                                         now + base::Minutes(5));
  EXPECT_EQ(subscription_manager.GetExpirationTime(subscription1.handle()),
            now + base::Minutes(5));

  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(subscription_manager.Exists(subscription1.handle()));
}

// Verifies SetExpirationTime on the subscription object works as expected.
TEST_F(ExpiringSubscriptionTest, SubscriptionSetExpirationTime) {
  ExpiringSubscriptionManagerType subscription_manager;

  base::Time now = base::Time::Now();
  ExpiringSubscription subscription = subscription_manager.Subscribe(
      now + base::Minutes(10), base::DoNothing());
  EXPECT_TRUE(subscription.IsAlive());

  // Shorten expiration.
  subscription.SetExpirationTime(now + base::Minutes(5));
  EXPECT_EQ(subscription_manager.GetExpirationTime(subscription.handle()),
            now + base::Minutes(5));

  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(subscription_manager.Exists(subscription.handle()));
  EXPECT_FALSE(subscription.IsAlive());
}

// Verifies GetExpirationTime for a non-existent handle.
TEST_F(ExpiringSubscriptionTest, GetExpirationTimeNonExistent) {
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscriptionHandle handle(base::Uuid::GenerateRandomV4());
  EXPECT_TRUE(subscription_manager.GetExpirationTime(handle).is_null());
}

// Verifies Exists for various states.
TEST_F(ExpiringSubscriptionTest, Exists) {
  ExpiringSubscriptionManagerType subscription_manager;
  base::Time now = base::Time::Now();

  // Test with an active subscription.
  ExpiringSubscription subscription1 =
      subscription_manager.Subscribe(now + base::Minutes(5), base::DoNothing());
  EXPECT_TRUE(subscription_manager.Exists(subscription1.handle()));

  // Test with a canceled subscription.
  ExpiringSubscription subscription2 =
      subscription_manager.Subscribe(now + base::Minutes(5), base::DoNothing());
  subscription_manager.Cancel(subscription2.handle());
  EXPECT_FALSE(subscription_manager.Exists(subscription2.handle()));

  // Test with an expired subscription.
  ExpiringSubscription subscription3 =
      subscription_manager.Subscribe(now + base::Minutes(5), base::DoNothing());
  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_FALSE(subscription_manager.Exists(subscription3.handle()));

  // Test with a non-existent handle.
  ExpiringSubscriptionHandle handle(base::Uuid::GenerateRandomV4());
  EXPECT_FALSE(subscription_manager.Exists(handle));
}

// Verifies Notify() does not crash with no subscribers.
TEST_F(ExpiringSubscriptionTest, NotifyNoSubscribers) {
  ExpiringSubscriptionManagerType subscription_manager;
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 0u);
  subscription_manager.Notify(123);
  // No crash, test passes.
}

// Verifies that a subscription with an expiration in the past expires
// immediately.
TEST_F(ExpiringSubscriptionTest, ImmediateExpiration) {
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscription subscription = subscription_manager.Subscribe(
      base::Time::Now() - base::Minutes(1), base::DoNothing());
  EXPECT_TRUE(subscription_manager.Exists(subscription.handle()));

  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, task_environment_.QuitClosure());
  task_environment_.RunUntilQuit();
  EXPECT_FALSE(subscription_manager.Exists(subscription.handle()));
}

// Verifies that destroying the manager cancels pending expirations.
TEST_F(ExpiringSubscriptionTest, ManagerDestruction) {
  auto manager = std::make_unique<ExpiringSubscriptionManagerType>();

  ExpiringSubscription subscription = manager->Subscribe(
      base::Time::Now() + base::Minutes(5), base::DoNothing());

  // Destroy the manager before the subscription expires.
  manager.reset();

  EXPECT_FALSE(subscription.IsAlive());
}

// Verifies that a notification callback can modify the expiration of another
// subscription.
TEST_F(ExpiringSubscriptionTest, NotifyCallbackModifiesExpiration) {
  ExpiringSubscriptionManagerType subscription_manager;
  base::Time now = base::Time::Now();
  ExpiringSubscription subscription1;
  ExpiringSubscription subscription2;

  auto callback1 = base::BindLambdaForTesting([&](int) {
    subscription_manager.SetExpirationTime(subscription2.handle(),
                                           now + base::Minutes(10));
  });

  subscription1 = subscription_manager.Subscribe(now + base::Minutes(5),
                                                 std::move(callback1));
  subscription2 =
      subscription_manager.Subscribe(now + base::Minutes(5), base::DoNothing());

  subscription_manager.Notify(123);

  EXPECT_EQ(subscription_manager.GetExpirationTime(subscription2.handle()),
            now + base::Minutes(10));
}

// Verifies that multiple subscriptions expiring at the same time are handled
// correctly.
TEST_F(ExpiringSubscriptionTest, MultipleSubscriptionsExpireAtSameTime) {
  ExpiringSubscriptionManagerType subscription_manager;
  base::Time now = base::Time::Now();

  ExpiringSubscription subscription1 =
      subscription_manager.Subscribe(now + base::Minutes(5), base::DoNothing());
  ExpiringSubscription subscription2 =
      subscription_manager.Subscribe(now + base::Minutes(5), base::DoNothing());

  task_environment_.FastForwardBy(base::Minutes(5));

  EXPECT_FALSE(subscription1.IsAlive());
  EXPECT_FALSE(subscription2.IsAlive());
}

// Verifies that calling Cancel() on a non-existent handle is safe.
TEST_F(ExpiringSubscriptionTest, CancelNonExistent) {
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscriptionHandle handle(base::Uuid::GenerateRandomV4());
  // Should not crash.
  subscription_manager.Cancel(handle);
}

// Verifies the move semantics of ExpiringSubscription.
TEST_F(ExpiringSubscriptionTest, ExpiringSubscriptionMove) {
  // This test verifies std::move behavior that is flagged by clang tidy.
  // NOLINTBEGIN(bugprone-use-after-move)

  ExpiringSubscriptionManagerType subscription_manager;

  ExpiringSubscription subscription1 = subscription_manager.Subscribe(
      base::Time::Now() + base::Minutes(5), base::DoNothing());
  ExpiringSubscriptionHandle handle = subscription1.handle();
  EXPECT_TRUE(subscription1.IsAlive());
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);

  // Test move constructor.
  ExpiringSubscription subscription2(std::move(subscription1));
  EXPECT_FALSE(subscription1.IsAlive());
  EXPECT_TRUE(subscription2.IsAlive());
  EXPECT_EQ(subscription2.handle(), handle);
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);

  // Test move assignment.
  ExpiringSubscription subscription3;
  EXPECT_FALSE(subscription3.IsAlive());
  subscription3 = std::move(subscription2);
  EXPECT_FALSE(subscription2.IsAlive());
  EXPECT_TRUE(subscription3.IsAlive());
  EXPECT_EQ(subscription3.handle(), handle);
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);

  // Test destruction.
  subscription3 = ExpiringSubscription();
  EXPECT_FALSE(subscription3.IsAlive());
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 0u);

  // NOLINTEND(bugprone-use-after-move)
}

// Verifies that move-assigning to an active subscription cancels the old one.
TEST_F(ExpiringSubscriptionTest, ExpiringSubscriptionMoveAssignmentToActive) {
  // This test verifies std::move behavior that is flagged by clang tidy.
  // NOLINTBEGIN(bugprone-use-after-move)

  ExpiringSubscriptionManagerType subscription_manager;

  base::Time now = base::Time::Now();
  ExpiringSubscription subscription1 =
      subscription_manager.Subscribe(now + base::Minutes(5), base::DoNothing());
  ExpiringSubscriptionHandle handle1 = subscription1.handle();

  ExpiringSubscription subscription2 = subscription_manager.Subscribe(
      now + base::Minutes(10), base::DoNothing());
  ExpiringSubscriptionHandle handle2 = subscription2.handle();

  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 2u);

  // This should cancel the subscription associated with handle1.
  subscription1 = std::move(subscription2);

  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 1u);
  EXPECT_FALSE(subscription_manager.Exists(handle1));
  EXPECT_TRUE(subscription_manager.Exists(handle2));
  EXPECT_EQ(subscription1.handle(), handle2);
  EXPECT_FALSE(subscription2.IsAlive());

  // NOLINTEND(bugprone-use-after-move)
}

// Verifies SetExpirationTime() on a non-existent handle is safe.
TEST_F(ExpiringSubscriptionTest, SetExpirationTimeNonExistent) {
  ExpiringSubscriptionManagerType subscription_manager;
  ExpiringSubscriptionHandle handle(base::Uuid::GenerateRandomV4());
  // Should not crash and should not add a subscription.
  subscription_manager.SetExpirationTime(handle,
                                         base::Time::Now() + base::Minutes(5));
  EXPECT_EQ(subscription_manager.GetNumberSubscribers(), 0u);
  EXPECT_FALSE(subscription_manager.Exists(handle));
}

}  // namespace one_time_tokens
