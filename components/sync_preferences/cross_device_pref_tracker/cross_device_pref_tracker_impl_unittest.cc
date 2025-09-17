// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/json/values_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_preferences {

namespace {

// Define constants used in the implementation for validation.
constexpr char kValueKey[] = "value";
constexpr char kUpdateTimeKey[] = "update_time";
constexpr char kLastObservedChangeTimeKey[] = "last_observed_change_time";

// Define test preferences.
const char kTrackedProfilePref[] = "tracked.profile.pref";
const char kTrackedLocalStatePref[] = "tracked.local_state.pref";
const char kUntrackedPref[] = "untracked.pref";

// Cross-device prefs names (must match implementation: prefix "cross_device.").
const char kCrossDeviceProfilePref[] = "cross_device.tracked.profile.pref";
const char kCrossDeviceLocalStatePref[] =
    "cross_device.tracked.local_state.pref";

const char kLocalCacheGuid[] = "id";

// A fake `CrossDevicePrefProvider` for controlling which prefs are tracked.
class FakeCrossDevicePrefProvider : public CrossDevicePrefProvider {
 public:
  FakeCrossDevicePrefProvider() {
    profile_prefs_.insert(kTrackedProfilePref);
    local_state_prefs_.insert(kTrackedLocalStatePref);
  }
  ~FakeCrossDevicePrefProvider() override = default;

  const base::flat_set<std::string_view>& GetProfilePrefs() const override {
    return profile_prefs_;
  }

  const base::flat_set<std::string_view>& GetLocalStatePrefs() const override {
    return local_state_prefs_;
  }

 private:
  base::flat_set<std::string_view> profile_prefs_;
  base::flat_set<std::string_view> local_state_prefs_;
};

class CrossDevicePrefTrackerTest : public testing::Test {
 public:
  CrossDevicePrefTrackerTest() {
    RegisterProfilePrefs(profile_prefs_.registry());
    RegisterLocalStatePrefs(local_state_prefs_.registry());
    // By default, `DeviceInfo` is ready. Tests requiring delayed initialization
    // will call `ResetLocalDeviceInfo()`.
    InitializeLocalDeviceInfo();
  }

  ~CrossDevicePrefTrackerTest() override {
    if (tracker_) {
      // `KeyedService` requires explicit shutdown.
      tracker_->Shutdown();
    }
  }

  // Returns the internal `FakeLocalDeviceInfoProvider` used by the fake
  // service.
  syncer::FakeLocalDeviceInfoProvider* GetLocalProvider() {
    return device_info_sync_service_.GetLocalDeviceInfoProvider();
  }

  // Returns the internal `FakeDeviceInfoTracker` used by the fake service.
  syncer::FakeDeviceInfoTracker* GetTracker() {
    return device_info_sync_service_.GetDeviceInfoTracker();
  }

  void InitializeLocalDeviceInfo() {
    // Mark the provider as ready.
    GetLocalProvider()->SetReady(true);
    // Add the local device to the tracker's list of all devices.
    GetTracker()->Add(GetLocalProvider()->GetLocalDeviceInfo());
    // Tell the tracker which device in its list is the local one.
    GetTracker()->SetLocalCacheGuid(kLocalCacheGuid);
  }

  void ResetLocalDeviceInfo() {
    const syncer::DeviceInfo* local_device_info_to_remove =
        GetLocalProvider()->GetLocalDeviceInfo();
    // Mark the provider as not ready, which makes `GetLocalDeviceInfo()` return
    // nullptr.
    GetLocalProvider()->SetReady(false);
    // Clear the provider's internal data.
    GetLocalProvider()->Clear();
    // If the device was previously added to the tracker, remove it.
    if (local_device_info_to_remove) {
      GetTracker()->Remove(local_device_info_to_remove);
    }
  }

  void CreateTracker() {
    tracker_ = std::make_unique<CrossDevicePrefTrackerImpl>(
        &profile_prefs_, &local_state_prefs_, &device_info_sync_service_,
        std::make_unique<FakeCrossDevicePrefProvider>());
  }

  void RegisterProfilePrefs(scoped_refptr<PrefRegistrySimple> registry) {
    registry->RegisterIntegerPref(kTrackedProfilePref, 0);
    registry->RegisterIntegerPref(kUntrackedPref, 0);
    registry->RegisterDictionaryPref(kCrossDeviceProfilePref);
    registry->RegisterDictionaryPref(kCrossDeviceLocalStatePref);
  }

  void RegisterLocalStatePrefs(scoped_refptr<PrefRegistrySimple> registry) {
    registry->RegisterIntegerPref(kTrackedLocalStatePref, 0);
  }

  // Helper to retrieve the cross-device dictionary entry for a given pref and
  // device. Returns nullptr if the entry does not exist.
  const base::Value::Dict* GetCrossDevicePrefEntry(
      const std::string& cross_device_pref_name,
      const std::string& cache_guid) {
    const base::Value::Dict& dict =
        profile_prefs_.GetDict(cross_device_pref_name);
    return dict.FindDict(cache_guid);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_prefs_;
  syncer::FakeDeviceInfoSyncService device_info_sync_service_;
  std::unique_ptr<CrossDevicePrefTrackerImpl> tracker_;
};

// Verifies that when the tracker is initialized with `DeviceInfo` already
// available, it performs an initial sync of all tracked preferences.
TEST_F(CrossDevicePrefTrackerTest,
       InitializesAndSyncsPrefsWhenDeviceInfoIsReady) {
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);
  local_state_prefs_.SetInteger(kTrackedLocalStatePref, 20);

  CreateTracker();

  // Verify that the initial values are synced to the cross-device prefs.
  // These are initial syncs, so they should not have an observed timestamp.
  const base::Value::Dict* profile_pref_entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(profile_pref_entry, nullptr);
  EXPECT_EQ(profile_pref_entry->FindInt(kValueKey), 10);
  EXPECT_TRUE(profile_pref_entry->contains(kUpdateTimeKey));
  EXPECT_FALSE(profile_pref_entry->contains(kLastObservedChangeTimeKey));

  const base::Value::Dict* local_state_pref_entry =
      GetCrossDevicePrefEntry(kCrossDeviceLocalStatePref, kLocalCacheGuid);
  ASSERT_NE(local_state_pref_entry, nullptr);
  EXPECT_EQ(local_state_pref_entry->FindInt(kValueKey), 20);
  EXPECT_TRUE(local_state_pref_entry->contains(kUpdateTimeKey));
  EXPECT_FALSE(local_state_pref_entry->contains(kLastObservedChangeTimeKey));
}

// Verifies that a local change to a tracked profile pref is observed and synced
// to the cross-device dictionary with an observed timestamp. Also verifies that
// setting the pref back to default removes the entry.
TEST_F(CrossDevicePrefTrackerTest,
       SyncsProfilePrefChangeWithObservedTimestamp) {
  CreateTracker();

  // Initial state (default value 0).
  const base::Value::Dict* initial_entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(initial_entry, nullptr);

  // Change the tracked pref to a non-default value, which should trigger the
  // `PrefChangeRegistrar`.
  profile_prefs_.SetInteger(kTrackedProfilePref, 50);

  // Verify the change is propagated with both timestamps.
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 50);
  EXPECT_TRUE(entry->contains(kUpdateTimeKey));
  EXPECT_TRUE(entry->contains(kLastObservedChangeTimeKey));

  // Verify timestamps are roughly equal (set nearly simultaneously).
  std::optional<base::Time> update_time =
      base::ValueToTime(entry->Find(kUpdateTimeKey));
  std::optional<base::Time> observed_time =
      base::ValueToTime(entry->Find(kLastObservedChangeTimeKey));
  EXPECT_EQ(update_time, observed_time);

  // Change the pref back to its default value.
  profile_prefs_.ClearPref(kTrackedProfilePref);

  // Verify the entry is now removed from the cross-device dictionary.
  const base::Value::Dict* cleared_entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(cleared_entry, nullptr);
}

// Verifies that a local change to a tracked local state pref is observed and
// synced to the cross-device dictionary with an observed timestamp.
TEST_F(CrossDevicePrefTrackerTest,
       SyncsLocalStatePrefChangeWithObservedTimestamp) {
  CreateTracker();
  local_state_prefs_.SetInteger(kTrackedLocalStatePref, 60);
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceLocalStatePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 60);
  EXPECT_TRUE(entry->contains(kUpdateTimeKey));
  EXPECT_TRUE(entry->contains(kLastObservedChangeTimeKey));
}

// Verifies that changes to a pref not registered with the provider are ignored.
TEST_F(CrossDevicePrefTrackerTest, IgnoresUntrackedPrefChange) {
  CreateTracker();
  profile_prefs_.SetInteger(kUntrackedPref, 70);

  // Ensure no unexpected writes occurred. The tracked pref entry should not
  // exist because it was initialized to its default value and never changed.
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(entry, nullptr);
}

// Verifies that if the tracker is initialized before `DeviceInfo` is ready, it
// waits until `DeviceInfo` is available and then performs the initial sync.
TEST_F(CrossDevicePrefTrackerTest, DelayedInitializationWaitsForDeviceInfo) {
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);
  ResetLocalDeviceInfo();
  CreateTracker();

  // Verify that nothing is written yet because the Cache GUID is missing.
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  // Make `DeviceInfo` ready. This should trigger the tracker to sync.
  InitializeLocalDeviceInfo();

  // Verify that the initial value is now synced.
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 10);
  EXPECT_TRUE(entry->contains(kUpdateTimeKey));
  EXPECT_FALSE(entry->contains(kLastObservedChangeTimeKey));
}

// Verifies that if a pref changes while `DeviceInfo` is not ready, the tracker
// syncs the latest value once `DeviceInfo` becomes available.
TEST_F(CrossDevicePrefTrackerTest, SyncsLatestValueAfterDelayedInitialization) {
  ResetLocalDeviceInfo();
  CreateTracker();

  // Change a pref while not ready. The tracker observes this internally but
  // cannot sync it yet.
  profile_prefs_.SetInteger(kTrackedProfilePref, 50);
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  // Make `DeviceInfo` ready. This should trigger the tracker to sync.
  InitializeLocalDeviceInfo();

  // Verify the latest value (50) is synced. This is considered a refresh, not
  // an observed change.
  const base::Value::Dict* entry1 =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry1, nullptr);
  EXPECT_EQ(entry1->FindInt(kValueKey), 50);
  EXPECT_TRUE(entry1->contains(kUpdateTimeKey));
  EXPECT_FALSE(entry1->contains(kLastObservedChangeTimeKey));

  // Verify subsequent changes are correctly handled as observed changes.
  profile_prefs_.SetInteger(kTrackedProfilePref, 55);
  const base::Value::Dict* entry2 =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry2, nullptr);
  EXPECT_EQ(entry2->FindInt(kValueKey), 55);
  EXPECT_TRUE(entry2->contains(kUpdateTimeKey));
  EXPECT_TRUE(entry2->contains(kLastObservedChangeTimeKey));
}

// Verifies that after `Shutdown()` is called, the tracker no longer observes or
// syncs pref changes.
TEST_F(CrossDevicePrefTrackerTest, ShutdownStopsTrackingChanges) {
  CreateTracker();
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);
  const base::Value::Dict* entry1 =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry1, nullptr);
  EXPECT_EQ(entry1->FindInt(kValueKey), 10);
  EXPECT_TRUE(entry1->contains(kUpdateTimeKey));
  EXPECT_TRUE(entry1->contains(kLastObservedChangeTimeKey));

  tracker_->Shutdown();

  // This change should be ignored.
  profile_prefs_.SetInteger(kTrackedProfilePref, 100);

  // The value should remain at the state before shutdown.
  const base::Value::Dict* entry2 =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry2, nullptr);
  EXPECT_EQ(entry2->FindInt(kValueKey), 10);
  EXPECT_TRUE(entry2->contains(kUpdateTimeKey));
  EXPECT_TRUE(entry2->contains(kLastObservedChangeTimeKey));
}

// Verifies the optimization where a write is skipped if a pref's value has not
// changed during a refresh (e.g., a simulated restart).
TEST_F(CrossDevicePrefTrackerTest, SkipsWriteOnRefreshIfValueIsUnchanged) {
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);
  CreateTracker();

  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 10);
  EXPECT_TRUE(entry->contains(kUpdateTimeKey));
  EXPECT_FALSE(entry->contains(kLastObservedChangeTimeKey));
  const base::Value* initial_timestamp = entry->Find(kUpdateTimeKey);

  task_environment_.FastForwardBy(base::Seconds(5));

  // Simulate a refresh by recreating the tracker.
  tracker_->Shutdown();
  CreateTracker();

  // Verify the timestamp has NOT changed, indicating the write was skipped.
  const base::Value::Dict* updated_entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(updated_entry, nullptr);
  EXPECT_EQ(updated_entry->FindInt(kValueKey), 10);
  EXPECT_TRUE(updated_entry->contains(kUpdateTimeKey));
  EXPECT_FALSE(updated_entry->contains(kLastObservedChangeTimeKey));
  const base::Value* current_timestamp = updated_entry->Find(kUpdateTimeKey);
  EXPECT_EQ(*current_timestamp, *initial_timestamp);
}

// Verifies that an observed timestamp from a previous user action is preserved
// during a subsequent refresh (e.g., restart) if the value is unchanged.
TEST_F(CrossDevicePrefTrackerTest,
       PreservesObservedTimestampOnRefreshIfValueIsUnchanged) {
  CreateTracker();
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);  // Observed change.

  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 10);
  EXPECT_TRUE(entry->contains(kUpdateTimeKey));
  EXPECT_TRUE(entry->contains(kLastObservedChangeTimeKey));
  const base::Value* observed_timestamp =
      entry->Find(kLastObservedChangeTimeKey);

  // Simulate a restart.
  tracker_->Shutdown();
  task_environment_.FastForwardBy(base::Seconds(5));
  CreateTracker();  // This is a non-observed, initial sync.

  // Verify the observed timestamp was preserved.
  const base::Value::Dict* updated_entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(updated_entry, nullptr);
  EXPECT_EQ(updated_entry->FindInt(kValueKey), 10);
  EXPECT_TRUE(updated_entry->contains(kUpdateTimeKey));
  EXPECT_TRUE(updated_entry->contains(kLastObservedChangeTimeKey));
  const base::Value* current_observed_timestamp =
      updated_entry->Find(kLastObservedChangeTimeKey);
  EXPECT_EQ(*current_observed_timestamp, *observed_timestamp);
}

// Verifies that if a pref value changes during a refresh (e.g., simulating an
// offline change detected at startup), the previous observed timestamp is NOT
// carried over to the new value.
TEST_F(CrossDevicePrefTrackerTest,
       DoesNotPreserveObservedTimestampOnRefreshIfValueChanges) {
  CreateTracker();
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);  // Observed change.

  // Verify initial state has an observed timestamp.
  const base::Value::Dict* entry1 =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry1, nullptr);
  EXPECT_EQ(entry1->FindInt(kValueKey), 10);
  EXPECT_TRUE(entry1->contains(kUpdateTimeKey));
  EXPECT_TRUE(entry1->contains(kLastObservedChangeTimeKey));

  // Simulate an offline change and a restart.
  tracker_->Shutdown();
  // Change value while tracker is off.
  profile_prefs_.SetInteger(kTrackedProfilePref, 20);
  task_environment_.FastForwardBy(base::Seconds(5));
  CreateTracker();  // This is a non-observed, initial sync.

  // Verify the new value (20) is synced, but it should NOT have the observed
  // timestamp, as this specific value change was not observed live.
  const base::Value::Dict* entry2 =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry2, nullptr);
  EXPECT_EQ(entry2->FindInt(kValueKey), 20);
  EXPECT_TRUE(entry2->contains(kUpdateTimeKey));
  EXPECT_FALSE(entry2->contains(kLastObservedChangeTimeKey));
}

}  // namespace
}  // namespace sync_preferences
