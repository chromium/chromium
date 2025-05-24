// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/top_level_host_scan_cache.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "chromeos/ash/components/tether/fake_host_scan_cache.h"
#include "chromeos/ash/components/tether/host_scan_test_util.h"
#include "chromeos/ash/components/tether/persistent_host_scan_cache.h"
#include "chromeos/ash/components/timer_factory/timer_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

class FakePersistentHostScanCache : public FakeHostScanCache,
                                    public PersistentHostScanCache {
 public:
  FakePersistentHostScanCache() = default;
  ~FakePersistentHostScanCache() override = default;

  // PersistentHostScanCache:
  std::unordered_map<std::string, HostScanCacheEntry> GetStoredCacheEntries()
      override {
    return cache();
  }
};

// MockTimer which invokes a callback in its destructor.
class ExtendedMockTimer : public base::MockOneShotTimer {
 public:
  explicit ExtendedMockTimer(base::OnceClosure destructor_callback)
      : destructor_callback_(std::move(destructor_callback)) {}

  ~ExtendedMockTimer() override { std::move(destructor_callback_).Run(); }

 private:
  base::OnceClosure destructor_callback_;
};

class TestTimerFactory : public ash::timer_factory::TimerFactory {
 public:
  TestTimerFactory() = default;
  ~TestTimerFactory() override = default;

  std::unordered_map<std::string, raw_ptr<ExtendedMockTimer, CtnExperimental>>&
  tether_network_guid_to_timer_map() {
    return tether_network_guid_to_timer_map_;
  }

  void set_tether_network_guid_for_next_timer(
      const std::string& tether_network_guid_for_next_timer) {
    tether_network_guids_for_upcoming_timers_.push_back(
        tether_network_guid_for_next_timer);
  }

  // TimerFactory:
  std::unique_ptr<base::OneShotTimer> CreateOneShotTimer() override {
    EXPECT_TRUE(!tether_network_guids_for_upcoming_timers_.empty());

    // Pop the first GUID off the list of upcoming GUIDs.
    std::string guid_for_timer =
        tether_network_guids_for_upcoming_timers_.front();
    EXPECT_FALSE(guid_for_timer.empty());
    tether_network_guids_for_upcoming_timers_.erase(
        tether_network_guids_for_upcoming_timers_.begin());

    ExtendedMockTimer* mock_timer = new ExtendedMockTimer(
        base::BindOnce(&TestTimerFactory::OnActiveTimerDestructor,
                       base::Unretained(this), guid_for_timer));
    tether_network_guid_to_timer_map_[guid_for_timer] = mock_timer;

    return base::WrapUnique(mock_timer);
  }

 private:
  void OnActiveTimerDestructor(const std::string& tether_network_guid) {
    tether_network_guid_to_timer_map_.erase(
        tether_network_guid_to_timer_map_.find(tether_network_guid));
  }

  std::vector<std::string> tether_network_guids_for_upcoming_timers_;
  std::unordered_map<std::string, raw_ptr<ExtendedMockTimer, CtnExperimental>>
      tether_network_guid_to_timer_map_;
};

}  // namespace

// TODO(khorimoto): The test uses a FakeHostScanCache to keep an in-memory
// cache of expected values. This has the potential to be confusing, since this
// is the test for TopLevelHostScanCache. Clean this up to avoid using
// FakeHostScanCache if possible.
class TopLevelHostScanCacheTest : public testing::Test {
 public:
  TopLevelHostScanCacheTest(const TopLevelHostScanCacheTest&) = delete;
  TopLevelHostScanCacheTest& operator=(const TopLevelHostScanCacheTest&) =
      delete;

 protected:
  TopLevelHostScanCacheTest()
      : test_entries_(host_scan_test_util::CreateTestEntries()) {}

  void SetUp() override {
    test_timer_factory_ = new TestTimerFactory();
    fake_active_host_ = std::make_unique<FakeActiveHost>();
    fake_network_host_scan_cache_ = std::make_unique<FakeHostScanCache>();
    fake_persistent_host_scan_cache_ =
        base::WrapUnique(new FakePersistentHostScanCache());

    host_scan_cache_ = std::make_unique<TopLevelHostScanCache>(
        base::WrapUnique(test_timer_factory_.get()), fake_active_host_.get(),
        fake_network_host_scan_cache_.get(),
        fake_persistent_host_scan_cache_.get());

    // To track what is expected to be contained in the cache, maintain a
    // FakeHostScanCache in memory and update it alongside |host_scan_cache_|.
    // Use a std::vector to track which device IDs correspond to devices whose
    // Tether networks' HasConnectedToHost fields are expected to be set.
    expected_cache_ = std::make_unique<FakeHostScanCache>();

    device_id_tether_network_guid_map_ =
        std::make_unique<DeviceIdTetherNetworkGuidMap>();
  }

  void FireTimer(const std::string& tether_network_guid) {
    ExtendedMockTimer* timer =
        test_timer_factory_
            ->tether_network_guid_to_timer_map()[tether_network_guid];
    ASSERT_TRUE(timer);
    timer->Fire();

    // If the device whose correlated timer has fired is not the active host, it
    // is expected to be removed from the cache.
    if (fake_active_host_->GetTetherNetworkGuid() != tether_network_guid) {
      expected_cache_->RemoveHostScanResult(tether_network_guid);
    }
  }

  void SetActiveHost(const std::string& tether_network_guid) {
    if (tether_network_guid.empty()) {
      fake_active_host_->SetActiveHostDisconnected();
    } else {
      fake_active_host_->SetActiveHostConnected(
          device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
              tether_network_guid),
          tether_network_guid, "wifiNetworkGuid");
    }
  }

  void SetHostScanResult(const HostScanCacheEntry& entry) {
    test_timer_factory_->set_tether_network_guid_for_next_timer(
        entry.tether_network_guid);
    host_scan_cache_->SetHostScanResult(entry);
    expected_cache_->SetHostScanResult(entry);
  }

  void RemoveHostScanResult(const std::string& tether_network_guid) {
    host_scan_cache_->RemoveHostScanResult(tether_network_guid);
    if (fake_active_host_->GetTetherNetworkGuid() != tether_network_guid)
      expected_cache_->RemoveHostScanResult(tether_network_guid);
  }

  // Verifies that the information present in |expected_cache_| mirrors what
  // |host_scan_cache_| has stored.
  void VerifyCacheContainsExpectedContents(size_t expected_size) {
    EXPECT_EQ(expected_size, expected_cache_->size());
    EXPECT_EQ(expected_size, fake_network_host_scan_cache_->size());
    EXPECT_EQ(expected_size, fake_persistent_host_scan_cache_->size());
    EXPECT_EQ(expected_cache_->GetTetherGuidsInCache(),
              host_scan_cache_->GetTetherGuidsInCache());

    for (auto& it : expected_cache_->cache()) {
      const std::string tether_network_guid = it.first;
      const HostScanCacheEntry& expected_entry = it.second;

      const HostScanCacheEntry* network_entry =
          fake_network_host_scan_cache_->GetCacheEntry(tether_network_guid);
      ASSERT_TRUE(network_entry);

      const HostScanCacheEntry* persistent_entry =
          fake_persistent_host_scan_cache_->GetCacheEntry(tether_network_guid);
      ASSERT_TRUE(persistent_entry);

      EXPECT_EQ(expected_entry.device_name, network_entry->device_name);
      EXPECT_EQ(expected_entry.device_name, persistent_entry->device_name);
      EXPECT_EQ(expected_entry.carrier, network_entry->carrier);
      EXPECT_EQ(expected_entry.carrier, persistent_entry->carrier);
      EXPECT_EQ(expected_entry.battery_percentage,
                network_entry->battery_percentage);
      EXPECT_EQ(expected_entry.battery_percentage,
                persistent_entry->battery_percentage);
      EXPECT_EQ(expected_entry.signal_strength, network_entry->signal_strength);
      EXPECT_EQ(expected_entry.signal_strength,
                persistent_entry->signal_strength);
      EXPECT_EQ(expected_entry.setup_required, network_entry->setup_required);
      EXPECT_EQ(expected_entry.setup_required,
                persistent_entry->setup_required);

      // Ensure that each entry has an actively-running Timer.
      ExtendedMockTimer* timer =
          test_timer_factory_
              ->tether_network_guid_to_timer_map()[tether_network_guid];
      ASSERT_TRUE(timer);
      EXPECT_TRUE(timer->IsRunning());
    }
  }

  const std::unordered_map<std::string, HostScanCacheEntry> test_entries_;

  raw_ptr<TestTimerFactory, DanglingUntriaged> test_timer_factory_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeHostScanCache> fake_network_host_scan_cache_;
  std::unique_ptr<FakePersistentHostScanCache> fake_persistent_host_scan_cache_;

  std::unique_ptr<FakeHostScanCache> expected_cache_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;

  std::unique_ptr<TopLevelHostScanCache> host_scan_cache_;
};

TEST_F(TopLevelHostScanCacheTest, TestSetScanResultsAndLetThemExpire) {
  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyCacheContainsExpectedContents(1u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid1));
  VerifyCacheContainsExpectedContents(2u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid2));
  VerifyCacheContainsExpectedContents(3u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid3));
  VerifyCacheContainsExpectedContents(4u /* expected_size */);

  FireTimer(host_scan_test_util::kTetherGuid0);
  VerifyCacheContainsExpectedContents(3u /* expected_size */);

  FireTimer(host_scan_test_util::kTetherGuid1);
  VerifyCacheContainsExpectedContents(2u /* expected_size */);

  FireTimer(host_scan_test_util::kTetherGuid2);
  VerifyCacheContainsExpectedContents(1u /* expected_size */);

  FireTimer(host_scan_test_util::kTetherGuid3);
  VerifyCacheContainsExpectedContents(0 /* expected_size */);
}

TEST_F(TopLevelHostScanCacheTest, TestSetScanResultThenUpdateAndRemove) {
  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyCacheContainsExpectedContents(1u /* expected_size */);

  // Change the fields for tether network with GUID |kTetherGuid0| to the
  // fields corresponding to |kTetherGuid1|.
  SetHostScanResult(
      *HostScanCacheEntry::Builder()
           .SetTetherNetworkGuid(host_scan_test_util::kTetherGuid0)
           .SetDeviceName(host_scan_test_util::kTetherDeviceName0)
           .SetCarrier(host_scan_test_util::kTetherCarrier1)
           .SetBatteryPercentage(host_scan_test_util::kTetherBatteryPercentage1)
           .SetSignalStrength(host_scan_test_util::kTetherSignalStrength1)
           .SetSetupRequired(host_scan_test_util::kTetherSetupRequired1)
           .Build());
  VerifyCacheContainsExpectedContents(1u /* expected_size */);

  // Now, remove that result.
  RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
  VerifyCacheContainsExpectedContents(0 /* expected_size */);
}

TEST_F(TopLevelHostScanCacheTest, TestSetScanResult_SetActiveHost) {
  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyCacheContainsExpectedContents(1u /* expected_size */);

  // Now, set the active host to be the device 0.
  SetActiveHost(host_scan_test_util::kTetherGuid0);

  // Attempt to remove the active host. This operation should fail since
  // removing the active host from the cache is not allowed.
  RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
  VerifyCacheContainsExpectedContents(1u /* expected_size */);

  // Fire the timer for the active host. Likewise, this should not result in the
  // cache entry being removed.
  FireTimer(host_scan_test_util::kTetherGuid0);
  VerifyCacheContainsExpectedContents(1u /* expected_size */);

  // Now, unset the active host.
  SetActiveHost("");

  // Removing the device should now succeed.
  RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
  EXPECT_TRUE(expected_cache_->empty());
  VerifyCacheContainsExpectedContents(0 /* expected_size */);
}

TEST_F(TopLevelHostScanCacheTest, TestRecoversFromCrashAndCleansUpWhenDeleted) {
  // Delete the cache that was initialized in SetUp(). This test requires extra
  // setup before initialization.
  host_scan_cache_.reset();

  // Add results for GUIDs 0 and 1 to the persistent cache.
  fake_persistent_host_scan_cache_->SetHostScanResult(
      test_entries_.at(host_scan_test_util::kTetherGuid0));
  fake_persistent_host_scan_cache_->SetHostScanResult(
      test_entries_.at(host_scan_test_util::kTetherGuid1));

  // These results are expected to be in the top-level cache after it is
  // created.
  expected_cache_->SetHostScanResult(
      test_entries_.at(host_scan_test_util::kTetherGuid0));
  expected_cache_->SetHostScanResult(
      test_entries_.at(host_scan_test_util::kTetherGuid1));

  // Alert the timer factory that these GUIDs will be added. To ensure that the
  // timer GUID is set in the correct order, iterate through the stored cache
  // entries to mimic the iteration order performed in TopLevelHostScanCache.
  // See crbug.com/750342.
  test_timer_factory_ = new TestTimerFactory();
  std::unordered_map<std::string, HostScanCacheEntry> persisted_entries =
      fake_persistent_host_scan_cache_->GetStoredCacheEntries();
  for (const auto& it : persisted_entries)
    test_timer_factory_->set_tether_network_guid_for_next_timer(it.first);

  // Create the top-level cache. It should have automatically picked up the
  // persisted scan results, even though they were not explicitly added.
  host_scan_cache_ = std::make_unique<TopLevelHostScanCache>(
      base::WrapUnique(test_timer_factory_.get()), fake_active_host_.get(),
      fake_network_host_scan_cache_.get(),
      fake_persistent_host_scan_cache_.get());
  VerifyCacheContainsExpectedContents(2u /* expected_size */);

  // Test that the timers are still valid for the scan results that were added
  // at startup by firing the timer for GUID 0.
  FireTimer(host_scan_test_util::kTetherGuid0);
  VerifyCacheContainsExpectedContents(1u /* expected_size */);

  // Now, only GUID 1 should be present.
  EXPECT_TRUE(
      host_scan_cache_->ExistsInCache(host_scan_test_util::kTetherGuid1));

  // Now, delete the top-level cache. It should result in the sub-caches being
  // cleared. This verifies that the persistent cache is cleared when logging
  // out (i.e., when the Tether component is shut down without a crash).
  host_scan_cache_.reset();
  EXPECT_TRUE(fake_network_host_scan_cache_->empty());
  EXPECT_TRUE(fake_persistent_host_scan_cache_->empty());
}

}  // namespace tether

}  // namespace ash
