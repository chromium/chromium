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
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/fake_local_device_info_provider.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
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

// A gMock matcher to verify the integer value inside a TimestampedPrefValue.
MATCHER_P(HasIntValue, expected_value, "") {
  if (!arg.value.is_int()) {
    *result_listener << "whose 'value' is not an integer (" << arg.value << ")";
    return false;
  }
  const int actual_value = arg.value.GetInt();
  if (actual_value != expected_value) {
    *result_listener << "whose 'value' is " << actual_value;
    return false;
  }
  return true;
}

// A gMock matcher to verify both the integer value and the last observed change
// time inside a TimestampedPrefValue.
MATCHER_P2(IsTimestampedPrefValue, expected_value, expected_time, "") {
  if (!arg.value.is_int()) {
    *result_listener << "whose 'value' is not an integer (" << arg.value << ")";
    return false;
  }
  const int actual_value = arg.value.GetInt();
  if (actual_value != expected_value) {
    *result_listener << "whose 'value' is " << actual_value;
    return false;
  }
  if (arg.last_observed_change_time != expected_time) {
    *result_listener << "whose 'last_observed_change_time' is "
                     << arg.last_observed_change_time;
    return false;
  }
  return true;
}

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

  std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
      const std::string& guid,
      const std::string& name = "name",
      const std::optional<syncer::DeviceInfo::SharingInfo>& sharing_info =
          std::nullopt,
      sync_pb::SyncEnums_DeviceType device_type =
          sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      syncer::DeviceInfo::OsType os_type = syncer::DeviceInfo::OsType::kLinux,
      syncer::DeviceInfo::FormFactor form_factor =
          syncer::DeviceInfo::FormFactor::kDesktop,
      const std::string& manufacturer_name = "manufacturer",
      const std::string& model_name = "model",
      const std::string& full_hardware_class = std::string(),
      base::Time last_updated_timestamp = base::Time::Now()) {
    return std::make_unique<syncer::DeviceInfo>(
        guid, name, "chrome_version", "user_agent", device_type, os_type,
        form_factor, "device_id", manufacturer_name, model_name,
        full_hardware_class, last_updated_timestamp,
        syncer::DeviceInfoUtil::GetPulseInterval(),
        /*send_tab_to_self_receiving_enabled=*/
        false,
        sync_pb::
            SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
        sharing_info,
        /*paask_info=*/std::nullopt,
        /*fcm_registration_token=*/std::string(),
        /*interested_data_types=*/syncer::DataTypeSet(),
        /*auto_sign_out_last_signin_timestamp=*/std::nullopt);
  }

  // Helper to create a fake `DeviceInfo` for testing filters.
  std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfo(
      const std::string& guid,
      syncer::DeviceInfo::OsType os_type,
      syncer::DeviceInfo::FormFactor form_factor) {
    return CreateFakeDeviceInfo(guid, "Device Name", std::nullopt,
                                sync_pb::SyncEnums::TYPE_UNSET, os_type,
                                form_factor);
  }

  // Helper to manually populate the cross-device dictionary pref.
  // This simulates data synced from a remote device, bypassing local tracking
  // logic.
  void InjectCrossDevicePrefEntry(const std::string& cross_device_pref_name,
                                  const std::string& cache_guid,
                                  const base::Value& value,
                                  base::Time update_time,
                                  std::optional<base::Time> observed_time) {
    ScopedDictPrefUpdate update(&profile_prefs_, cross_device_pref_name);
    base::Value::Dict entry;
    entry.Set(kValueKey, value.Clone());
    entry.Set(kUpdateTimeKey, base::TimeToValue(update_time));
    if (observed_time.has_value()) {
      entry.Set(kLastObservedChangeTimeKey,
                base::TimeToValue(observed_time.value()));
    }
    update->Set(cache_guid, std::move(entry));
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

// Verifies basic functionality of GetValues(), including correct sorting by
// recency (update time) and projection of timestamps.
TEST_F(CrossDevicePrefTrackerTest,
       GetValuesSortsByRecencyAndHandlesTimestamps) {
  CreateTracker();

  const base::Time kTime1 = base::Time::Now() + base::Seconds(10);
  const base::Time kTime2 = base::Time::Now() + base::Seconds(20);
  const base::Time kTime3 = base::Time::Now() + base::Seconds(30);

  // Add devices to the tracker (required for entries to be processed).
  GetTracker()->Add(CreateDeviceInfo("guid1",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid2",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid3",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));

  // Populate the cross-device pref.
  // Device 1: Value 100, Update Time 1, Observed Time 1 (Observed change)
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid1", base::Value(100),
                             kTime1, kTime1);
  // Device 2: Value 200, Update Time 3 (Newest), No observed time (Initial
  // sync)
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid2", base::Value(200),
                             kTime3, std::nullopt);
  // Device 3: Value 300, Update Time 2, Observed Time 1 (Stale observed time)
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid3", base::Value(300),
                             kTime2, kTime1);

  // Query using the tracked pref name.
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, {});

  // Verify sorting (most recent update time first: T3, T2, T1) and correct
  // projection of values and timestamps.
  EXPECT_THAT(results,
              testing::ElementsAre(IsTimestampedPrefValue(200, base::Time()),
                                   IsTimestampedPrefValue(300, kTime1),
                                   IsTimestampedPrefValue(100, kTime1)));
}

// Verifies that GetMostRecentValue returns the single most recent entry.
TEST_F(CrossDevicePrefTrackerTest, GetMostRecentValue) {
  CreateTracker();

  const base::Time kTime1 = base::Time::Now() + base::Seconds(10);
  const base::Time kTime2 = base::Time::Now() + base::Seconds(20);  // Newest

  // Add devices.
  GetTracker()->Add(CreateDeviceInfo("guid1",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid2", syncer::DeviceInfo::OsType::kMac,
                                     syncer::DeviceInfo::FormFactor::kDesktop));

  // Add entries.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid1", base::Value(100),
                             kTime1, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid2", base::Value(200),
                             kTime2, kTime2);

  // Query the most recent value.
  std::optional<TimestampedPrefValue> result =
      tracker_->GetMostRecentValue(kTrackedProfilePref, {});

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value.GetInt(), 200);
  EXPECT_EQ(result->last_observed_change_time, kTime2);
}

// Verifies that GetValues can filter results based on OS Type.
TEST_F(CrossDevicePrefTrackerTest, GetValuesFiltersByOsType) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();

  // Add devices with different OS types.
  GetTracker()->Add(CreateDeviceInfo("guid_win1",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid_win2",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid_mac",
                                     syncer::DeviceInfo::OsType::kMac,
                                     syncer::DeviceInfo::FormFactor::kDesktop));

  // Add corresponding entries.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_win1",
                             base::Value(1), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_win2",
                             base::Value(2), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_mac",
                             base::Value(3), kNow, std::nullopt);

  // Filter for Windows devices.
  CrossDevicePrefTracker::DeviceFilter filter;
  filter.os_type = syncer::DeviceInfo::OsType::kWindows;
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, filter);

  // Should contain the two Windows devices. Since timestamps are identical,
  // the order is not guaranteed.
  EXPECT_THAT(results,
              testing::UnorderedElementsAre(HasIntValue(1), HasIntValue(2)));
}

// Verifies that GetValues can filter results based on Form Factor.
TEST_F(CrossDevicePrefTrackerTest, GetValuesFiltersByFormFactor) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();

  // Add devices with different form factors.
  GetTracker()->Add(CreateDeviceInfo("guid_phone",
                                     syncer::DeviceInfo::OsType::kAndroid,
                                     syncer::DeviceInfo::FormFactor::kPhone));
  GetTracker()->Add(CreateDeviceInfo("guid_tablet",
                                     syncer::DeviceInfo::OsType::kAndroid,
                                     syncer::DeviceInfo::FormFactor::kTablet));

  // Add corresponding entries.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_phone",
                             base::Value(1), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_tablet",
                             base::Value(2), kNow, std::nullopt);

  // Filter for Phone form factor.
  CrossDevicePrefTracker::DeviceFilter filter;
  filter.form_factor = syncer::DeviceInfo::FormFactor::kPhone;
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, filter);

  EXPECT_THAT(results, testing::ElementsAre(HasIntValue(1)));
}

// Verifies that GetValues can filter results using both OS Type and Form
// Factor.
TEST_F(CrossDevicePrefTrackerTest, GetValuesFiltersByOsTypeAndFormFactor) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();

  // Android Phone (Match)
  GetTracker()->Add(CreateDeviceInfo("guid_android_phone",
                                     syncer::DeviceInfo::OsType::kAndroid,
                                     syncer::DeviceInfo::FormFactor::kPhone));
  // Android Tablet (OS match only)
  GetTracker()->Add(CreateDeviceInfo("guid_android_tablet",
                                     syncer::DeviceInfo::OsType::kAndroid,
                                     syncer::DeviceInfo::FormFactor::kTablet));
  // iOS Phone (Form Factor match only)
  GetTracker()->Add(CreateDeviceInfo("guid_ios_phone",
                                     syncer::DeviceInfo::OsType::kIOS,
                                     syncer::DeviceInfo::FormFactor::kPhone));

  // Add corresponding entries.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_android_phone",
                             base::Value(1), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_android_tablet",
                             base::Value(2), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_ios_phone",
                             base::Value(3), kNow, std::nullopt);

  // Filter for Android Phones specifically.
  CrossDevicePrefTracker::DeviceFilter filter;
  filter.os_type = syncer::DeviceInfo::OsType::kAndroid;
  filter.form_factor = syncer::DeviceInfo::FormFactor::kPhone;
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, filter);

  EXPECT_THAT(results, testing::ElementsAre(HasIntValue(1)));
}

// Verifies that GetMostRecentValue respects filters.
TEST_F(CrossDevicePrefTrackerTest, GetMostRecentValueWithFilter) {
  CreateTracker();

  const base::Time kTime1 = base::Time::Now() + base::Seconds(10);  // Older
  const base::Time kTime2 = base::Time::Now() + base::Seconds(20);  // Newer

  // Add devices.
  GetTracker()->Add(CreateDeviceInfo("guid_win",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid_mac",
                                     syncer::DeviceInfo::OsType::kMac,
                                     syncer::DeviceInfo::FormFactor::kDesktop));

  // Windows entry (older, T1).
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_win",
                             base::Value(100), kTime1, std::nullopt);
  // Mac entry (newer, T2).
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_mac",
                             base::Value(200), kTime2, kTime2);

  // Filter for Windows. Even though the Mac entry is newer overall, the
  // Windows entry is the most recent one matching the filter.
  CrossDevicePrefTracker::DeviceFilter filter;
  filter.os_type = syncer::DeviceInfo::OsType::kWindows;
  std::optional<TimestampedPrefValue> result =
      tracker_->GetMostRecentValue(kTrackedProfilePref, filter);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value.GetInt(), 100);
}

// Verifies that GetValues gracefully handles Cache GUIDs present in the pref
// dictionary but missing from the DeviceInfoTracker (stale entries).
TEST_F(CrossDevicePrefTrackerTest, GetValuesIgnoresDevicesMissingFromTracker) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();

  // Add one known device.
  GetTracker()->Add(CreateDeviceInfo("guid_known",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));

  // Add entries for both a known and an unknown device.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_known",
                             base::Value(1), kNow, std::nullopt);
  // "guid_unknown" is not added to the DeviceInfoTracker.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_unknown",
                             base::Value(2), kNow, std::nullopt);

  // Query the values.
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, {});

  // The unknown device should be filtered out, leaving only the known one.
  EXPECT_THAT(results, testing::ElementsAre(HasIntValue(1)));
}

// Verifies that GetValues gracefully handles entries in the dictionary that
// have invalid formats (missing keys, wrong types).
TEST_F(CrossDevicePrefTrackerTest, GetValuesHandlesInvalidDictionaryEntries) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();

  // Add devices corresponding to the test entries. They must be known to the
  // tracker; otherwise, they are filtered out before parsing is attempted.
  GetTracker()->Add(CreateDeviceInfo("guid1",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid2",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid3",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  GetTracker()->Add(CreateDeviceInfo("guid4",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));

  // Manually add entries with various invalid formats.
  ScopedDictPrefUpdate update(&profile_prefs_, kCrossDeviceProfilePref);
  base::Value::Dict& dict = update.Get();

  // Entry 1: Valid (control case).
  base::Value::Dict entry1;
  entry1.Set(kValueKey, 1);
  entry1.Set(kUpdateTimeKey, base::TimeToValue(kNow));
  dict.Set("guid1", std::move(entry1));

  // Entry 2: Missing 'value' key.
  base::Value::Dict entry2;
  entry2.Set(kUpdateTimeKey, base::TimeToValue(kNow));
  dict.Set("guid2", std::move(entry2));

  // Entry 3: Invalid 'update_time' format.
  base::Value::Dict entry3;
  entry3.Set(kValueKey, 3);
  entry3.Set(kUpdateTimeKey, "invalid_time");
  dict.Set("guid3", std::move(entry3));

  // Entry 4: Entry itself is not a dictionary.
  dict.Set("guid4", base::Value("not_a_dict"));

  // Query the values.
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, {});

  // Only the valid entry should be returned.
  EXPECT_THAT(results, testing::ElementsAre(HasIntValue(1)));
}

// Verifies that the Query API returns empty results when the pref is empty,
// untracked, or non-existent.
TEST_F(CrossDevicePrefTrackerTest, GetValuesReturnsEmptyForEmptyOrUntracked) {
  CreateTracker();

  // Case 1: Empty dictionary.
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, {});
  EXPECT_TRUE(results.empty());
  EXPECT_FALSE(
      tracker_->GetMostRecentValue(kTrackedProfilePref, {}).has_value());

  // Case 2: Untracked pref (kUntrackedPref's cross-device counterpart is not
  // registered).
  results = tracker_->GetValues(kUntrackedPref, {});
  EXPECT_TRUE(results.empty());

  // Case 3: Non-existent pref.
  results = tracker_->GetValues("non.existent.pref", {});
  EXPECT_TRUE(results.empty());
}

// Verifies that GetMostRecentValue returns std::nullopt if entries exist but
// none match the filter.
TEST_F(CrossDevicePrefTrackerTest,
       GetMostRecentValueReturnsNulloptWhenFilteredOut) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();

  // Add a Windows device.
  GetTracker()->Add(CreateDeviceInfo("guid_win",
                                     syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop));
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, "guid_win",
                             base::Value(100), kNow, std::nullopt);

  // Filter for Mac (which doesn't exist).
  CrossDevicePrefTracker::DeviceFilter filter;
  filter.os_type = syncer::DeviceInfo::OsType::kMac;
  std::optional<TimestampedPrefValue> result =
      tracker_->GetMostRecentValue(kTrackedProfilePref, filter);
  EXPECT_FALSE(result.has_value());
}

}  // namespace
}  // namespace sync_preferences
