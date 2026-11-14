// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/network_name_cache.h"

#include <atomic>
#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ConnectionType = net::NetworkChangeNotifier::ConnectionType;

class NetworkNameCacheTest : public testing::Test {
 protected:
  void NotifyNetworkChange(ConnectionType type) {
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(type);
  }

  // Runs tasks on the main thread only, without waiting for (possibly hung)
  // ThreadPool tasks. Used to verify that something did *not* happen, which
  // `base::test::RunUntil()` cannot express.
  void RunMainThreadUntilIdle() { base::RunLoop().RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_ =
      net::NetworkChangeNotifier::CreateMockIfNeeded();
};

TEST_F(NetworkNameCacheTest, DoesNotQueryUntilRequested) {
  std::atomic<int> call_count{0};
  auto cache = NetworkNameCache::CreateForTesting(
      base::BindLambdaForTesting([&call_count] {
        call_count++;
        return std::string("WiFi");
      }));

  NotifyNetworkChange(ConnectionType::CONNECTION_WIFI);
  // Verifies that no query is made, so wait for all tasks, not for a value.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, call_count.load());

  EXPECT_EQ("", cache->GetNetworkName());
  ASSERT_TRUE(
      base::test::RunUntil([&] { return cache->GetNetworkName() == "WiFi"; }));
  EXPECT_EQ(1, call_count.load());
}

TEST_F(NetworkNameCacheTest, DoesNotBlockCallingThreadWhenProviderHangs) {
  const base::PlatformThreadId main_thread_id =
      base::PlatformThread::CurrentId();
  std::atomic<base::PlatformThreadId> provider_thread_id{
      base::kInvalidThreadId};
  std::atomic<int> call_count{0};
  base::WaitableEvent provider_started;
  base::WaitableEvent unblock_provider;

  auto cache = NetworkNameCache::CreateForTesting(base::BindLambdaForTesting(
      [&provider_thread_id, &call_count, &provider_started, &unblock_provider] {
        provider_thread_id = base::PlatformThread::CurrentId();
        call_count++;
        provider_started.Signal();
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        unblock_provider.Wait();
        return std::string("EnterpriseWiFi-5G");
      }));

  // Returns immediately while the background query is blocked.
  EXPECT_EQ("", cache->GetNetworkName());
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    provider_started.Wait();
  }
  EXPECT_NE(main_thread_id, provider_thread_id.load());

  // Subsequent calls also return immediately, without starting new queries.
  EXPECT_EQ("", cache->GetNetworkName());
  RunMainThreadUntilIdle();
  EXPECT_EQ(1, call_count.load());

  unblock_provider.Signal();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return cache->GetNetworkName() == "EnterpriseWiFi-5G"; }));
  EXPECT_EQ(1, call_count.load());
}

TEST_F(NetworkNameCacheTest, RefreshesOnNetworkChangeAndClearsOnDisconnect) {
  std::atomic<int> call_count{0};
  auto cache = NetworkNameCache::CreateForTesting(
      base::BindLambdaForTesting([&call_count] {
        return std::string(++call_count == 1 ? "WiFi-Network-1"
                                             : "WiFi-Network-2");
      }));

  EXPECT_EQ("", cache->GetNetworkName());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return cache->GetNetworkName() == "WiFi-Network-1"; }));

  // Disconnecting clears the name without querying the provider. Wait for all
  // tasks to verify that no query is made.
  NotifyNetworkChange(ConnectionType::CONNECTION_NONE);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", cache->GetNetworkName());
  EXPECT_EQ(1, call_count.load());

  // Reconnecting queries the new name.
  NotifyNetworkChange(ConnectionType::CONNECTION_WIFI);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return cache->GetNetworkName() == "WiFi-Network-2"; }));
  EXPECT_EQ(2, call_count.load());
}

TEST_F(NetworkNameCacheTest, SupersedesQueriesWhileProviderHangs) {
  std::atomic<int> call_count{0};
  base::WaitableEvent first_call_started;
  base::WaitableEvent unblock_first_call;

  auto cache = NetworkNameCache::CreateForTesting(base::BindLambdaForTesting(
      [&call_count, &first_call_started, &unblock_first_call] {
        if (++call_count == 1) {
          first_call_started.Signal();
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
          unblock_first_call.Wait();
          return std::string("StaleWiFi");
        }
        return std::string("FreshWiFi");
      }));

  EXPECT_EQ("", cache->GetNetworkName());
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    first_call_started.Wait();
  }

  // Several network changes while the first query is hung.
  NotifyNetworkChange(ConnectionType::CONNECTION_NONE);
  NotifyNetworkChange(ConnectionType::CONNECTION_WIFI);
  NotifyNetworkChange(ConnectionType::CONNECTION_WIFI);
  RunMainThreadUntilIdle();

  // No other query runs concurrently with the hung one.
  EXPECT_EQ(1, call_count.load());
  EXPECT_EQ("", cache->GetNetworkName());

  // Once unblocked, the stale result is dropped and only the latest queued
  // query runs.
  unblock_first_call.Signal();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return cache->GetNetworkName() == "FreshWiFi"; }));
  EXPECT_EQ(2, call_count.load());
}

TEST_F(NetworkNameCacheTest, SetProviderForTestingDropsInFlightResult) {
  base::WaitableEvent first_call_started;
  base::WaitableEvent unblock_first_call;

  auto cache = NetworkNameCache::CreateForTesting(
      base::BindLambdaForTesting([&first_call_started, &unblock_first_call] {
        first_call_started.Signal();
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        unblock_first_call.Wait();
        return std::string("OldProviderWiFi");
      }));

  EXPECT_EQ("", cache->GetNetworkName());
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    first_call_started.Wait();
  }

  cache->SetProviderForTesting(base::BindLambdaForTesting(
      [] { return std::string("NewProviderWiFi"); }));
  unblock_first_call.Signal();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return cache->GetNetworkName() == "NewProviderWiFi"; }));
}

TEST_F(NetworkNameCacheTest, DestroyingCacheDropsInFlightReply) {
  base::WaitableEvent provider_started;
  base::WaitableEvent unblock_provider;

  auto cache = NetworkNameCache::CreateForTesting(
      base::BindLambdaForTesting([&provider_started, &unblock_provider] {
        provider_started.Signal();
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        unblock_provider.Wait();
        return std::string("WiFi");
      }));

  EXPECT_EQ("", cache->GetNetworkName());
  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    provider_started.Wait();
  }

  cache.reset();
  unblock_provider.Signal();
  // Must not run the reply on the destroyed cache (which would be a
  // use-after-free caught by ASan). Wait for all tasks, since this verifies
  // that something does *not* happen.
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace policy
