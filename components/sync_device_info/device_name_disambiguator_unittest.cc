// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_name_disambiguator.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/sync/base/features.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::AllOf;
using testing::ElementsAre;
using testing::Field;

class DeviceNameDisambiguatorTest : public testing::Test {
 public:
  DeviceNameDisambiguatorTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kSyncSimplifyDeviceNaming,
                              syncer::kSyncDisambiguateDeviceNamesWithChannel},
        /*disabled_features=*/{});
  }
  ~DeviceNameDisambiguatorTest() override = default;

  std::string GetDisambiguatedDisplayName(
      const DeviceInfo* target,
      const std::vector<const DeviceInfo*>& devices,
      const DeviceInfo* local_device = nullptr) {
    std::vector<std::string> names =
        GetDeviceDisplayNamesListDisambiguatedByChannel(devices, local_device);
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i] == target) {
        return names[i];
      }
    }
    return std::string();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that unique devices return their base display names without any release
// channel labels, even if they are on a non-stable release channel.
TEST_F(DeviceNameDisambiguatorTest,
       GetDisambiguatedDisplayName_UniqueDevices_ReturnsBaseName) {
  std::unique_ptr<DeviceInfo> phone =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid1")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
          .Build();
  std::unique_ptr<DeviceInfo> laptop =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kMac)
          .WithGuid("guid2")
          .WithClientName("")
          .WithManufacturerName("Apple")
          .WithModelName("MacBook Pro")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();

  EXPECT_EQ("Google Phone", GetDisambiguatedDisplayName(
                                phone.get(), {phone.get(), laptop.get()}));
  EXPECT_EQ("MacBook Pro", GetDisambiguatedDisplayName(
                               laptop.get(), {phone.get(), laptop.get()}));
}

// Tests that devices sharing a base display name on different channels receive
// channel disambiguation labels.
TEST_F(
    DeviceNameDisambiguatorTest,
    GetDisambiguatedDisplayName_DuplicateBaseNameDifferentChannels_ReturnsChannelLabel) {
  std::unique_ptr<DeviceInfo> stable_device =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid1")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();
  std::unique_ptr<DeviceInfo> canary_device =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid2")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
          .Build();

  std::vector<const DeviceInfo*> devices = {stable_device.get(),
                                            canary_device.get()};
  // Stable channel devices omit the channel label, while non-stable channels
  // append their release channel name when disambiguating duplicates.
  EXPECT_EQ("Google Phone",
            GetDisambiguatedDisplayName(stable_device.get(), devices));
  EXPECT_EQ("Google Phone (Canary)",
            GetDisambiguatedDisplayName(canary_device.get(), devices));
}

// Tests that remote devices colliding with the active local device's display
// name return the base name when on the Stable channel, but receive a release
// channel label if on a non-stable channel.
TEST_F(
    DeviceNameDisambiguatorTest,
    GetDisambiguatedDisplayName_RemoteCollidesWithLocalStableDevice_ReturnsBaseNameOrLabel) {
  std::unique_ptr<DeviceInfo> local_device =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("local_guid")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();
  std::unique_ptr<DeviceInfo> remote_stable_device =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("remote_stable_guid")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();
  std::unique_ptr<DeviceInfo> remote_canary_device =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("remote_canary_guid")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
          .Build();

  std::vector<const DeviceInfo*> devices = {remote_stable_device.get(),
                                            remote_canary_device.get()};
  EXPECT_EQ("Google Phone",
            GetDisambiguatedDisplayName(remote_stable_device.get(), devices,
                                        local_device.get()));
  EXPECT_EQ("Google Phone (Canary)",
            GetDisambiguatedDisplayName(remote_canary_device.get(), devices,
                                        local_device.get()));
}

// Tests that a mix of non-stable channel duplicates and stable-channel
// duplicates correctly disambiguates non-stable channels while returning the
// base name for stable-channel duplicates.
TEST_F(
    DeviceNameDisambiguatorTest,
    GetDisambiguatedDisplayName_MultipleChannelsAndProfiles_DisambiguatesCorrectly) {
  std::unique_ptr<DeviceInfo> stable_profile1 =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid1")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();
  std::unique_ptr<DeviceInfo> stable_profile2 =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid2")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();
  std::unique_ptr<DeviceInfo> canary_device =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid3")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
          .Build();

  std::vector<const DeviceInfo*> devices = {
      stable_profile1.get(), stable_profile2.get(), canary_device.get()};
  // Since the base name "Google Phone" has multiple occurrences across devices,
  // the Canary device receives its release channel label.
  EXPECT_EQ("Google Phone (Canary)",
            GetDisambiguatedDisplayName(canary_device.get(), devices));
  // Stable channel devices omit the release channel label.
  EXPECT_EQ("Google Phone",
            GetDisambiguatedDisplayName(stable_profile1.get(), devices));
  EXPECT_EQ("Google Phone",
            GetDisambiguatedDisplayName(stable_profile2.get(), devices));
}

// Tests that devices with an empty or unrecognized user agent return clean
// display names without empty label parentheses.
TEST_F(DeviceNameDisambiguatorTest,
       GetDisambiguatedDisplayName_EmptyUserAgent_ReturnsBaseName) {
  std::unique_ptr<DeviceInfo> device =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid1")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 6")
          .WithSyncUserAgent("")
          .Build();

  EXPECT_EQ("Google Phone",
            GetDisambiguatedDisplayName(device.get(), {device.get()}));
}

// Tests that colliding devices on a non-stable release channel receive
// the release channel label.
TEST_F(
    DeviceNameDisambiguatorTest,
    GetDisambiguatedDisplayName_CollidingNonStableProfiles_ReturnsChannelLabel) {
  std::unique_ptr<DeviceInfo> canary1 =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid1")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 7")
          .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
          .Build();
  std::unique_ptr<DeviceInfo> canary2 =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid2")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 7")
          .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
          .Build();

  std::vector<const DeviceInfo*> devices = {canary1.get(), canary2.get()};
  EXPECT_EQ("Google Phone (Canary)",
            GetDisambiguatedDisplayName(canary1.get(), devices));
  EXPECT_EQ("Google Phone (Canary)",
            GetDisambiguatedDisplayName(canary2.get(), devices));
}

// Tests that nullptr entries in the device list do not crash or corrupt counts.
TEST_F(DeviceNameDisambiguatorTest,
       GetDisambiguatedDisplayName_NullDevicePointersInList_SafelyIgnored) {
  std::unique_ptr<DeviceInfo> phone =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid1")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 7")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();

  EXPECT_EQ("Google Phone",
            GetDisambiguatedDisplayName(phone.get(), {phone.get(), nullptr}));
}

// Tests that GetDeviceDisplayNamesListDisambiguatedByChannel resolves
// duplicate names across different release channels and preserves order.
TEST_F(
    DeviceNameDisambiguatorTest,
    GetDeviceDisplayNamesListDisambiguatedByChannel_DuplicateDevices_ReturnsDisambiguatedNames) {
  std::unique_ptr<DeviceInfo> stable1 =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid1")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 7")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();
  std::unique_ptr<DeviceInfo> stable2 =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid2")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 7")
          .WithSyncUserAgent("Mozilla/5.0 channel(stable)")
          .Build();
  std::unique_ptr<DeviceInfo> canary =
      TestDeviceInfoBuilder(DeviceInfo::OsType::kAndroid)
          .WithGuid("guid3")
          .WithClientName("")
          .WithManufacturerName("Google")
          .WithModelName("Pixel 7")
          .WithSyncUserAgent("Mozilla/5.0 channel(canary)")
          .Build();

  std::vector<std::string> result =
      GetDeviceDisplayNamesListDisambiguatedByChannel(
          {stable1.get(), stable2.get(), canary.get()},
          /*local_device=*/nullptr);

  EXPECT_THAT(result, ElementsAre("Google Phone", "Google Phone",
                                  "Google Phone (Canary)"));
}

}  // namespace

}  // namespace syncer
