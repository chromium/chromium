// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_prefs.h"

#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class DeviceInfoPrefsTest : public testing::Test {
 protected:
  DeviceInfoPrefsTest() : device_info_prefs_(&pref_service_, &clock_) {
    DeviceInfoPrefs::RegisterProfilePrefs(pref_service_.registry());
  }
  ~DeviceInfoPrefsTest() override = default;

  DeviceInfoPrefs device_info_prefs_;
  base::SimpleTestClock clock_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(DeviceInfoPrefsTest, ShouldMigrateFromObsoletePref) {
  const char kObsoleteDeviceInfoRecentGUIDs[] = "sync.local_device_guids";

  ListPrefUpdate cache_guids_update(&pref_service_,
                                    kObsoleteDeviceInfoRecentGUIDs);

  cache_guids_update->Insert(cache_guids_update->GetList().begin(),
                             base::Value("old_guid1"));
  cache_guids_update->Insert(cache_guids_update->GetList().begin(),
                             base::Value("old_guid2"));

  ASSERT_FALSE(device_info_prefs_.IsRecentLocalCacheGuid("old_guid1"));
  ASSERT_FALSE(device_info_prefs_.IsRecentLocalCacheGuid("old_guid2"));

  DeviceInfoPrefs::MigrateRecentLocalCacheGuidsPref(&pref_service_);

  EXPECT_TRUE(device_info_prefs_.IsRecentLocalCacheGuid("old_guid1"));
  EXPECT_TRUE(device_info_prefs_.IsRecentLocalCacheGuid("old_guid2"));
}

TEST_F(DeviceInfoPrefsTest, ShouldGarbageCollectExpiredCacheGuids) {
  const base::TimeDelta kMaxDaysLocalCacheGuidsStored =
      base::TimeDelta::FromDays(10);

  device_info_prefs_.AddLocalCacheGuid("guid1");
  clock_.Advance(kMaxDaysLocalCacheGuidsStored -
                 base::TimeDelta::FromMinutes(1));
  device_info_prefs_.AddLocalCacheGuid("guid2");

  // First garbage collection immediately before taking effect, hence a no-op.
  device_info_prefs_.GarbageCollectExpiredCacheGuids();
  EXPECT_TRUE(device_info_prefs_.IsRecentLocalCacheGuid("guid1"));
  EXPECT_TRUE(device_info_prefs_.IsRecentLocalCacheGuid("guid2"));

  // Advancing one day causes the first GUID to be garbage-collected.
  clock_.Advance(base::TimeDelta::FromDays(1));
  device_info_prefs_.GarbageCollectExpiredCacheGuids();
  EXPECT_FALSE(device_info_prefs_.IsRecentLocalCacheGuid("guid1"));
  EXPECT_TRUE(device_info_prefs_.IsRecentLocalCacheGuid("guid2"));
}

TEST_F(DeviceInfoPrefsTest, ShouldTruncateAfterMaximumNumberOfGuids) {
  const int kMaxLocalCacheGuidsStored = 30;

  device_info_prefs_.AddLocalCacheGuid("orig_guid");

  // Fill up exactly the maximum number, without triggering a truncation.
  for (int i = 0; i < kMaxLocalCacheGuidsStored - 1; i++) {
    device_info_prefs_.AddLocalCacheGuid(base::StringPrintf("guid%d", i));
  }

  EXPECT_TRUE(device_info_prefs_.IsRecentLocalCacheGuid("orig_guid"));

  // Adding one more should truncate exactly one.
  device_info_prefs_.AddLocalCacheGuid("newest_guid");
  EXPECT_FALSE(device_info_prefs_.IsRecentLocalCacheGuid("orig_guid"));
  EXPECT_TRUE(device_info_prefs_.IsRecentLocalCacheGuid("newest_guid"));
  EXPECT_TRUE(device_info_prefs_.IsRecentLocalCacheGuid("guid1"));
}

}  // namespace

}  // namespace syncer
