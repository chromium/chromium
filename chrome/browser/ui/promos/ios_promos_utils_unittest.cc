// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promos_utils.h"

#include "base/json/values_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/prefs/cross_device_pref_tracker/cross_device_pref_tracker_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#include "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_names.h"
#include "components/sync_preferences/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DeviceInfo;

namespace {

const char* kIOSDevice = "ios_device";
const char* kAndroidDevice = "android_device";

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<KeyedService> CreateTestDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::FakeDeviceInfoSyncService>();
}

std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfo(
    const std::string& guid,
    syncer::DeviceInfo::OsType os_type,
    syncer::DeviceInfo::FormFactor form_factor) {
  return std::make_unique<syncer::DeviceInfo>(
      guid, "name", "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, os_type, form_factor,
      "scoped_id", "manufacturer", "model", "full_hardware_class",
      /*last_updated_timestamp=*/base::Time::Now(),
      /*pulse_interval=*/base::Days(1),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::DataTypeSet::All(),
      /*auto_sign_out_last_signin_timestamp=*/std::nullopt);
}

std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfoWithTime(
    const std::string& guid,
    syncer::DeviceInfo::OsType os_type,
    base::Time last_updated) {
  return std::make_unique<syncer::DeviceInfo>(
      guid, "name", "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_PHONE, os_type,
      syncer::DeviceInfo::FormFactor::kPhone, "scoped_id", "manufacturer",
      "model", "full_hardware_class", last_updated,
      /*pulse_interval=*/base::Days(1),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::DataTypeSet::All(),
      /*auto_sign_out_last_signin_timestamp=*/std::nullopt);
}

class IOSPromosUtilsTest : public testing::Test {
 public:
  IOSPromosUtilsTest() {
    feature_list_.InitAndEnableFeature(
        sync_preferences::features::kEnableCrossDevicePrefTracker);
  }
  ~IOSPromosUtilsTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        DeviceInfoSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateTestDeviceInfoSyncService));
    profile_ = builder.Build();
    CrossDevicePrefTrackerFactory::GetForProfile(profile());
  }

  void TearDown() override {
    profile_.reset();
    testing::Test::TearDown();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  syncer::FakeDeviceInfoSyncService* device_info_sync_service() {
    return static_cast<syncer::FakeDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetForProfile(profile()));
  }

  syncer::FakeDeviceInfoTracker* device_info_tracker() {
    return device_info_sync_service()->GetDeviceInfoTracker();
  }

  void WriteCrossDeviceValue(PrefService* prefs,
                             std::string_view pref_name,
                             std::string_view device_guid,
                             base::Value&& value,
                             base::Time timestamp = base::Time::Now()) {
    base::Value::Dict timestamped_value;
    timestamped_value.Set("value", std::move(value));
    timestamped_value.Set("last_observed_change_time",
                          base::TimeToValue(timestamp));
    timestamped_value.Set("update_time", base::TimeToValue(timestamp));
    base::Value::Dict cross_device_value;
    cross_device_value.Set(device_guid, std::move(timestamped_value));
    prefs->SetDict(pref_name, std::move(cross_device_value));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(IOSPromosUtilsTest, IsUserActiveOnIOS_Recent) {
  // Set a recent timestamp for the iOS device.
  WriteCrossDeviceValue(profile()->GetPrefs(),
                        prefs::kCrossDeviceCrossPlatformPromosIOS16thActiveDay,
                        kIOSDevice, base::TimeToValue(base::Time::Now()));

  // Fake an iOS device.
  device_info_tracker()->Add(
      CreateDeviceInfo(kIOSDevice, syncer::DeviceInfo::OsType::kIOS,
                       syncer::DeviceInfo::FormFactor::kPhone));

  EXPECT_TRUE(ios_promos_utils::IsUserActiveOnIOS(profile()));
}

TEST_F(IOSPromosUtilsTest, IsUserActiveOnIOS_Old) {
  // Set an old timestamp for the iOS device.
  WriteCrossDeviceValue(profile()->GetPrefs(),
                        prefs::kCrossDeviceCrossPlatformPromosIOS16thActiveDay,
                        kIOSDevice,
                        base::TimeToValue(base::Time::Now() - base::Days(30)));

  // Fake an iOS device.
  device_info_tracker()->Add(
      CreateDeviceInfo(kIOSDevice, syncer::DeviceInfo::OsType::kIOS,
                       syncer::DeviceInfo::FormFactor::kPhone));

  EXPECT_FALSE(ios_promos_utils::IsUserActiveOnIOS(profile()));
}

TEST_F(IOSPromosUtilsTest, IsUserActiveOnIOS_NoIOSDevice) {
  // Set a recent timestamp for a non-iOS device.
  WriteCrossDeviceValue(profile()->GetPrefs(),
                        prefs::kCrossDeviceCrossPlatformPromosIOS16thActiveDay,
                        kAndroidDevice, base::TimeToValue(base::Time::Now()));

  // Fake a non-iOS device.
  device_info_tracker()->Add(
      CreateDeviceInfo(kAndroidDevice, syncer::DeviceInfo::OsType::kAndroid,
                       syncer::DeviceInfo::FormFactor::kPhone));

  EXPECT_FALSE(ios_promos_utils::IsUserActiveOnIOS(profile()));
}

// Tests that IsUserActiveOnAndroid returns true when a recent Android device
// exists.
TEST_F(IOSPromosUtilsTest, IsUserActiveOnAndroid_Recent) {
  device_info_tracker()->Add(CreateDeviceInfoWithTime(
      kAndroidDevice, syncer::DeviceInfo::OsType::kAndroid,
      base::Time::Now() - base::Days(1)));

  EXPECT_TRUE(ios_promos_utils::IsUserActiveOnAndroid(profile()));
}

// Tests that IsUserActiveOnAndroid returns false when only an old Android
// device exists.
TEST_F(IOSPromosUtilsTest, IsUserActiveOnAndroid_Old) {
  device_info_tracker()->Add(CreateDeviceInfoWithTime(
      kAndroidDevice, syncer::DeviceInfo::OsType::kAndroid,
      base::Time::Now() - base::Days(30)));

  EXPECT_FALSE(ios_promos_utils::IsUserActiveOnAndroid(profile()));
}

// Tests that IsUserActiveOnAndroid returns false when no Android device exists.
TEST_F(IOSPromosUtilsTest, IsUserActiveOnAndroid_NoDevice) {
  device_info_tracker()->Add(
      CreateDeviceInfoWithTime(kIOSDevice, syncer::DeviceInfo::OsType::kIOS,
                               base::Time::Now() - base::Days(1)));

  EXPECT_FALSE(ios_promos_utils::IsUserActiveOnAndroid(profile()));
}

}  // namespace
