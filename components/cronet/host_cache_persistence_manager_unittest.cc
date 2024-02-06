// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/host_cache_persistence_manager.h"

#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_cache.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

class HostCachePersistenceManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    cache_ = std::make_unique<net::HostCache>(/*max_entries=*/1000);
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterListPref(kPrefName);
  }

  void MakePersistenceManager(base::TimeDelta delay) {
    persistence_manager_ = std::make_unique<HostCachePersistenceManager>(
        cache_.get(), pref_service_.get(), kPrefName, delay, nullptr);
  }

  // Sets an entry in the HostCache in order to trigger a pref write. The
  // caller is responsible for making sure this is a change that will trigger
  // a write, and the HostCache's interaction with its PersistenceDelegate is
  // assumed to work (it's tested in net/dns/host_cache_unittest.cc).
  void WriteToCache(const std::string& host) {
    net::HostCache::Key key(host, net::DnsQueryType::UNSPECIFIED, 0,
                            net::HostResolverSource::ANY,
                            net::NetworkAnonymizationKey());
    net::HostCache::Entry entry(net::OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                                net::HostCache::Entry::SOURCE_UNKNOWN);
    cache_->Set(key, entry, base::TimeTicks::Now(), base::Seconds(1));
  }

  // Reads the current value of the pref from the TestingPrefServiceSimple
  // and deserializes it into a temporary new HostCache. Only checks the size,
  // not the full contents, since the tests in this file are only intended
  // to test that writes happen when they're supposed to, not serialization
  // correctness.
  void CheckPref(size_t expected_size) {
    const base::Value* value = pref_service_->GetUserPref(kPrefName);
    net::HostCache temp_cache(10);
    if (value)
      temp_cache.RestoreFromListValue(value->GetList());
    ASSERT_EQ(expected_size, temp_cache.size());
  }

  // Generates a temporary HostCache with a few entries and uses it to
  // initialize the value in prefs.
  void InitializePref() {
    net::HostCache temp_cache(10);

    net::HostCache::Key key1("1.test", net::DnsQueryType::UNSPECIFIED, 0,
                             net::HostResolverSource::ANY,
                             net::NetworkAnonymizationKey());
    net::HostCache::Key key2("2.test", net::DnsQueryType::UNSPECIFIED, 0,
                             net::HostResolverSource::ANY,
                             net::NetworkAnonymizationKey());
    net::HostCache::Key key3("3.test", net::DnsQueryType::UNSPECIFIED, 0,
                             net::HostResolverSource::ANY,
                             net::NetworkAnonymizationKey());
    net::HostCache::Entry entry(net::OK, /*ip_endpoints=*/{}, /*aliases=*/{},
                                net::HostCache::Entry::SOURCE_UNKNOWN);

    temp_cache.Set(key1, entry, base::TimeTicks::Now(), base::Seconds(1));
    temp_cache.Set(key2, entry, base::TimeTicks::Now(), base::Seconds(1));
    temp_cache.Set(key3, entry, base::TimeTicks::Now(), base::Seconds(1));

    base::Value::List list;
    temp_cache.GetList(list, false /* include_stale */,
                       net::HostCache::SerializationType::kRestorable);
    pref_service_->SetList(kPrefName, std::move(list));
  }

  static const char kPrefName[];

  base::test::TaskEnvironment task_environment_;
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;

  // The HostCache and PrefService have to outlive the
  // HostCachePersistenceManager.
  std::unique_ptr<net::HostCache> cache_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<HostCachePersistenceManager> persistence_manager_;
};

const char HostCachePersistenceManagerTest::kPrefName[] = "net.test";

// Make a single change to the HostCache and make sure that it's written
// when the timer expires. Then repeat.
TEST_F(HostCachePersistenceManagerTest, SeparateWrites) {
  MakePersistenceManager(base::Seconds(60));

  WriteToCache("1.test");
  task_runner_->FastForwardBy(base::Seconds(59));
  CheckPref(0);
  task_runner_->FastForwardBy(base::Seconds(1));
  CheckPref(1);

  WriteToCache("2.test");
  task_runner_->FastForwardBy(base::Seconds(59));
  CheckPref(1);
  task_runner_->FastForwardBy(base::Seconds(1));
  CheckPref(2);
}

// Write to the HostCache multiple times and make sure that all changes
// are written to prefs at the appropriate times.
TEST_F(HostCachePersistenceManagerTest, MultipleWrites) {
  MakePersistenceManager(base::Seconds(300));

  WriteToCache("1.test");
  WriteToCache("2.test");
  task_runner_->FastForwardBy(base::Seconds(299));
  CheckPref(0);
  task_runner_->FastForwardBy(base::Seconds(1));
  CheckPref(2);

  WriteToCache("3.test");
  WriteToCache("4.test");
  task_runner_->FastForwardBy(base::Seconds(299));
  CheckPref(2);
  task_runner_->FastForwardBy(base::Seconds(1));
  CheckPref(4);
}

// Make changes to the HostCache at different times and ensure that the writes
// to prefs are batched as expected.
TEST_F(HostCachePersistenceManagerTest, BatchedWrites) {
  MakePersistenceManager(base::Milliseconds(100));

  WriteToCache("1.test");
  task_runner_->FastForwardBy(base::Milliseconds(30));
  WriteToCache("2.test");
  task_runner_->FastForwardBy(base::Milliseconds(30));
  WriteToCache("3.test");
  CheckPref(0);
  task_runner_->FastForwardBy(base::Milliseconds(40));
  CheckPref(3);

  // Add a delay in between batches.
  task_runner_->FastForwardBy(base::Milliseconds(50));

  WriteToCache("4.test");
  task_runner_->FastForwardBy(base::Milliseconds(30));
  WriteToCache("5.test");
  task_runner_->FastForwardBy(base::Milliseconds(30));
  WriteToCache("6.test");
  CheckPref(3);
  task_runner_->FastForwardBy(base::Milliseconds(40));
  CheckPref(6);
}

// Set the pref before the HostCachePersistenceManager is created, and make
// sure it gets picked up by the HostCache.
TEST_F(HostCachePersistenceManagerTest, InitAfterPrefs) {
  CheckPref(0);
  InitializePref();
  CheckPref(3);

  MakePersistenceManager(base::Seconds(1));
  task_runner_->RunUntilIdle();
  ASSERT_EQ(3u, cache_->size());
}

// Set the pref after the HostCachePersistenceManager is created, and make
// sure it gets picked up by the HostCache.
TEST_F(HostCachePersistenceManagerTest, InitBeforePrefs) {
  MakePersistenceManager(base::Seconds(1));
  ASSERT_EQ(0u, cache_->size());

  CheckPref(0);
  InitializePref();
  CheckPref(3);
  ASSERT_EQ(3u, cache_->size());
}

}  // namespace cronet
