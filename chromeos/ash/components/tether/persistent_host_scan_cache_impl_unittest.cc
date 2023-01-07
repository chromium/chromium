// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/persistent_host_scan_cache_impl.h"

#include <memory>
#include <unordered_map>

#include "chromeos/ash/components/tether/fake_host_scan_cache.h"
#include "chromeos/ash/components/tether/host_scan_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

class PersistentHostScanCacheImplTest : public testing::Test {
 public:
  PersistentHostScanCacheImplTest(const PersistentHostScanCacheImplTest&) =
      delete;
  PersistentHostScanCacheImplTest& operator=(
      const PersistentHostScanCacheImplTest&) = delete;

 protected:
  PersistentHostScanCacheImplTest()
      : test_entries_(host_scan_test_util::CreateTestEntries()) {}

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    PersistentHostScanCacheImpl::RegisterPrefs(test_pref_service_->registry());

    host_scan_cache_ =
        std::make_unique<PersistentHostScanCacheImpl>(test_pref_service_.get());
    expected_cache_ = std::make_unique<FakeHostScanCache>();
  }

  void SetHostScanResult(const HostScanCacheEntry& entry) {
    host_scan_cache_->SetHostScanResult(entry);
    expected_cache_->SetHostScanResult(entry);
    EXPECT_TRUE(host_scan_cache_->ExistsInCache(entry.tether_network_guid));
    EXPECT_TRUE(expected_cache_->ExistsInCache(entry.tether_network_guid));
  }

  void RemoveHostScanResult(const std::string& tether_network_guid) {
    host_scan_cache_->RemoveHostScanResult(tether_network_guid);
    expected_cache_->RemoveHostScanResult(tether_network_guid);
    EXPECT_FALSE(host_scan_cache_->ExistsInCache(tether_network_guid));
    EXPECT_FALSE(expected_cache_->ExistsInCache(tether_network_guid));
  }

  void VerifyPersistentCacheMatchesInMemoryCache(size_t expected_size) {
    std::unordered_map<std::string, HostScanCacheEntry> entries =
        host_scan_cache_->GetStoredCacheEntries();
    EXPECT_EQ(expected_size, entries.size());
    EXPECT_EQ(expected_size, expected_cache_->size());
    EXPECT_EQ(expected_cache_->cache(), entries);
    EXPECT_EQ(expected_cache_->GetTetherGuidsInCache(),
              host_scan_cache_->GetTetherGuidsInCache());
  }

  const std::unordered_map<std::string, HostScanCacheEntry> test_entries_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeHostScanCache> expected_cache_;

  std::unique_ptr<PersistentHostScanCacheImpl> host_scan_cache_;
};

TEST_F(PersistentHostScanCacheImplTest, TestSetAndRemove) {
  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyPersistentCacheMatchesInMemoryCache(1u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid1));
  VerifyPersistentCacheMatchesInMemoryCache(2u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid2));
  VerifyPersistentCacheMatchesInMemoryCache(3u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid3));
  VerifyPersistentCacheMatchesInMemoryCache(4u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
  VerifyPersistentCacheMatchesInMemoryCache(3u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid1);
  VerifyPersistentCacheMatchesInMemoryCache(2u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid2);
  VerifyPersistentCacheMatchesInMemoryCache(1u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid3);
  VerifyPersistentCacheMatchesInMemoryCache(0u /* expected_size */);
}

TEST_F(PersistentHostScanCacheImplTest, TestUpdate) {
  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyPersistentCacheMatchesInMemoryCache(1u /* expected_size */);
  EXPECT_EQ(host_scan_test_util::kTetherSetupRequired0,
            host_scan_cache_->DoesHostRequireSetup(
                host_scan_test_util::kTetherGuid0));

  // Update existing entry, including changing the "setup required" field.
  SetHostScanResult(
      *HostScanCacheEntry::Builder()
           .SetTetherNetworkGuid(host_scan_test_util::kTetherGuid0)
           .SetDeviceName(host_scan_test_util::kTetherDeviceName0)
           .SetCarrier(host_scan_test_util::kTetherCarrier1)
           .SetBatteryPercentage(host_scan_test_util::kTetherBatteryPercentage1)
           .SetSignalStrength(host_scan_test_util::kTetherSignalStrength1)
           .SetSetupRequired(host_scan_test_util::kTetherSetupRequired1)
           .Build());
  VerifyPersistentCacheMatchesInMemoryCache(1u /* expected_size */);
  EXPECT_EQ(host_scan_test_util::kTetherSetupRequired1,
            host_scan_cache_->DoesHostRequireSetup(
                host_scan_test_util::kTetherGuid0));
}

TEST_F(PersistentHostScanCacheImplTest, TestStoredPersistently) {
  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid0));
  VerifyPersistentCacheMatchesInMemoryCache(1u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid1));
  VerifyPersistentCacheMatchesInMemoryCache(2u /* expected_size */);

  // Now, delete the existing PersistentHostScanCacheImpl object. All of its
  // in-memory state will be cleaned up, but it should have stored the scanned
  // data persistently.
  host_scan_cache_.reset();

  // Create a new object.
  host_scan_cache_ =
      std::make_unique<PersistentHostScanCacheImpl>(test_pref_service_.get());

  // The new object should still access the stored scanned data.
  VerifyPersistentCacheMatchesInMemoryCache(2u /* expected_size */);

  // Make some changes - update one existing result, add a new one, and remove
  // an old one.
  SetHostScanResult(
      *HostScanCacheEntry::Builder()
           .SetTetherNetworkGuid(host_scan_test_util::kTetherGuid0)
           .SetDeviceName(host_scan_test_util::kTetherDeviceName0)
           .SetCarrier(host_scan_test_util::kTetherCarrier1)
           .SetBatteryPercentage(host_scan_test_util::kTetherBatteryPercentage1)
           .SetSignalStrength(host_scan_test_util::kTetherSignalStrength1)
           .SetSetupRequired(host_scan_test_util::kTetherSetupRequired1)
           .Build());
  VerifyPersistentCacheMatchesInMemoryCache(2u /* expected_size */);

  SetHostScanResult(test_entries_.at(host_scan_test_util::kTetherGuid2));
  VerifyPersistentCacheMatchesInMemoryCache(3u /* expected_size */);

  RemoveHostScanResult(host_scan_test_util::kTetherGuid1);
  VerifyPersistentCacheMatchesInMemoryCache(2u /* expected_size */);

  // Delete the current PersistentHostScanCacheImpl and create another new one.
  // It should still contain the same data.
  host_scan_cache_ =
      std::make_unique<PersistentHostScanCacheImpl>(test_pref_service_.get());
  VerifyPersistentCacheMatchesInMemoryCache(2u /* expected_size */);
}

}  // namespace tether

}  // namespace ash
