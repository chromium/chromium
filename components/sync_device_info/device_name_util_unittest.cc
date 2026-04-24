// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_name_util.h"

#include <memory>
#include <set>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
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
    DeviceInfo::DeviceType device_type,
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
      DeviceInfo::SendTabReceivingType::kChromeOrUnspecified,
      DeviceInfo::SharingInfo(
          {"sender_id_fcm_token", "sender_id_p256dh", "sender_id_auth_secret"},
          "chime_representative_target_id",
          std::set<DeviceInfo::SharingFeature>{
              DeviceInfo::SharingFeature::kClickToCallV2}),
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/DataTypeSet(),
      /*auto_sign_out_last_signin_timestamp=*/std::nullopt,
      /*desktop_to_ios_promo_receiving_enabled=*/false,
      /*desktop_to_ios_promo_receiving_types=*/
      MobilePromoOnDesktopPromoTypeSet{},
      /*glic_experimental_triggering_opted_in=*/false);
}

}  // namespace

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_AppleDevices_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "MacbookPro1,1", DeviceInfo::DeviceType::kMac,
      DeviceInfo::OsType::kMac, DeviceInfo::FormFactor::kDesktop, "Apple Inc.",
      "MacbookPro1,1");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("MacbookPro1,1", names.full_name);
  EXPECT_EQ("MacbookPro", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_AppleDevices_FullySynced) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Bobs-iMac", DeviceInfo::DeviceType::kMac,
      DeviceInfo::OsType::kMac, DeviceInfo::FormFactor::kDesktop, "Apple Inc.",
      "MacbookPro1,1");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Bobs-iMac", names.full_name);
  EXPECT_EQ("Bobs-iMac", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_IOS_GenericName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "iPhone", DeviceInfo::DeviceType::kPhone,
      DeviceInfo::OsType::kIOS, DeviceInfo::FormFactor::kPhone, "Apple Inc.",
      "iPhone14,5");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("iPhone14,5", names.full_name);
  EXPECT_EQ("iPhone", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_IOS_CustomName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "John's iPhone", DeviceInfo::DeviceType::kPhone,
      DeviceInfo::OsType::kIOS, DeviceInfo::FormFactor::kPhone, "Apple Inc.",
      "iPhone14,5");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("John's iPhone", names.full_name);
  EXPECT_EQ("John's iPhone", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_EmptyClientName) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "", DeviceInfo::DeviceType::kWindows,
                           DeviceInfo::OsType::kWindows,
                           DeviceInfo::FormFactor::kDesktop, "Dell", "XPS 13");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Dell Computer XPS 13", names.full_name);
  EXPECT_EQ("Dell Computer", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_ChromeOSDevices) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Chromebook", DeviceInfo::DeviceType::kChromeOS,
      DeviceInfo::OsType::kChromeOsAsh, DeviceInfo::FormFactor::kDesktop,
      "Google", "Chromebook");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Google Chromebook", names.full_name);
  EXPECT_EQ("Google Chromebook", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_AndroidPhones) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "Pixel 2", DeviceInfo::DeviceType::kPhone,
                           DeviceInfo::OsType::kAndroid,
                           DeviceInfo::FormFactor::kPhone, "Google", "Pixel 2");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Google Phone Pixel 2", names.full_name);
  EXPECT_EQ("Google Phone", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_AndroidTablets) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel C", DeviceInfo::DeviceType::kTablet,
      DeviceInfo::OsType::kAndroid, DeviceInfo::FormFactor::kTablet, "Google",
      "Pixel C");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Google Tablet Pixel C", names.full_name);
  EXPECT_EQ("Google Tablet", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_Windows_SigninOnly) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "BX123", DeviceInfo::DeviceType::kWindows,
                           DeviceInfo::OsType::kWindows,
                           DeviceInfo::FormFactor::kDesktop, "Dell", "BX123");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Dell Computer BX123", names.full_name);
  EXPECT_EQ("Dell Computer", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_Windows_FullySynced) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BOBS-WINDOWS-1", DeviceInfo::DeviceType::kWindows,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop, "Dell",
      "BX123");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("BOBS-WINDOWS-1", names.full_name);
  EXPECT_EQ("BOBS-WINDOWS-1", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_Linux_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "30BDS0RA0G", DeviceInfo::DeviceType::kLinux,
      DeviceInfo::OsType::kLinux, DeviceInfo::FormFactor::kDesktop, "LENOVO",
      "30BDS0RA0G");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("LENOVO Computer 30BDS0RA0G", names.full_name);
  EXPECT_EQ("LENOVO Computer", names.short_name);
}

TEST_F(DeviceNameUtilTest, GetDeviceDisplayNames_Linux_FullySynced) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "bob.chromium.org", DeviceInfo::DeviceType::kLinux,
      DeviceInfo::OsType::kLinux, DeviceInfo::FormFactor::kDesktop, "LENOVO",
      "30BDS0RA0G");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("bob.chromium.org", names.full_name);
  EXPECT_EQ("bob.chromium.org", names.short_name);
}

TEST_F(DeviceNameUtilTest, CheckManufacturerNameCapitalization) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "model", DeviceInfo::DeviceType::kWindows,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop, "foo bar",
      "model");
  DeviceDisplayNames names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Foo Bar Computer model", names.full_name);
  EXPECT_EQ("Foo Bar Computer", names.short_name);

  device = CreateFakeDeviceInfo(
      "guid", "model", DeviceInfo::DeviceType::kWindows,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop, "foo1bar",
      "model");
  names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Foo1Bar Computer model", names.full_name);
  EXPECT_EQ("Foo1Bar Computer", names.short_name);

  device = CreateFakeDeviceInfo(
      "guid", "model", DeviceInfo::DeviceType::kWindows,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop,
      "foo_bar-FOO", "model");
  names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Foo_Bar-FOO Computer model", names.full_name);
  EXPECT_EQ("Foo_Bar-FOO Computer", names.short_name);

  device = CreateFakeDeviceInfo(
      "guid", "model", DeviceInfo::DeviceType::kWindows,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop,
      "foo&bar foo", "model");
  names = GetDeviceDisplayNames(device.get());

  EXPECT_EQ("Foo&Bar Foo Computer model", names.full_name);
  EXPECT_EQ("Foo&Bar Foo Computer", names.short_name);
}

TEST_F(DeviceNameUtilTest, DetermineDisplayNamesAndDeduplicate) {
  std::unique_ptr<DeviceInfo> local_device = CreateFakeDeviceInfo(
      "local_guid", "XPS 13", DeviceInfo::DeviceType::kWindows,
      DeviceInfo::OsType::kWindows, DeviceInfo::FormFactor::kDesktop, "Dell",
      "XPS 13");
  ASSERT_EQ("Dell Computer XPS 13",
            GetDeviceDisplayNames(local_device.get()).full_name);

  std::unique_ptr<DeviceInfo> device1 =
      CreateFakeDeviceInfo("guid1", "Pixel 6", DeviceInfo::DeviceType::kPhone,
                           DeviceInfo::OsType::kAndroid,
                           DeviceInfo::FormFactor::kPhone, "Google", "Pixel 6");
  DeviceDisplayNames names1 = GetDeviceDisplayNames(device1.get());
  ASSERT_EQ("Google Phone Pixel 6", names1.full_name);
  ASSERT_EQ("Google Phone", names1.short_name);

  std::unique_ptr<DeviceInfo> device2 =
      CreateFakeDeviceInfo("guid2", "Pixel 7", DeviceInfo::DeviceType::kPhone,
                           DeviceInfo::OsType::kAndroid,
                           DeviceInfo::FormFactor::kPhone, "Google", "Pixel 7");
  DeviceDisplayNames names2 = GetDeviceDisplayNames(device2.get());
  ASSERT_EQ("Google Phone Pixel 7", names2.full_name);
  ASSERT_EQ("Google Phone", names2.short_name);

  std::unique_ptr<DeviceInfo> device3 =
      CreateFakeDeviceInfo("guid3", "XPS 13", DeviceInfo::DeviceType::kWindows,
                           DeviceInfo::OsType::kWindows,
                           DeviceInfo::FormFactor::kDesktop, "Dell", "XPS 13");
  ASSERT_EQ("Dell Computer XPS 13",
            GetDeviceDisplayNames(device3.get()).full_name);

  std::vector<const DeviceInfo*> devices = {device1.get(), device2.get(),
                                            device3.get()};

  auto results = DetermineDisplayNamesAndDeduplicate(
      devices, GetDeviceDisplayNames(local_device.get()).full_name);

  // device1 and device2 have the same short name "Google Phone", so they should
  // use their full names.
  // device3 has the same full name as the local device, so it should be
  // filtered out.
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(device1.get(), results[0].device);
  EXPECT_EQ("Google Phone Pixel 6", results[0].display_name);
  EXPECT_EQ(device2.get(), results[1].device);
  EXPECT_EQ("Google Phone Pixel 7", results[1].display_name);
}

TEST_F(DeviceNameUtilTest,
       DetermineDisplayNamesAndDeduplicate_UniqueShortNames) {
  std::unique_ptr<DeviceInfo> device1 =
      CreateFakeDeviceInfo("guid1", "Pixel 6", DeviceInfo::DeviceType::kPhone,
                           DeviceInfo::OsType::kAndroid,
                           DeviceInfo::FormFactor::kPhone, "Google", "Pixel 6");
  ASSERT_EQ("Google Phone", GetDeviceDisplayNames(device1.get()).short_name);

  std::unique_ptr<DeviceInfo> device2 =
      CreateFakeDeviceInfo("guid2", "XPS 13", DeviceInfo::DeviceType::kWindows,
                           DeviceInfo::OsType::kWindows,
                           DeviceInfo::FormFactor::kDesktop, "Dell", "XPS 13");
  ASSERT_EQ("Dell Computer", GetDeviceDisplayNames(device2.get()).short_name);

  std::vector<const DeviceInfo*> devices = {device1.get(), device2.get()};

  auto results = DetermineDisplayNamesAndDeduplicate(devices, std::nullopt);

  // Both have unique short names.
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(device1.get(), results[0].device);
  EXPECT_EQ("Google Phone", results[0].display_name);
  EXPECT_EQ(device2.get(), results[1].device);
  EXPECT_EQ("Dell Computer", results[1].display_name);
}

}  // namespace syncer
