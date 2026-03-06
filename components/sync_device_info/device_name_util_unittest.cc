// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_name_util.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class DeviceNameUtilTest : public testing::Test {
 public:
  DeviceNameUtilTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  syncer::TestSyncService test_sync_service_;
};

static std::unique_ptr<DeviceInfo> CreateFakeDeviceInfo(
    const std::string& id,
    const std::string& name,
    sync_pb::SyncEnums_DeviceType device_type,
    DeviceInfo::OsType os_type,
    DeviceInfo::FormFactor form_factor,
    const std::string& manufacturer_name,
    const std::string& model_name) {
  return std::make_unique<DeviceInfo>(
      id, name, "chrome_version", "user_agent", device_type, os_type,
      form_factor, "device_id", manufacturer_name, model_name,
      /*full_hardware_class=*/std::string(),
      /*last_updated_timestamp=*/base::Time::Now(),
      DeviceInfoUtil::GetPulseInterval(),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      DeviceInfo::SharingInfo(
          {"sender_id_fcm_token", "sender_id_p256dh", "sender_id_auth_secret"},
          "chime_representative_target_id",
          std::set<sync_pb::SharingSpecificFields::EnabledFeatures>{
              sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2}),
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/DataTypeSet(),
      /*auto_sign_out_last_signin_timestamp=*/std::nullopt,
      /*desktop_to_ios_promo_receiving_enabled=*/false);
}

}  // namespace

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_AppleDevices_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "MacbookPro1,1", sync_pb::SyncEnums_DeviceType_TYPE_MAC,
      DeviceInfo::OsType::kMac, DeviceInfo::FormFactor::kDesktop, "Apple Inc.",
      "MacbookPro1,1");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("MacbookPro1,1", names.full_name);
  EXPECT_EQ("MacbookPro", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_AppleDevices_FullySynced) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Bobs-iMac", sync_pb::SyncEnums_DeviceType_TYPE_MAC,
      DeviceInfo::OsType::kMac, DeviceInfo::FormFactor::kDesktop, "Apple Inc.",
      "MacbookPro1,1");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Bobs-iMac", names.full_name);
  EXPECT_EQ("Bobs-iMac", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_IOS_GenericName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "iPhone", sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
      DeviceInfo::OsType::kIOS, DeviceInfo::FormFactor::kPhone, "Apple Inc.",
      "iPhone14,5");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("iPhone14,5", names.full_name);
  EXPECT_EQ("iPhone", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_IOS_CustomName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "John's iPhone", sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
      DeviceInfo::OsType::kIOS, DeviceInfo::FormFactor::kPhone, "Apple Inc.",
      "iPhone14,5");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("John's iPhone", names.full_name);
  EXPECT_EQ("John's iPhone", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_EmptyClientName) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
                           DeviceInfo::OsType::kWindows,
                           DeviceInfo::FormFactor::kDesktop, "Dell", "XPS 13");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Dell Computer XPS 13", names.full_name);
  EXPECT_EQ("Dell Computer", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_ChromeOSDevices) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Chromebook", sync_pb::SyncEnums_DeviceType_TYPE_CROS,
      DeviceInfo::OsType::kChromeOsAsh, DeviceInfo::FormFactor::kDesktop,
      "Google", "Chromebook");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Google Chromebook", names.full_name);
  EXPECT_EQ("Google Chromebook", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_AndroidPhones) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel 2", sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
      DeviceInfo::OsType::kAndroid, DeviceInfo::FormFactor::kPhone, "Google",
      "Pixel 2");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Google Phone Pixel 2", names.full_name);
  EXPECT_EQ("Google Phone", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_AndroidTablets) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel C", sync_pb::SyncEnums_DeviceType_TYPE_TABLET,
      DeviceInfo::OsType::kAndroid, DeviceInfo::FormFactor::kTablet, "Google",
      "Pixel C");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Google Tablet Pixel C", names.full_name);
  EXPECT_EQ("Google Tablet", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_Windows_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BX123", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop, "Dell",
      "BX123");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Dell Computer BX123", names.full_name);
  EXPECT_EQ("Dell Computer", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_Windows_FullySynced) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BOBS-WINDOWS-1", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop, "Dell",
      "BX123");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("BOBS-WINDOWS-1", names.full_name);
  EXPECT_EQ("BOBS-WINDOWS-1", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_Linux_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "30BDS0RA0G", sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      DeviceInfo::OsType::kLinux, DeviceInfo::FormFactor::kDesktop, "LENOVO",
      "30BDS0RA0G");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("LENOVO Computer 30BDS0RA0G", names.full_name);
  EXPECT_EQ("LENOVO Computer", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_Linux_FullySynced) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "bob.chromium.org", sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
      DeviceInfo::OsType::kLinux, DeviceInfo::FormFactor::kDesktop, "LENOVO",
      "30BDS0RA0G");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("bob.chromium.org", names.full_name);
  EXPECT_EQ("bob.chromium.org", names.short_name);
}

TEST_F(DeviceNameUtilTest, CheckManufacturerNameCapitalization) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "model", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop, "foo bar",
      "model");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Foo Bar Computer model", names.full_name);
  EXPECT_EQ("Foo Bar Computer", names.short_name);

  device = CreateFakeDeviceInfo(
      "guid", "model", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop, "foo1bar",
      "model");
  names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Foo1Bar Computer model", names.full_name);
  EXPECT_EQ("Foo1Bar Computer", names.short_name);

  device = CreateFakeDeviceInfo(
      "guid", "model", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop,
      "foo_bar-FOO", "model");
  names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Foo_Bar-FOO Computer model", names.full_name);
  EXPECT_EQ("Foo_Bar-FOO Computer", names.short_name);

  device = CreateFakeDeviceInfo(
      "guid", "model", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop,
      "foo&bar foo", "model");
  names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Foo&Bar Foo Computer model", names.full_name);
  EXPECT_EQ("Foo&Bar Foo Computer", names.short_name);
}

}  // namespace syncer
