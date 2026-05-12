// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/synced_set_up/utils.h"

#include <optional>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

using ServiceStatus = ::sync_preferences::CrossDevicePrefTracker::ServiceStatus;

// Test implementation of `CrossDevicePrefTracker`.
class TestCrossDevicePrefTracker
    : public sync_preferences::CrossDevicePrefTracker {
 public:
  TestCrossDevicePrefTracker() = default;
  ~TestCrossDevicePrefTracker() override = default;

  // `KeyedService` overrides.
  void Shutdown() override {}

  // `CrossDevicePrefTracker` overrides.
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  ServiceStatus GetServiceStatus() const override { return service_status_; }

  std::vector<sync_preferences::TimestampedPrefValue> GetValues(
      std::string_view pref_name,
      const DeviceFilter& filter) const override {
    auto it = pref_values_.find(pref_name);
    if (it == pref_values_.end()) {
      return {};
    }

    std::vector<sync_preferences::TimestampedPrefValue> result;
    for (const auto& timestamped_value : it->second) {
      sync_preferences::TimestampedPrefValue copied_value =
          timestamped_value.Clone();
      result.push_back(std::move(copied_value));
    }
    return result;
  }

  std::optional<sync_preferences::TimestampedPrefValue> GetMostRecentValue(
      std::string_view pref_name,
      const DeviceFilter& filter) const override {
    return std::nullopt;
  }

  // Testing Method for injecting pref values into the tracker.
  void AddSyncedPrefValue(std::string_view pref_name,
                          sync_preferences::TimestampedPrefValue& value) {
    pref_values_[pref_name].push_back(std::move(value));
  }

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override {
    return base::android::ScopedJavaLocalRef<jobject>();
  }

  int GetServiceStatus(JNIEnv* env) const override {
    return static_cast<int>(GetServiceStatus());
  }

  base::android::ScopedJavaLocalRef<jobjectArray> GetValues(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& pref_name,
      std::optional<int> os_type,
      std::optional<int> form_factor,
      std::optional<int64_t> max_sync_recency_microseconds) const override {
    return base::android::ScopedJavaLocalRef<jobjectArray>();
  }

  base::android::ScopedJavaLocalRef<jobject> GetMostRecentValue(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& pref_name,
      std::optional<int> os_type,
      std::optional<int> form_factor,
      std::optional<int64_t> max_sync_recency_microseconds) const override {
    return base::android::ScopedJavaLocalRef<jobject>();
  }
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  // Testing member. Map containing TimestampedPrefValues mapped to their
  // associated pref's name.
  std::map<std::string_view,
           std::vector<sync_preferences::TimestampedPrefValue>>
      pref_values_;
  ServiceStatus service_status_ = ServiceStatus::kAvailable;
};

}  // namespace

// Test suite for Synced Set Up utility functions.
class SyncedSetUpUtilsTest : public PlatformTest {
 public:
  // Helper for configuring a TimestampedPrefValue.
  void ConfigureTimestampedPrefValue(
      sync_preferences::TimestampedPrefValue& timestamped_value,
      base::Value value,
      std::string device_sync_cache_guid,
      base::Time last_observed_change_time = base::Time::Now()) {
    timestamped_value.value = value.Clone();
    timestamped_value.last_observed_change_time = last_observed_change_time;
    timestamped_value.device_sync_cache_guid = device_sync_cache_guid;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestCrossDevicePrefTracker pref_tracker_;
  syncer::FakeDeviceInfoTracker device_info_tracker_;
};

namespace sync_preferences {

// Test that a device with a matching form factor is chosen as the best fit
// device.
TEST_F(SyncedSetUpUtilsTest, TestMatchPrefsByFormFactor) {
  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("local_device")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // Android tablet.
  std::unique_ptr<syncer::DeviceInfo> android_tablet =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kAndroid)
          .WithGuid("android_tablet")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kTablet)
          .Build();

  // Android phone (match).
  std::unique_ptr<syncer::DeviceInfo> android_phone =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kAndroid)
          .WithGuid("android_phone")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // iOS tablet.
  std::unique_ptr<syncer::DeviceInfo> ios_tablet =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("ios_tablet")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kTablet)
          .Build();

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_tablet.get());
  device_info_tracker_.Add(android_phone.get());
  device_info_tracker_.Add(ios_tablet.get());

  // Configure some `TimestampedPrefValue` objects associated with the tracked
  // device GUID's and add them to the pref tracker.
  TimestampedPrefValue local_device_magic_stack_enabled;
  ConfigureTimestampedPrefValue(local_device_magic_stack_enabled,
                                base::Value(true), local_device.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      local_device_magic_stack_enabled);

  TimestampedPrefValue android_tablet_magic_stack_enabled;
  TimestampedPrefValue android_tablet_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_tablet_magic_stack_enabled,
                                base::Value(true),
                                android_tablet.get()->guid());
  ConfigureTimestampedPrefValue(android_tablet_most_visited_enabled,
                                base::Value(false),
                                android_tablet.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_tablet_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_tablet_most_visited_enabled);

  TimestampedPrefValue android_phone_magic_stack_enabled;
  TimestampedPrefValue android_phone_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_phone_magic_stack_enabled,
                                base::Value(false),
                                android_phone.get()->guid());
  ConfigureTimestampedPrefValue(android_phone_most_visited_enabled,
                                base::Value(true), android_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_phone_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_phone_most_visited_enabled);

  TimestampedPrefValue ios_tablet_magic_stack_enabled;
  TimestampedPrefValue ios_tablet_most_visited_enabled;
  ConfigureTimestampedPrefValue(ios_tablet_magic_stack_enabled,
                                base::Value(false), ios_tablet.get()->guid());
  ConfigureTimestampedPrefValue(ios_tablet_most_visited_enabled,
                                base::Value(false), ios_tablet.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_tablet_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_tablet_most_visited_enabled);

  // Expect that the prefs from the Android phone are returned.
  std::map<std::string_view, base::Value> expected_result;
  expected_result.insert({prefs::kCrossDeviceMagicStackHomeModuleEnabled,
                          android_phone_magic_stack_enabled.value.Clone()});
  expected_result.insert({prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
                          android_phone_most_visited_enabled.value.Clone()});

  std::map<std::string_view, base::Value> result =
      synced_set_up::GetCrossDevicePrefsFromRemoteDevice(
          &pref_tracker_, &device_info_tracker_, local_device.get());
  ASSERT_TRUE(!result.empty());
  ASSERT_EQ(result.size(), expected_result.size());

  // Compare the resultant map to the expected map.
  for (const auto& [pref_name, pref_value] : expected_result) {
    auto it = result.find(pref_name);
    ASSERT_NE(it, result.end());
    EXPECT_EQ(it->second, pref_value);
  }
}

// Test that a device with a matching OS is chosen as the best fit device if
// there is no device with a matching form factor.
TEST_F(SyncedSetUpUtilsTest, TestMatchPrefsByOsType) {
  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("local_device")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // Android tablet.
  std::unique_ptr<syncer::DeviceInfo> android_tablet =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kAndroid)
          .WithGuid("android_tablet")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kTablet)
          .Build();

  // iOS tablet (match).
  std::unique_ptr<syncer::DeviceInfo> ios_tablet =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("ios_tablet")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kTablet)
          .Build();

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_tablet.get());
  device_info_tracker_.Add(ios_tablet.get());

  // Configure some `TimestampedPrefValue` objects associated with the tracked
  // device GUID's and add them to the pref tracker.
  TimestampedPrefValue local_device_magic_stack_enabled;
  ConfigureTimestampedPrefValue(local_device_magic_stack_enabled,
                                base::Value(true), local_device.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      local_device_magic_stack_enabled);

  TimestampedPrefValue android_tablet_magic_stack_enabled;
  TimestampedPrefValue android_tablet_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_tablet_magic_stack_enabled,
                                base::Value(true),
                                android_tablet.get()->guid());
  ConfigureTimestampedPrefValue(android_tablet_most_visited_enabled,
                                base::Value(false),
                                android_tablet.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_tablet_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_tablet_most_visited_enabled);

  TimestampedPrefValue ios_tablet_magic_stack_enabled;
  TimestampedPrefValue ios_tablet_most_visited_enabled;
  ConfigureTimestampedPrefValue(ios_tablet_magic_stack_enabled,
                                base::Value(false), ios_tablet.get()->guid());
  ConfigureTimestampedPrefValue(ios_tablet_most_visited_enabled,
                                base::Value(false), ios_tablet.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_tablet_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_tablet_most_visited_enabled);

  // Expect that the prefs from the iOS tablet are returned.
  std::map<std::string_view, base::Value> expected_result;
  expected_result.insert({prefs::kCrossDeviceMagicStackHomeModuleEnabled,
                          ios_tablet_magic_stack_enabled.value.Clone()});
  expected_result.insert({prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
                          ios_tablet_most_visited_enabled.value.Clone()});

  std::map<std::string_view, base::Value> result =
      synced_set_up::GetCrossDevicePrefsFromRemoteDevice(
          &pref_tracker_, &device_info_tracker_, local_device.get());
  ASSERT_TRUE(!result.empty());
  ASSERT_EQ(result.size(), expected_result.size());

  // compare the returned map to the expected map
  for (const auto& [pref_name, pref_value] : expected_result) {
    auto it = result.find(pref_name);
    ASSERT_NE(it, result.end());
    EXPECT_EQ(it->second, pref_value);
  }
}

// Test that a device with the highest volume of observed pref changes is chosen
// as the best fit device if the synced devices score the same against the
// current device on form factor and OS.
TEST_F(SyncedSetUpUtilsTest, TestMatchPrefsByObservedChangeCount) {
  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("local_device")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // Android phone.
  std::unique_ptr<syncer::DeviceInfo> android_phone =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kAndroid)
          .WithGuid("android_phone")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // iOS phone.
  std::unique_ptr<syncer::DeviceInfo> ios_phone =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("ios_phone")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // iOS phone (match).
  std::unique_ptr<syncer::DeviceInfo> ios_phone_2 =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("ios_phone_2")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_phone.get());
  device_info_tracker_.Add(ios_phone.get());
  device_info_tracker_.Add(ios_phone_2.get());

  // Configure some `TimestampedPrefValue` objects associated with the tracked
  // device GUID's and add them to the pref tracker.
  TimestampedPrefValue local_device_magic_stack_enabled;
  ConfigureTimestampedPrefValue(local_device_magic_stack_enabled,
                                base::Value(true), local_device.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      local_device_magic_stack_enabled);

  TimestampedPrefValue android_phone_magic_stack_enabled;
  TimestampedPrefValue android_phone_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_phone_magic_stack_enabled,
                                base::Value(true), android_phone.get()->guid());
  ConfigureTimestampedPrefValue(android_phone_most_visited_enabled,
                                base::Value(false),
                                android_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_phone_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_phone_most_visited_enabled);

  TimestampedPrefValue ios_phone_1_magic_stack_enabled;
  TimestampedPrefValue ios_phone_1_most_visited_enabled;
  ConfigureTimestampedPrefValue(ios_phone_1_magic_stack_enabled,
                                base::Value(false), ios_phone.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_1_most_visited_enabled,
                                base::Value(false), ios_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_phone_1_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_phone_1_most_visited_enabled);

  TimestampedPrefValue ios_phone_2_magic_stack_enabled;
  TimestampedPrefValue ios_phone_2_most_visited_enabled;
  TimestampedPrefValue ios_phone_2_price_tracking_enabled;
  ConfigureTimestampedPrefValue(ios_phone_2_magic_stack_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_2_most_visited_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_2_price_tracking_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_phone_2_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_phone_2_most_visited_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
      ios_phone_2_price_tracking_enabled);

  // Expect that the prefs from the second iOS phone with more changes are
  // returned.
  std::map<std::string_view, base::Value> expected_result;
  expected_result.insert({prefs::kCrossDeviceMagicStackHomeModuleEnabled,
                          ios_phone_2_magic_stack_enabled.value.Clone()});
  expected_result.insert({prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
                          ios_phone_2_most_visited_enabled.value.Clone()});
  expected_result.insert({prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
                          ios_phone_2_price_tracking_enabled.value.Clone()});

  std::map<std::string_view, base::Value> result =
      synced_set_up::GetCrossDevicePrefsFromRemoteDevice(
          &pref_tracker_, &device_info_tracker_, local_device.get());
  ASSERT_TRUE(!result.empty());
  ASSERT_EQ(result.size(), expected_result.size());

  // Compare the resultant map to the expected map.
  for (const auto& [pref_name, pref_value] : expected_result) {
    auto it = result.find(pref_name);
    ASSERT_NE(it, result.end());
    EXPECT_EQ(it->second, pref_value);
  }
}

// Tests that no new prefs to apply are returned if the local device has a
// higher volume of observed pref changes than the otherwise best fitting
// device.
TEST_F(SyncedSetUpUtilsTest, TestKeepLocalPrefsByChangeActivity) {
  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("local_device")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // Android phone.
  std::unique_ptr<syncer::DeviceInfo> android_phone =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kAndroid)
          .WithGuid("android_phone")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // iOS phone.
  std::unique_ptr<syncer::DeviceInfo> ios_phone =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("ios_phone")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // iOS phone (match).
  std::unique_ptr<syncer::DeviceInfo> ios_phone_2 =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("ios_phone_2")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_phone.get());
  device_info_tracker_.Add(ios_phone.get());
  device_info_tracker_.Add(ios_phone_2.get());

  // Configure some `TimestampedPrefValue` objects associated with the tracked
  // device GUID's and add them to the pref tracker.
  TimestampedPrefValue local_device_magic_stack_enabled;
  TimestampedPrefValue local_device_most_visited_enabled;
  TimestampedPrefValue local_device_price_tracking_enabled;
  TimestampedPrefValue local_device_safety_check_enabled;
  ConfigureTimestampedPrefValue(local_device_magic_stack_enabled,
                                base::Value(true), local_device.get()->guid());
  ConfigureTimestampedPrefValue(local_device_most_visited_enabled,
                                base::Value(true), local_device.get()->guid());
  ConfigureTimestampedPrefValue(local_device_price_tracking_enabled,
                                base::Value(true), local_device.get()->guid());
  ConfigureTimestampedPrefValue(local_device_safety_check_enabled,
                                base::Value(true), local_device.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      local_device_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      local_device_most_visited_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
      local_device_price_tracking_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceSafetyCheckHomeModuleEnabled,
      local_device_safety_check_enabled);

  TimestampedPrefValue android_phone_magic_stack_enabled;
  TimestampedPrefValue android_phone_most_visited_enabled;
  ConfigureTimestampedPrefValue(android_phone_magic_stack_enabled,
                                base::Value(true), android_phone.get()->guid());
  ConfigureTimestampedPrefValue(android_phone_most_visited_enabled,
                                base::Value(false),
                                android_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      android_phone_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      android_phone_most_visited_enabled);

  TimestampedPrefValue ios_phone_1_magic_stack_enabled;
  TimestampedPrefValue ios_phone_1_most_visited_enabled;
  ConfigureTimestampedPrefValue(ios_phone_1_magic_stack_enabled,
                                base::Value(false), ios_phone.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_1_most_visited_enabled,
                                base::Value(false), ios_phone.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_phone_1_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_phone_1_most_visited_enabled);

  TimestampedPrefValue ios_phone_2_magic_stack_enabled;
  TimestampedPrefValue ios_phone_2_most_visited_enabled;
  TimestampedPrefValue ios_phone_2_price_tracking_enabled;
  ConfigureTimestampedPrefValue(ios_phone_2_magic_stack_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_2_most_visited_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  ConfigureTimestampedPrefValue(ios_phone_2_price_tracking_enabled,
                                base::Value(true), ios_phone_2.get()->guid());
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled,
      ios_phone_2_magic_stack_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
      ios_phone_2_most_visited_enabled);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
      ios_phone_2_price_tracking_enabled);

  // Expect that no new prefs are returned.
  std::map<std::string_view, base::Value> result =
      synced_set_up::GetCrossDevicePrefsFromRemoteDevice(
          &pref_tracker_, &device_info_tracker_, local_device.get());
  EXPECT_TRUE(result.empty());
}

// Tests that if a pref has multiple observed changes only the most recently
// observed pref change is retrieved.
TEST_F(SyncedSetUpUtilsTest, TestReturnsMostRecentObservedPrefChanges) {
  const base::Time kNow = base::Time::Now();

  // Local device (iOS phone).
  std::unique_ptr<syncer::DeviceInfo> local_device =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithGuid("local_device")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  // Android phone (match).
  std::unique_ptr<syncer::DeviceInfo> android_phone =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kAndroid)
          .WithGuid("android_phone")
          .WithClientName("Device Name")
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();

  device_info_tracker_.Add(local_device.get());
  device_info_tracker_.SetLocalCacheGuid(local_device.get()->guid());
  device_info_tracker_.Add(android_phone.get());

  // Configure some `TimestampedPrefValue` objects for the same pref associated
  // with the Android phone GUID's and add them to the pref tracker. These
  // represent the same pref being changed several times over a period of time.
  TimestampedPrefValue magic_stack_enabled_day;
  TimestampedPrefValue magic_stack_enabled_now;
  TimestampedPrefValue magic_stack_enabled_week;
  ConfigureTimestampedPrefValue(magic_stack_enabled_day, base::Value(true),
                                android_phone.get()->guid(),
                                kNow - base::Days(1));
  ConfigureTimestampedPrefValue(magic_stack_enabled_now, base::Value(false),
                                android_phone.get()->guid(), kNow);
  ConfigureTimestampedPrefValue(magic_stack_enabled_week, base::Value(true),
                                android_phone.get()->guid(),
                                kNow - base::Days(7));
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled, magic_stack_enabled_day);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled, magic_stack_enabled_now);
  pref_tracker_.AddSyncedPrefValue(
      prefs::kCrossDeviceMagicStackHomeModuleEnabled, magic_stack_enabled_week);

  // Expect that the pref is returned with its most recently set value.
  std::map<std::string_view, base::Value> expected_result;
  expected_result.insert({prefs::kCrossDeviceMagicStackHomeModuleEnabled,
                          magic_stack_enabled_now.value.Clone()});

  std::map<std::string_view, base::Value> result =
      synced_set_up::GetCrossDevicePrefsFromRemoteDevice(
          &pref_tracker_, &device_info_tracker_, local_device.get());
  ASSERT_TRUE(!result.empty());
  ASSERT_EQ(result.size(), expected_result.size());

  // Compare the resultant map to the expected map.
  for (const auto& [pref_name, pref_value] : expected_result) {
    auto it = result.find(pref_name);
    ASSERT_NE(it, result.end());
    EXPECT_EQ(it->second, pref_value);
  }
}

}  // namespace sync_preferences
