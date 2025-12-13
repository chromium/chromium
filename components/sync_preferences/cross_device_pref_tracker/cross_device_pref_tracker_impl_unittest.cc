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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/test_sync_service.h"
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

// Histogram name for service status at query time.
constexpr char kAvailabilityAtQueryHistogram[] =
    "Sync.CrossDevicePrefTracker.AvailabilityAtQuery";

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

// A gMock matcher to verify the integer value, the last observed change
// time, and the guid inside a TimestampedPrefValue.
MATCHER_P3(IsTimestampedPrefValue,
           expected_value,
           expected_time,
           expected_guid,
           "") {
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
  if (arg.device_sync_cache_guid != expected_guid) {
    *result_listener << "whose 'guid' is " << arg.device_sync_cache_guid;
    return false;
  }
  return true;
}

// A gMock matcher to verify the GUID of a syncer::DeviceInfo object.
MATCHER_P(HasGuid, expected_guid, "") {
  if (arg.guid() != expected_guid) {
    *result_listener << "whose GUID is " << arg.guid();
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

// A mock observer to verify remote pref change notifications.
class MockObserver : public CrossDevicePrefTracker::Observer {
 public:
  MOCK_METHOD(void,
              OnRemotePrefChanged,
              (std::string_view pref_name,
               const TimestampedPrefValue& value,
               const syncer::DeviceInfo& device_info),
              (override));
};

class CrossDevicePrefTrackerTest : public testing::Test {
 public:
  CrossDevicePrefTrackerTest() {
    RegisterProfilePrefs(profile_prefs_.registry());
    RegisterLocalStatePrefs(local_state_prefs_.registry());
    // By default, `DeviceInfo` is ready. Tests requiring delayed initialization
    // will call `ResetLocalDeviceInfo()`.
    InitializeLocalDeviceInfo();
    SetSyncEnabled(true);
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

    if (tracker_) {
      tracker_->OnDeviceInfoChange();
    }
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

  // Helper to configure the TestSyncService state.
  void SetSyncEnabled(bool active) {
    if (active) {
      test_sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
      test_sync_service_.SetAllowedByEnterprisePolicy(true);
      test_sync_service_.SetHasUnrecoverableError(false);
      test_sync_service_.ClearAuthError();
      test_sync_service_.SetMaxTransportState(
          syncer::SyncService::TransportState::ACTIVE);
      test_sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          {syncer::UserSelectableType::kPreferences});
    } else {
      test_sync_service_.SetSignedOut();
      test_sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false, {});
    }
  }

  // Helper to simulate the SyncService state changing and notifying observers.
  void ChangeSyncState(bool active) {
    SetSyncEnabled(active);
    if (tracker_) {
      // Notify the tracker of the state change.
      test_sync_service_.FireStateChanged();
    }
  }

  void CreateTracker(bool pass_sync_service = true) {
    tracker_ = std::make_unique<CrossDevicePrefTrackerImpl>(
        &profile_prefs_, &local_state_prefs_, &device_info_sync_service_,
        pass_sync_service ? &test_sync_service_ : nullptr,
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
      syncer::DeviceInfo::FormFactor form_factor,
      base::Time last_updated_timestamp = base::Time::Now()) {
    return CreateFakeDeviceInfo(guid, "Device Name", std::nullopt,
                                sync_pb::SyncEnums::TYPE_UNSET, os_type,
                                form_factor, "manufacturer", "model",
                                std::string(), last_updated_timestamp);
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
  syncer::TestSyncService test_sync_service_;
  std::unique_ptr<CrossDevicePrefTrackerImpl> tracker_;
};

// Verifies that when the tracker is initialized with `DeviceInfo` and Sync
// already available, it performs an initial sync of all tracked preferences.
TEST_F(CrossDevicePrefTrackerTest,
       InitializesAndSyncsPrefsWhenDeviceInfoAndSyncAreReady) {
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
// (Sync is active by default).
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

  auto expected_results =
      testing::ElementsAre(IsTimestampedPrefValue(200, base::Time(), "guid2"),
                           IsTimestampedPrefValue(300, kTime1, "guid3"),
                           IsTimestampedPrefValue(100, kTime1, "guid1"));

  // Query using the tracked pref name.
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, {});

  // Verify sorting (most recent update time first: T3, T2, T1) and correct
  // projection of values and timestamps.
  EXPECT_THAT(results, expected_results);

  // Query using the cross-device pref name and verify results are identical.
  std::vector<TimestampedPrefValue> results_cross_device =
      tracker_->GetValues(kCrossDeviceProfilePref, {});
  EXPECT_THAT(results_cross_device, expected_results);
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

  // Query the most recent value using the tracked pref name.
  std::optional<TimestampedPrefValue> result =
      tracker_->GetMostRecentValue(kTrackedProfilePref, {});

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->value.GetInt(), 200);
  EXPECT_EQ(result->last_observed_change_time, kTime2);
  EXPECT_EQ(result->device_sync_cache_guid, "guid2");

  // Query the most recent value using the cross-device pref name.
  std::optional<TimestampedPrefValue> result_cross_device =
      tracker_->GetMostRecentValue(kCrossDeviceProfilePref, {});

  ASSERT_TRUE(result_cross_device.has_value());
  EXPECT_EQ(result_cross_device->value.GetInt(), 200);
  EXPECT_EQ(result_cross_device->last_observed_change_time, kTime2);
  EXPECT_EQ(result_cross_device->device_sync_cache_guid, "guid2");
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

// Verifies that GetValues can filter results based on an active syncing period.
TEST_F(CrossDevicePrefTrackerTest, GetValuesFiltersByActiveSyncingPeriod) {
  CreateTracker();
  const base::Time kOldSync = base::Time::Now() - base::Days(365);
  const base::Time kRecentSync = base::Time::Now();
  // Device that has not connected to sync servers in a long time.
  GetTracker()->Add(CreateDeviceInfo(
      "guid_inactive_android_phone", syncer::DeviceInfo::OsType::kAndroid,
      syncer::DeviceInfo::FormFactor::kPhone, kOldSync));

  // Device that has recently connected to sync servers (Match).
  GetTracker()->Add(CreateDeviceInfo(
      "guid_syncing_android_phone", syncer::DeviceInfo::OsType::kAndroid,
      syncer::DeviceInfo::FormFactor::kPhone, kRecentSync));

  // Add corresponding entries.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref,
                             "guid_inactive_android_phone", base::Value(1),
                             kOldSync, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref,
                             "guid_syncing_android_phone", base::Value(2),
                             kOldSync, std::nullopt);

  // Filter for devices that have connected to the sync servers within a recency
  // window.
  CrossDevicePrefTracker::DeviceFilter filter;
  base::TimeDelta lastSyncRecency = base::Days(90);
  filter.max_sync_recency = lastSyncRecency;
  std::vector<TimestampedPrefValue> results =
      tracker_->GetValues(kTrackedProfilePref, filter);

  EXPECT_THAT(results, testing::ElementsAre(HasIntValue(2)));
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
  EXPECT_EQ(result->device_sync_cache_guid, "guid_win");
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

// Verifies that observers are notified when a remote device updates a pref
// value, provided the DeviceInfo is already available.
TEST_F(CrossDevicePrefTrackerTest,
       NotifiesObserverWhenRemoteDeviceUpdatesPrefValue) {
  CreateTracker();
  MockObserver mock_observer;
  tracker_->AddObserver(&mock_observer);

  const std::string kRemoteGuid = "remote_guid";
  const base::Time kUpdateTime = base::Time::Now() + base::Seconds(10);
  const int kNewValue = 100;

  // Add the remote device info.
  std::unique_ptr<syncer::DeviceInfo> remote_device =
      CreateDeviceInfo(kRemoteGuid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  GetTracker()->Add(remote_device.get());

  // Expect a notification when the remote update occurs.
  EXPECT_CALL(
      mock_observer,
      OnRemotePrefChanged(
          testing::StrEq(kTrackedProfilePref),
          // Value 100, No observed time (base::Time()).
          IsTimestampedPrefValue(kNewValue, base::Time(), kRemoteGuid),
          // Use testing::Ref to ensure the correct DeviceInfo is passed.
          testing::Ref(*remote_device)));

  // Simulate a remote update by injecting the entry directly into the pref
  // service. This triggers the `cross_device_pref_registrar_`.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid,
                             base::Value(kNewValue), kUpdateTime, std::nullopt);

  tracker_->RemoveObserver(&mock_observer);
}

// Verifies that observers are NOT notified when the local device removes
// (clears) a pref value (suppression of self-notification for removals).
TEST_F(CrossDevicePrefTrackerTest,
       DoesNotNotifyObserverWhenLocalDeviceRemovesPrefValue) {
  CreateTracker();
  MockObserver mock_observer;
  tracker_->AddObserver(&mock_observer);

  // 1. Set an initial non-default value locally.
  profile_prefs_.SetInteger(kTrackedProfilePref, 50);
  ASSERT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  // Ensure no notifications are sent for local removals.
  EXPECT_CALL(mock_observer, OnRemotePrefChanged).Times(0);

  // 2. Trigger a local removal (clear back to default). This updates the
  // cross-device pref dictionary synchronously by removing the local entry.
  // The implementation should identify this removal as local and suppress the
  // notification.
  profile_prefs_.ClearPref(kTrackedProfilePref);

  ASSERT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  tracker_->RemoveObserver(&mock_observer);
}

// Verifies that if a pref is removed remotely, but the corresponding
// DeviceInfo is no longer available (e.g., device removed from Sync),
// observers are NOT notified, as the source metadata is missing.
TEST_F(CrossDevicePrefTrackerTest,
       DoesNotNotifyObserverOfRemovalIfDeviceInfoIsMissing) {
  const std::string kRemoteGuid = "remote_guid";
  const base::Time kInitialTime = base::Time::Now();

  // 1. Initialize the state with an existing value from the remote device.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid,
                             base::Value(100), kInitialTime, std::nullopt);

  CreateTracker();
  MockObserver mock_observer;
  tracker_->AddObserver(&mock_observer);

  // Note: Do NOT add the DeviceInfo for kRemoteGuid to the tracker.

  // Expect no notification because the DeviceInfo is missing when the removal
  // is processed.
  EXPECT_CALL(mock_observer, OnRemotePrefChanged).Times(0);

  // 2. Simulate a remote removal.
  ScopedDictPrefUpdate update(&profile_prefs_, kCrossDeviceProfilePref);
  update->Remove(kRemoteGuid);

  // If DeviceInfo arrived later, it would only trigger notifications for
  // existing values (if any), not past removals.

  tracker_->RemoveObserver(&mock_observer);
}

// Verifies that if pref data arrives via Sync before the corresponding
// DeviceInfo, observers are notified later when the DeviceInfo becomes
// available.
TEST_F(CrossDevicePrefTrackerTest,
       NotifiesObserverWhenDeviceInfoArrivesAfterPrefData) {
  const std::string kRemoteGuid = "remote_guid";
  const base::Time kUpdateTime = base::Time::Now() + base::Seconds(10);
  const int kValue = 200;

  // 1. Initialize the tracker. DeviceInfo for kRemoteGuid is NOT present yet.
  CreateTracker();
  MockObserver mock_observer;
  tracker_->AddObserver(&mock_observer);

  // 2. Set expectation that no calls should occur when the data is injected.
  // Although the data will be cached, the corresponding DeviceInfo is missing.
  EXPECT_CALL(mock_observer, OnRemotePrefChanged).Times(0);

  // 3. Inject pref data. This simulates data arriving via Sync before
  // DeviceInfo.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid,
                             base::Value(kValue), kUpdateTime, std::nullopt);

  // 4. Simulate DeviceInfo becoming available.
  std::unique_ptr<syncer::DeviceInfo> remote_device =
      CreateDeviceInfo(kRemoteGuid, syncer::DeviceInfo::OsType::kLinux,
                       syncer::DeviceInfo::FormFactor::kDesktop);

  // Expect a notification now that the DeviceInfo is available, triggered by
  // HandleRemoteDeviceInfoChanges().
  EXPECT_CALL(mock_observer,
              OnRemotePrefChanged(
                  testing::StrEq(kTrackedProfilePref),
                  IsTimestampedPrefValue(kValue, base::Time(), kRemoteGuid),
                  testing::Ref(*remote_device)));

  // Adding the device triggers OnDeviceInfoChange().
  GetTracker()->Add(remote_device.get());

  tracker_->RemoveObserver(&mock_observer);
}

// Verifies that the tracker correctly notifies multiple observers for different
// preferences and handles observer removal.
TEST_F(CrossDevicePrefTrackerTest, HandlesMultipleObserversAndPrefs) {
  CreateTracker();
  MockObserver mock_observer1;
  MockObserver mock_observer2;
  tracker_->AddObserver(&mock_observer1);
  tracker_->AddObserver(&mock_observer2);

  const std::string kRemoteGuid = "remote_guid";
  const base::Time kTime1 = base::Time::Now() + base::Seconds(10);
  const base::Time kTime2 = base::Time::Now() + base::Seconds(20);

  // Add the remote device info.
  std::unique_ptr<syncer::DeviceInfo> remote_device =
      CreateDeviceInfo(kRemoteGuid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  GetTracker()->Add(remote_device.get());

  // Expect both observers to be notified for the first pref change.
  EXPECT_CALL(mock_observer1,
              OnRemotePrefChanged(
                  testing::StrEq(kTrackedProfilePref),
                  IsTimestampedPrefValue(100, base::Time(), kRemoteGuid),
                  testing::Ref(*remote_device)));
  EXPECT_CALL(mock_observer2,
              OnRemotePrefChanged(
                  testing::StrEq(kTrackedProfilePref),
                  IsTimestampedPrefValue(100, base::Time(), kRemoteGuid),
                  testing::Ref(*remote_device)));

  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid,
                             base::Value(100), kTime1, std::nullopt);

  // Remove one observer.
  tracker_->RemoveObserver(&mock_observer2);

  // Expect only the remaining observer to be notified for the second pref
  // change.
  EXPECT_CALL(
      mock_observer1,
      OnRemotePrefChanged(testing::StrEq(kTrackedLocalStatePref),
                          // Observed time included in this notification.
                          IsTimestampedPrefValue(200, kTime2, kRemoteGuid),
                          testing::Ref(*remote_device)));
  EXPECT_CALL(mock_observer2, OnRemotePrefChanged).Times(0);

  // Inject change for a different pref (LocalState).
  InjectCrossDevicePrefEntry(kCrossDeviceLocalStatePref, kRemoteGuid,
                             base::Value(200), kTime2, kTime2);

  // Ensure the remaining observer is also removed before destruction.
  tracker_->RemoveObserver(&mock_observer1);
}

// Verifies that observers are NOT notified if a remote update contains
// malformed or incomplete data.
TEST_F(CrossDevicePrefTrackerTest, DoesNotNotifyIfRemoteUpdateIsMalformed) {
  CreateTracker();
  MockObserver mock_observer;
  tracker_->AddObserver(&mock_observer);

  const std::string kRemoteGuid = "remote_guid";

  // Add the remote device info.
  std::unique_ptr<syncer::DeviceInfo> remote_device =
      CreateDeviceInfo(kRemoteGuid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  GetTracker()->Add(remote_device.get());

  // Ensure no notifications are sent for malformed data.
  EXPECT_CALL(mock_observer, OnRemotePrefChanged).Times(0);

  // Simulate a remote update with invalid data (e.g., missing timestamp).
  // Manually manipulate the dictionary as InjectCrossDevicePrefEntry enforces
  // the correct format.
  ScopedDictPrefUpdate update(&profile_prefs_, kCrossDeviceProfilePref);
  base::Value::Dict entry;
  entry.Set(kValueKey, base::Value(100));
  // Missing kUpdateTimeKey, which makes ParseCrossDevicePrefEntry() fail.
  update->Set(kRemoteGuid, std::move(entry));

  tracker_->RemoveObserver(&mock_observer);
}

// Verifies that the internal caching mechanism prevents duplicate notifications
// if the PrefService signals a change but the actual dictionary content remains
// the same.
TEST_F(CrossDevicePrefTrackerTest, DoesNotNotifyIfRemoteValueIsUnchanged) {
  CreateTracker();
  MockObserver mock_observer;
  tracker_->AddObserver(&mock_observer);

  const std::string kRemoteGuid = "remote_guid";
  const base::Time kUpdateTime = base::Time::Now() + base::Seconds(10);
  const int kValue = 100;

  // Add the remote device info.
  std::unique_ptr<syncer::DeviceInfo> remote_device =
      CreateDeviceInfo(kRemoteGuid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  GetTracker()->Add(remote_device.get());

  // Expect the first notification.
  EXPECT_CALL(mock_observer, OnRemotePrefChanged).Times(1);

  // First update.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid,
                             base::Value(kValue), kUpdateTime, std::nullopt);

  // Reset mocks to check for the second update.
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer, OnRemotePrefChanged).Times(0);

  // Second update with the exact same data. Injecting it again triggers the
  // PrefChangeRegistrar, but the tracker logic in OnCrossDevicePrefChanged()
  // should compare the new dictionary with the cached one and skip
  // notification.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid,
                             base::Value(kValue), kUpdateTime, std::nullopt);

  tracker_->RemoveObserver(&mock_observer);
}

// Verifies that if Sync is inactive during initialization, the initial sync
// of preferences does not happen.
TEST_F(CrossDevicePrefTrackerTest, DoesNotInitializeWhenSyncIsInactive) {
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);
  SetSyncEnabled(false);

  CreateTracker();

  // Verify that the initial values are NOT synced because Sync is inactive.
  const base::Value::Dict* profile_pref_entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(profile_pref_entry, nullptr);
}

// Verifies that local changes to tracked prefs are ignored when Sync is
// inactive.
TEST_F(CrossDevicePrefTrackerTest, IgnoresLocalChangesWhenSyncIsInactive) {
  SetSyncEnabled(false);
  CreateTracker();

  // Change the tracked pref.
  profile_prefs_.SetInteger(kTrackedProfilePref, 50);

  // Verify the change is NOT propagated.
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(entry, nullptr);
}

// Verifies that when Sync becomes active (via OnStateChanged), the tracker
// refreshes and pushes the current state of all tracked prefs.
TEST_F(CrossDevicePrefTrackerTest, RefreshesPrefsWhenSyncBecomesActive) {
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);
  SetSyncEnabled(false);
  CreateTracker();

  // Verify initial state (no sync).
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  // Simulate Sync becoming active.
  ChangeSyncState(true);

  // Verify that the current value (10) is now synced. This is a refresh, not
  // an observed change.
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 10);
  EXPECT_TRUE(entry->contains(kUpdateTimeKey));
  EXPECT_FALSE(entry->contains(kLastObservedChangeTimeKey));
}

// Verifies that if a pref changes while Sync is inactive, the tracker
// syncs the latest value once Sync becomes available.
TEST_F(CrossDevicePrefTrackerTest, SyncsLatestValueWhenSyncBecomesActive) {
  SetSyncEnabled(false);
  CreateTracker();

  // Change a pref while inactive.
  profile_prefs_.SetInteger(kTrackedProfilePref, 50);
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  // Simulate Sync becoming active.
  ChangeSyncState(true);

  // Verify the latest value (50) is synced.
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 50);
}

// Verifies behavior when the SyncService pointer is null (e.g., in tests or
// specific configurations). The tracker should initialize but not sync
// anything.
TEST_F(CrossDevicePrefTrackerTest, HandlesNullSyncService) {
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);

  // Create tracker passing nullptr for SyncService.
  CreateTracker(/*pass_sync_service=*/false);

  EXPECT_EQ(tracker_->sync_service(), nullptr);

  // Verify that no sync occurs.
  const base::Value::Dict* entry1 =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(entry1, nullptr);

  // Verify that local changes are also ignored.
  profile_prefs_.SetInteger(kTrackedProfilePref, 20);
  const base::Value::Dict* entry2 =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(entry2, nullptr);
}

// Verifies that if DeviceInfo is delayed AND Sync is inactive, initialization
// is correctly deferred until both are ready.
TEST_F(CrossDevicePrefTrackerTest, DelayedInitWaitsForDeviceInfoAndSync) {
  profile_prefs_.SetInteger(kTrackedProfilePref, 10);
  ResetLocalDeviceInfo();
  SetSyncEnabled(false);
  CreateTracker();

  // Nothing should be written.
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  // Make DeviceInfo ready. This triggers OnDeviceInfoChange ->
  // OnLocalDeviceInfoReady.
  InitializeLocalDeviceInfo();
  // Still shouldn't sync because Sync is inactive.
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  // Make Sync active. This triggers OnStateChanged ->
  // SyncAllOnDevicePrefsToCrossDevice. Now initialization should complete.
  ChangeSyncState(true);

  // Verify sync occurred.
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 10);
}

// Verifies that writes are suppressed if the transport state is active but the
// required data type (PREFERENCES) is not.
TEST_F(CrossDevicePrefTrackerTest, InactiveWhenDataTypeIsDisabled) {
  // Configure SyncService to be ACTIVE but without PREFERENCES.
  test_sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  test_sync_service_.SetAllowedByEnterprisePolicy(true);
  test_sync_service_.SetHasUnrecoverableError(false);
  test_sync_service_.ClearAuthError();
  test_sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);

  // Set selected types *without* kPreferences. This is the key part.
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kBookmarks});

  profile_prefs_.SetInteger(kTrackedProfilePref, 10);
  CreateTracker();
  test_sync_service_.FireStateChanged();  // Trigger initial check

  // Verify that the initial value is NOT synced.
  const base::Value::Dict* entry =
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(entry, nullptr);

  // Verify subsequent changes are also not synced.
  profile_prefs_.SetInteger(kTrackedProfilePref, 20);
  entry = GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  EXPECT_EQ(entry, nullptr);

  // Enable the data type by re-selecting it.
  test_sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kPreferences});
  // Notify the tracker that the configuration changed.
  test_sync_service_.FireStateChanged();

  // Verify the latest value is now synced.
  entry = GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->FindInt(kValueKey), 20);
}

// Verifies that entries corresponding to devices removed from DeviceInfoTracker
// are garbage collected from the cross-device preference dictionaries.
TEST_F(CrossDevicePrefTrackerTest, GarbageCollectsStaleCacheGuids) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();
  const std::string kGuidA = "guid_a";
  const std::string kGuidB = "guid_b";

  // 1. Add two remote devices.
  std::unique_ptr<syncer::DeviceInfo> device_a =
      CreateDeviceInfo(kGuidA, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  std::unique_ptr<syncer::DeviceInfo> device_b =
      CreateDeviceInfo(kGuidB, syncer::DeviceInfo::OsType::kMac,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  // Adding devices triggers OnDeviceInfoChange, updating
  // `active_device_guids_`.
  GetTracker()->Add(device_a.get());
  GetTracker()->Add(device_b.get());

  // 2. Inject pref entries for both. This updates the internal cache via
  // OnCrossDevicePrefChanged.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kGuidA, base::Value(100),
                             kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kGuidB, base::Value(200),
                             kNow, std::nullopt);

  // 3. Verify both entries exist.
  ASSERT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kGuidA), nullptr);
  ASSERT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kGuidB), nullptr);

  // 4. Remove Device B and manually trigger the device info change notification
  // to run GarbageCollectStaleCacheGuids().
  GetTracker()->Remove(device_b.get());
  tracker_->OnDeviceInfoChange();

  // 5. Verify Device A persists and Device B is removed.
  EXPECT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kGuidA), nullptr);
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kGuidB), nullptr);
}

// Verifies that garbage collection works correctly across multiple tracked
// preferences simultaneously.
TEST_F(CrossDevicePrefTrackerTest,
       GarbageCollectsStaleGuidsAcrossMultiplePrefs) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();
  const std::string kStaleGuid = "guid_stale";
  const std::string kActiveGuid = "guid_active";

  // Add devices.
  std::unique_ptr<syncer::DeviceInfo> stale_device =
      CreateDeviceInfo(kStaleGuid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  std::unique_ptr<syncer::DeviceInfo> active_device =
      CreateDeviceInfo(kActiveGuid, syncer::DeviceInfo::OsType::kMac,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  GetTracker()->Add(stale_device.get());
  GetTracker()->Add(active_device.get());

  // Inject entries for both devices in both tracked prefs.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kStaleGuid,
                             base::Value(1), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kActiveGuid,
                             base::Value(2), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceLocalStatePref, kStaleGuid,
                             base::Value(3), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceLocalStatePref, kActiveGuid,
                             base::Value(4), kNow, std::nullopt);

  // Verify initial state.
  ASSERT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kStaleGuid),
            nullptr);
  ASSERT_NE(GetCrossDevicePrefEntry(kCrossDeviceLocalStatePref, kStaleGuid),
            nullptr);

  // Remove the stale device.
  GetTracker()->Remove(stale_device.get());
  tracker_->OnDeviceInfoChange();

  // Verify stale entries are gone from both prefs.
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kStaleGuid),
            nullptr);
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceLocalStatePref, kStaleGuid),
            nullptr);

  // Verify active entries remain.
  EXPECT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kActiveGuid),
            nullptr);
  EXPECT_NE(GetCrossDevicePrefEntry(kCrossDeviceLocalStatePref, kActiveGuid),
            nullptr);
}

// Verifies that the removal of entries during garbage collection does not
// trigger OnRemotePrefChanged notifications.
TEST_F(CrossDevicePrefTrackerTest,
       GarbageCollectionDoesNotTriggerRemoteNotifications) {
  CreateTracker();
  MockObserver mock_observer;
  tracker_->AddObserver(&mock_observer);

  const std::string kRemoteGuid = "remote_guid";
  const base::Time kNow = base::Time::Now();

  // Add the remote device.
  std::unique_ptr<syncer::DeviceInfo> remote_device =
      CreateDeviceInfo(kRemoteGuid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  GetTracker()->Add(remote_device.get());

  // Inject an entry. Expect one notification (for the addition).
  EXPECT_CALL(mock_observer, OnRemotePrefChanged).Times(1);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid,
                             base::Value(100), kNow, std::nullopt);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  ASSERT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid),
            nullptr);

  // Remove the device. This triggers garbage collection.
  // Ensure no notifications are sent during this process. The implementation
  // of ProcessRemoteUpdates only iterates over the *new* dictionary to find
  // additions/updates, so removals (like garbage collection) shouldn't trigger
  // notifications.
  EXPECT_CALL(mock_observer, OnRemotePrefChanged).Times(0);
  GetTracker()->Remove(remote_device.get());
  tracker_->OnDeviceInfoChange();

  // Verify the entry is gone.
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid),
            nullptr);

  tracker_->RemoveObserver(&mock_observer);
}

// Verifies that the local device's entry is preserved during garbage collection
// as long as the local device remains active in the DeviceInfoTracker.
TEST_F(CrossDevicePrefTrackerTest, GarbageCollectionPreservesLocalDeviceEntry) {
  CreateTracker();
  // The local device (kLocalCacheGuid) is active by default in the test setup.

  // Set a local pref, creating an entry for the local device.
  profile_prefs_.SetInteger(kTrackedProfilePref, 50);
  EXPECT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  // Add and then remove a remote device to trigger OnDeviceInfoChange() and
  // garbage collection explicitly.
  const std::string kRemoteGuid = "remote_guid";
  std::unique_ptr<syncer::DeviceInfo> remote_device =
      CreateDeviceInfo(kRemoteGuid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);

  GetTracker()->Add(remote_device.get());
  // Inject an entry for the remote device to ensure garbage collection has
  // potential work.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid,
                             base::Value(100), base::Time::Now(), std::nullopt);

  // Remove the remote device, triggering garbage collection.
  GetTracker()->Remove(remote_device.get());
  tracker_->OnDeviceInfoChange();

  // Verify the remote entry is gone, but the local entry remains.
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kRemoteGuid),
            nullptr);
  EXPECT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);
}

// Verifies that multiple entries corresponding to devices removed from
// DeviceInfoTracker are garbage collected simultaneously in a single pass.
TEST_F(CrossDevicePrefTrackerTest,
       GarbageCollectsMultipleStaleCacheGuidsSimultaneously) {
  CreateTracker();
  const base::Time kNow = base::Time::Now();
  const std::string kActiveGuid = "guid_active";
  const std::string kStaleGuid1 = "guid_stale_1";
  const std::string kStaleGuid2 = "guid_stale_2";

  // 1. Add devices.
  std::unique_ptr<syncer::DeviceInfo> active_device =
      CreateDeviceInfo(kActiveGuid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  std::unique_ptr<syncer::DeviceInfo> stale_device1 =
      CreateDeviceInfo(kStaleGuid1, syncer::DeviceInfo::OsType::kMac,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  std::unique_ptr<syncer::DeviceInfo> stale_device2 =
      CreateDeviceInfo(kStaleGuid2, syncer::DeviceInfo::OsType::kLinux,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  GetTracker()->Add(active_device.get());
  GetTracker()->Add(stale_device1.get());
  GetTracker()->Add(stale_device2.get());

  // 2. Inject pref entries.
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kActiveGuid,
                             base::Value(1), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kStaleGuid1,
                             base::Value(2), kNow, std::nullopt);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kStaleGuid2,
                             base::Value(3), kNow, std::nullopt);

  // 3. Remove stale devices and trigger garbage collection once.
  GetTracker()->Remove(stale_device1.get());
  GetTracker()->Remove(stale_device2.get());
  tracker_->OnDeviceInfoChange();

  // 4. Verify results.
  EXPECT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kActiveGuid),
            nullptr);
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kStaleGuid1),
            nullptr);
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kStaleGuid2),
            nullptr);
}

// Verifies that garbage collection correctly handles the case where all entries
// for a preference are stale, resulting in an empty dictionary.
TEST_F(CrossDevicePrefTrackerTest, GarbageCollectsAllEntriesIfAllAreStale) {
  CreateTracker();
  // Ensure the local device doesn't have an entry initially for this pref
  // to allow the dictionary to become empty.
  ASSERT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, kLocalCacheGuid),
            nullptr);

  const base::Time kNow = base::Time::Now();
  const std::string kGuidA = "guid_a";

  // 1. Add device and pref entry.
  std::unique_ptr<syncer::DeviceInfo> device_a =
      CreateDeviceInfo(kGuidA, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop);
  GetTracker()->Add(device_a.get());
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, kGuidA, base::Value(100),
                             kNow, std::nullopt);

  ASSERT_FALSE(profile_prefs_.GetDict(kCrossDeviceProfilePref).empty());

  // 2. Remove device and trigger garbage collection.
  GetTracker()->Remove(device_a.get());
  tracker_->OnDeviceInfoChange();

  // 3. Verify the dictionary is now empty.
  EXPECT_TRUE(profile_prefs_.GetDict(kCrossDeviceProfilePref).empty());
}

// Verifies that the `AvailabilityAtQuery` histogram records `kAvailable` when
// the tracker is fully initialized and Sync is configured.
TEST_F(CrossDevicePrefTrackerTest, RecordsTrackerAvailabilityMetricAvailable) {
  // Setup: `DeviceInfo` ready (default), Sync enabled (default).
  CreateTracker();
  base::HistogramTester histogram_tester;

  tracker_->GetValues(kTrackedProfilePref, {});

  histogram_tester.ExpectUniqueSample(
      kAvailabilityAtQueryHistogram,
      CrossDevicePrefTrackerAvailabilityAtQuery::kAvailable, 1);

  tracker_->GetMostRecentValue(kTrackedProfilePref, {});

  histogram_tester.ExpectTotalCount(kAvailabilityAtQueryHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kAvailabilityAtQueryHistogram,
      CrossDevicePrefTrackerAvailabilityAtQuery::kAvailable, 2);
}

// Verifies that the metric records `kSyncNotConfigured` when `DeviceInfo` is
// ready but Sync is disabled.
TEST_F(CrossDevicePrefTrackerTest,
       RecordsTrackerAvailabilityMetricSyncNotConfigured) {
  // Setup: `DeviceInfo` ready (default), Sync disabled.
  SetSyncEnabled(false);
  CreateTracker();
  base::HistogramTester histogram_tester;

  tracker_->GetValues(kTrackedProfilePref, {});

  histogram_tester.ExpectUniqueSample(
      kAvailabilityAtQueryHistogram,
      CrossDevicePrefTrackerAvailabilityAtQuery::kSyncNotConfigured, 1);
}

// Verifies that the metric records `kLocalDeviceInfoMissing` when Sync is
// enabled but `DeviceInfo` initialization is delayed.
TEST_F(CrossDevicePrefTrackerTest,
       RecordsTrackerAvailabilityMetricLocalDeviceInfoMissing) {
  // Setup: `DeviceInfo` NOT ready, Sync enabled (default).
  ResetLocalDeviceInfo();
  CreateTracker();
  base::HistogramTester histogram_tester;

  tracker_->GetValues(kTrackedProfilePref, {});

  histogram_tester.ExpectUniqueSample(
      kAvailabilityAtQueryHistogram,
      CrossDevicePrefTrackerAvailabilityAtQuery::kLocalDeviceInfoMissing, 1);
}

// Verifies that the metric records the combined state when both `DeviceInfo` is
// missing and Sync is disabled.
TEST_F(
    CrossDevicePrefTrackerTest,
    RecordsTrackerAvailabilityMetricSyncNotConfiguredAndLocalDeviceInfoMissing) {
  // Setup: `DeviceInfo` NOT ready, Sync disabled.
  ResetLocalDeviceInfo();
  SetSyncEnabled(false);
  CreateTracker();
  base::HistogramTester histogram_tester;

  tracker_->GetValues(kTrackedProfilePref, {});

  histogram_tester.ExpectUniqueSample(
      kAvailabilityAtQueryHistogram,
      CrossDevicePrefTrackerAvailabilityAtQuery::
          kSyncNotConfiguredAndLocalDeviceInfoMissing,
      1);
}

// Verifies that the `AvailabilityAtQuery` histogram correctly reflects the
// status changing dynamically (e.g., initialization completing).
TEST_F(CrossDevicePrefTrackerTest,
       RecordsTrackerAvailabilityMetricStatusChange) {
  // Setup: Start with missing `DeviceInfo` (Sync enabled by default).
  ResetLocalDeviceInfo();
  CreateTracker();
  base::HistogramTester histogram_tester;

  tracker_->GetValues(kTrackedProfilePref, {});
  histogram_tester.ExpectBucketCount(
      kAvailabilityAtQueryHistogram,
      CrossDevicePrefTrackerAvailabilityAtQuery::kLocalDeviceInfoMissing, 1);

  InitializeLocalDeviceInfo();

  tracker_->GetValues(kTrackedProfilePref, {});
  histogram_tester.ExpectBucketCount(
      kAvailabilityAtQueryHistogram,
      CrossDevicePrefTrackerAvailabilityAtQuery::kAvailable, 1);

  histogram_tester.ExpectTotalCount(kAvailabilityAtQueryHistogram, 2);
}

// Verifies that devices that are already expired when the tracker is
// initialized are immediately garbage collected.
TEST_F(CrossDevicePrefTrackerTest,
       GarbageCollectsStaleEntriesFromInitiallyExpiredDevice) {
  // Set up an active device that should not be garbage collected.
  const base::Time recent_time = base::Time::Now() - base::Days(1);
  const std::string active_guid = "guid_active";
  GetTracker()->Add(
      CreateDeviceInfo(active_guid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop, recent_time));
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, active_guid,
                             base::Value(100), recent_time, std::nullopt);

  // Set up an expired device that should be garbage collected.
  const base::Time expired_time = base::Time::Now() - base::Days(15);
  const std::string expired_guid = "guid_expired";
  GetTracker()->Add(
      CreateDeviceInfo(expired_guid, syncer::DeviceInfo::OsType::kMac,
                       syncer::DeviceInfo::FormFactor::kDesktop, expired_time));
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, expired_guid,
                             base::Value(200), expired_time, std::nullopt);

  // Creating the tracker runs the initialization logic, which should identify
  // the expired device and garbage collect its entries.
  CreateTracker();

  EXPECT_NE(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, active_guid),
            nullptr);
  EXPECT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, expired_guid),
            nullptr);
}

// Verifies that a device that becomes expired over time has its entries
// garbage collected when device info changes.
TEST_F(CrossDevicePrefTrackerTest,
       GarbageCollectsStaleEntriesFromNewlyExpiredDevice) {
  CreateTracker();
  const base::Time start_time = base::Time::Now();

  // Set up the first device, which will eventually expire.
  const std::string should_expire_guid = "guid_expire";
  std::unique_ptr<syncer::DeviceInfo> device_to_expire =
      CreateDeviceInfo(should_expire_guid, syncer::DeviceInfo::OsType::kWindows,
                       syncer::DeviceInfo::FormFactor::kDesktop, start_time);
  GetTracker()->Add(device_to_expire.get());
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, should_expire_guid,
                             base::Value(100), start_time, std::nullopt);

  // Set up the second device, which will remain active.
  const std::string should_remain_guid = "guid_remain";
  std::unique_ptr<syncer::DeviceInfo> device_to_remain =
      CreateDeviceInfo(should_remain_guid, syncer::DeviceInfo::OsType::kMac,
                       syncer::DeviceInfo::FormFactor::kDesktop, start_time);
  GetTracker()->Add(device_to_remain.get());
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, should_remain_guid,
                             base::Value(200), start_time, std::nullopt);

  // Verify initial state.
  ASSERT_NE(
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, should_expire_guid),
      nullptr);
  ASSERT_NE(
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, should_remain_guid),
      nullptr);

  // Fast forward time, making the original timestamps stale.
  task_environment_.FastForwardBy(base::Days(15));

  // Simulate one device coming back online by updating its timestamp.
  const base::Time updated_time = base::Time::Now();
  std::unique_ptr<syncer::DeviceInfo> updated_device =
      CreateDeviceInfo(should_remain_guid, syncer::DeviceInfo::OsType::kMac,
                       syncer::DeviceInfo::FormFactor::kDesktop, updated_time);
  GetTracker()->Add(updated_device.get());

  // Trigger a device info change. The tracker should now see that
  // `device_to_expire` is stale, but `device_to_remain` is active.
  tracker_->OnDeviceInfoChange();

  EXPECT_EQ(
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, should_expire_guid),
      nullptr);
  EXPECT_NE(
      GetCrossDevicePrefEntry(kCrossDeviceProfilePref, should_remain_guid),
      nullptr);
}

// Verifies that if an expired device comes back online, it is correctly
// identified as a "new or reactivated" device and observers are notified.
TEST_F(CrossDevicePrefTrackerTest,
       NotifiesObserverForReactivatedExpiredDevice) {
  // Start with an expired device with an existing pref entry.
  const base::Time expired_time = base::Time::Now() - base::Days(15);
  const std::string guid = "guid_reactivated";
  GetTracker()->Add(CreateDeviceInfo(guid, syncer::DeviceInfo::OsType::kWindows,
                                     syncer::DeviceInfo::FormFactor::kDesktop,
                                     expired_time));
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, guid, base::Value(100),
                             expired_time, std::nullopt);

  // Initialize the tracker, which should garbage collect the expired entry.
  CreateTracker();
  ASSERT_EQ(GetCrossDevicePrefEntry(kCrossDeviceProfilePref, guid), nullptr);

  MockObserver mock_observer;
  tracker_->AddObserver(&mock_observer);

  // Simulate the device coming back online with a fresh timestamp and pref.
  const base::Time reactivated_time = base::Time::Now();
  std::unique_ptr<syncer::DeviceInfo> reactivated_device = CreateDeviceInfo(
      guid, syncer::DeviceInfo::OsType::kWindows,
      syncer::DeviceInfo::FormFactor::kDesktop, reactivated_time);
  InjectCrossDevicePrefEntry(kCrossDeviceProfilePref, guid, base::Value(200),
                             reactivated_time, std::nullopt);

  // Expect a notification because the device was not in the set of known
  // *active* devices, so it's treated as new.
  EXPECT_CALL(
      mock_observer,
      OnRemotePrefChanged(testing::StrEq(kTrackedProfilePref),
                          IsTimestampedPrefValue(200, base::Time(), guid),
                          testing::Ref(*reactivated_device)));

  GetTracker()->Add(reactivated_device.get());
  tracker_->RemoveObserver(&mock_observer);
}

// Verifies that cloning a `TimestampedPrefValue` returns a deep copy of the
// object.
TEST_F(CrossDevicePrefTrackerTest, CloningTimestampedValueReturnsDeepCopy) {
  TimestampedPrefValue original_value{
      .value = base::Value(5),
      .last_observed_change_time = base::Time::Now(),
      .device_sync_cache_guid = "guid",
  };

  TimestampedPrefValue cloned_value = original_value.Clone();

  ASSERT_EQ(original_value.value, cloned_value.value);
  ASSERT_EQ(original_value.last_observed_change_time,
            cloned_value.last_observed_change_time);
  ASSERT_EQ(original_value.device_sync_cache_guid,
            cloned_value.device_sync_cache_guid);

  // Memory should not be shared between the original and cloned values.
  EXPECT_NE(&original_value, &cloned_value);
  EXPECT_NE(&original_value.value, &cloned_value.value);
  EXPECT_NE(&original_value.last_observed_change_time,
            &cloned_value.last_observed_change_time);
  EXPECT_NE(&original_value.device_sync_cache_guid,
            &cloned_value.device_sync_cache_guid);
}

}  // namespace
}  // namespace sync_preferences
