// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_name_util.h"

#include <memory>
#include <set>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::ElementsAre;
using testing::Test;

class DeviceNameUtilTest : public Test {
 public:
  DeviceNameUtilTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  syncer::TestSyncService test_sync_service_;
};

static std::unique_ptr<DeviceInfo> CreateFakeDeviceInfo(
    const std::string& id,
    const std::string& name,
    DeviceInfo::OsType os_type,
    const std::string& manufacturer_name,
    const std::string& model_name,
    std::optional<DeviceInfo::DeviceType> device_type = std::nullopt,
    std::optional<DeviceInfo::FormFactor> form_factor = std::nullopt) {
  TestDeviceInfoBuilder builder(os_type);
  builder.WithGuid(id)
      .WithClientName(name)
      .WithManufacturerName(manufacturer_name)
      .WithModelName(model_name);
  if (device_type) {
    builder.WithDeviceType(*device_type);
  }
  if (form_factor) {
    builder.WithFormFactor(*form_factor);
  }
  return builder.Build();
}

// Tests that macOS devices with a model-based `client_name` strip the model
// number suffix in `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_AppleDevices_SigninOnly) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "MacbookPro1,1", DeviceInfo::OsType::kMac,
                           "Apple Inc.", "MacbookPro1,1");

  EXPECT_EQ("MacbookPro", GetDeviceDisplayName(device.get()));
}

// Tests that macOS devices with a custom `client_name` preserve that custom
// name in `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_AppleDevices_FullySynced) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "Bobs-iMac", DeviceInfo::OsType::kMac,
                           "Apple Inc.", "MacbookPro1,1");

  EXPECT_EQ("Bobs-iMac", GetDeviceDisplayName(device.get()));
}

// Tests that iOS devices with a generic `"iPhone"` `client_name` fall back to
// the stripped model prefix in `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_IOS_GenericName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "iPhone", DeviceInfo::OsType::kIOS, "Apple Inc.", "iPhone14,5");

  EXPECT_EQ("iPhone", GetDeviceDisplayName(device.get()));
}

// Tests that iOS devices with a custom `client_name` preserve that custom name
// in `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_IOS_CustomName) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "John's iPhone", DeviceInfo::OsType::kIOS,
                           "Apple Inc.", "iPhone14,5");

  EXPECT_EQ("John's iPhone", GetDeviceDisplayName(device.get()));
}

// Tests that when `client_name` is empty, `GetDeviceDisplayName()` falls back
// to the manufacturer and form factor format.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_EmptyClientName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "", DeviceInfo::OsType::kWindows, "Dell", "XPS 13");

  EXPECT_EQ("Dell Computer", GetDeviceDisplayName(device.get()));
}

// Tests that ChromeOS devices combine the capitalized manufacturer and model
// name in `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_ChromeOSDevices) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Chromebook", DeviceInfo::OsType::kChromeOsAsh, "Google",
      "Chromebook");

  EXPECT_EQ("Google Chromebook", GetDeviceDisplayName(device.get()));
}

// Tests that Android phones without a custom `client_name` format the display
// name using the manufacturer and phone form factor.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_AndroidPhones) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel 2", DeviceInfo::OsType::kAndroid, "Google", "Pixel 2");

  EXPECT_EQ("Google Phone", GetDeviceDisplayName(device.get()));
}

// Tests that Android tablets without a custom `client_name` format the display
// name using the manufacturer and tablet form factor.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_AndroidTablets) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel C", DeviceInfo::OsType::kAndroid, "Google", "Pixel C",
      DeviceInfo::DeviceType::kTablet, DeviceInfo::FormFactor::kTablet);

  EXPECT_EQ("Google Tablet", GetDeviceDisplayName(device.get()));
}

// Tests that Windows devices where `client_name` equals `model_name` fall back
// to the manufacturer and computer form factor.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Windows_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BX123", DeviceInfo::OsType::kWindows, "Dell", "BX123");

  EXPECT_EQ("Dell Computer", GetDeviceDisplayName(device.get()));
}

// Tests that Windows devices with a non-autogenerated `client_name` preserve
// that name in `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Windows_FullySynced) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BOBS-WINDOWS-1", DeviceInfo::OsType::kWindows, "Dell", "BX123");

  EXPECT_EQ("BOBS-WINDOWS-1", GetDeviceDisplayName(device.get()));
}

// Tests that Windows devices with an autogenerated `"DESKTOP-"` `client_name`
// format the display name as `"<Manufacturer> Desktop"`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Windows_GenericDesktop) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "DESKTOP-R5U8O1I",
                           DeviceInfo::OsType::kWindows, "Dell", "XPS 13");

  EXPECT_EQ("Dell Desktop", GetDeviceDisplayName(device.get()));
}

// Tests that Windows devices with an autogenerated `"LAPTOP-"` `client_name`
// format the display name as `"<Manufacturer> Laptop"`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Windows_GenericLaptop) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "LAPTOP-R5U8O1IS",
                           DeviceInfo::OsType::kWindows, "Dell", "XPS 13");

  EXPECT_EQ("Dell Laptop", GetDeviceDisplayName(device.get()));
}

// Tests that Windows devices with a user-defined `client_name` preserve that
// custom name in `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Windows_CustomName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "My Work PC", DeviceInfo::OsType::kWindows, "Dell", "XPS 13");

  EXPECT_EQ("My Work PC", GetDeviceDisplayName(device.get()));
}

// Tests edge cases of Windows autogenerated `client_name` detection in
// `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Windows_GenericNameEdgeCases) {
  std::string manufacturer = "Dell";
  std::string model = "XPS 13";

  // Missing hyphen should NOT match.
  {
    std::unique_ptr<DeviceInfo> device =
        CreateFakeDeviceInfo("guid", "DESKTOP123", DeviceInfo::OsType::kWindows,
                             manufacturer, model);
    EXPECT_EQ("DESKTOP123", GetDeviceDisplayName(device.get()));
  }

  // Prefix in the middle should NOT match.
  {
    std::unique_ptr<DeviceInfo> device =
        CreateFakeDeviceInfo("guid", "MY-DESKTOP-123",
                             DeviceInfo::OsType::kWindows, manufacturer, model);
    EXPECT_EQ("MY-DESKTOP-123", GetDeviceDisplayName(device.get()));
  }

  // Lowercase prefix should NOT match (case-sensitive).
  {
    std::unique_ptr<DeviceInfo> device =
        CreateFakeDeviceInfo("guid", "desktop-123",
                             DeviceInfo::OsType::kWindows, manufacturer, model);
    EXPECT_EQ("desktop-123", GetDeviceDisplayName(device.get()));
  }

  // Fits the pattern (up to 7 chars prefix, dash, 7+ chars suffix, all
  // uppercase alnum).
  {
    std::unique_ptr<DeviceInfo> device =
        CreateFakeDeviceInfo("guid", "ABCDEFG-1234567",
                             DeviceInfo::OsType::kWindows, manufacturer, model);
    EXPECT_EQ("Dell Computer", GetDeviceDisplayName(device.get()));
  }

  // Too long prefix (8 chars, total 15, suffix 6) -> should NOT match
  // (`hyphen_pos > 7`).
  {
    std::unique_ptr<DeviceInfo> device =
        CreateFakeDeviceInfo("guid", "ABCDEFGH-123456",
                             DeviceInfo::OsType::kWindows, manufacturer, model);
    EXPECT_EQ("ABCDEFGH-123456", GetDeviceDisplayName(device.get()));
  }

  // Too short suffix (6 chars, total 14) -> should NOT match (invalid length).
  {
    std::unique_ptr<DeviceInfo> device =
        CreateFakeDeviceInfo("guid", "ABCDEFG-123456",
                             DeviceInfo::OsType::kWindows, manufacturer, model);
    EXPECT_EQ("ABCDEFG-123456", GetDeviceDisplayName(device.get()));
  }

  // Total length too long (16 chars: 7 + 1 + 8) -> should NOT match (invalid
  // length).
  {
    std::unique_ptr<DeviceInfo> device =
        CreateFakeDeviceInfo("guid", "ABCDEFG-12345678",
                             DeviceInfo::OsType::kWindows, manufacturer, model);
    EXPECT_EQ("ABCDEFG-12345678", GetDeviceDisplayName(device.get()));
  }

  // Special characters in prefix (total 15: 5 prefix + 1 dash + 9 suffix) ->
  // should NOT match (invalid chars).
  {
    std::unique_ptr<DeviceInfo> device =
        CreateFakeDeviceInfo("guid", "AB_CD-123456789",
                             DeviceInfo::OsType::kWindows, manufacturer, model);
    EXPECT_EQ("AB_CD-123456789", GetDeviceDisplayName(device.get()));
  }
}

// Tests that a generic Windows auto-generated name (e.g. `"JOHN-R5U8O1I234"`)
// is correctly identified as low quality and simplified.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Windows_GenericAutogenerated) {
  // `"JOHN-R5U8O1I234"` (15 chars) fits the pattern of auto-generated names.
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "JOHN-R5U8O1I234",
                           DeviceInfo::OsType::kWindows, "Dell", "XPS 13");

  EXPECT_EQ("Dell Computer", GetDeviceDisplayName(device.get()));
}

// Tests that Linux devices where `client_name` matches `model_name` fall back
// to the manufacturer and computer form factor.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Linux_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "30BDS0RA0G", DeviceInfo::OsType::kLinux, "LENOVO", "30BDS0RA0G");

  EXPECT_EQ("Lenovo Computer", GetDeviceDisplayName(device.get()));
}

// Tests that Linux devices with a custom hostname preserve `client_name` in
// `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_Linux_FullySynced) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "bob.chromium.org",
                           DeviceInfo::OsType::kLinux, "LENOVO", "30BDS0RA0G");

  EXPECT_EQ("bob.chromium.org", GetDeviceDisplayName(device.get()));
}

// Tests that manufacturer names are title-cased across ASCII and Unicode inputs
// in `GetDeviceDisplayName()`.
TEST_F(DeviceNameUtilTest, CheckManufacturerNameCapitalization) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "model", DeviceInfo::OsType::kWindows, "foo bar", "model");
  EXPECT_EQ("Foo Bar Computer", GetDeviceDisplayName(device.get()));

  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "foo1bar", "model");
  EXPECT_EQ("Foo1bar Computer", GetDeviceDisplayName(device.get()));

  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "foo_bar-FOO", "model");
  EXPECT_EQ("Foo_bar-Foo Computer", GetDeviceDisplayName(device.get()));

  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "foo&bar foo", "model");
  EXPECT_EQ("Foo&Bar Foo Computer", GetDeviceDisplayName(device.get()));

  // Non-ASCII manufacturer names without casing (e.g. Chinese) should be
  // returned as-is.
  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "电子产品", "model");
  EXPECT_EQ("电子产品 Computer", GetDeviceDisplayName(device.get()));

  // Non-ASCII manufacturer names with casing (e.g. Cyrillic) should be
  // capitalized.
  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "иван", "model");
  EXPECT_EQ("Иван Computer", GetDeviceDisplayName(device.get()));
}

constexpr char kSamsungManufacturer[] = "Samsung";
constexpr char kGalaxyS22UltraMarketingName[] = "Galaxy S22 Ultra";
constexpr char kGalaxyS22UltraModel[] = "SM-S908U";

constexpr char kGoogleManufacturer[] = "Google";
constexpr char kPixel9Name[] = "Pixel 9";

constexpr char kAppleManufacturer[] = "Apple Inc.";
constexpr char kIPhone13MarketingName[] = "iPhone 13";
constexpr char kIPhone13Model[] = "iPhone14,5";

// Tests that if a server-determined marketing name is available, it is used as
// the display name, bypassing platform-specific fallback rules (such as
// Android's generic `"Manufacturer Phone"` fallback and iOS's model-prefix
// parsing).
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_WithServerDeterminedName) {
  // Case 1: Android Phone where model differs from marketing name.
  {
    TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
    builder.WithGuid("guid1")
        .WithClientName(kGalaxyS22UltraModel)
        .WithManufacturerName(kSamsungManufacturer)
        .WithModelName(kGalaxyS22UltraModel)
        .WithServerDeterminedModelName(kGalaxyS22UltraMarketingName);
    std::unique_ptr<DeviceInfo> device = builder.Build();

    EXPECT_EQ(kGalaxyS22UltraMarketingName, GetDeviceDisplayName(device.get()));
  }

  // Case 2: Android Phone where model is the same as marketing name.
  {
    TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
    builder.WithGuid("guid2")
        .WithClientName(kPixel9Name)
        .WithManufacturerName(kGoogleManufacturer)
        .WithModelName(kPixel9Name)
        .WithServerDeterminedModelName(kPixel9Name);
    std::unique_ptr<DeviceInfo> device = builder.Build();

    EXPECT_EQ(kPixel9Name, GetDeviceDisplayName(device.get()));
  }

  // Case 3: iOS Phone. Bypasses Apple prefix parsing.
  {
    TestDeviceInfoBuilder builder(DeviceInfo::OsType::kIOS);
    builder.WithGuid("guid3")
        .WithClientName("iPhone")
        .WithManufacturerName(kAppleManufacturer)
        .WithModelName(kIPhone13Model)
        .WithServerDeterminedModelName(kIPhone13MarketingName);
    std::unique_ptr<DeviceInfo> device = builder.Build();

    EXPECT_EQ(kIPhone13MarketingName, GetDeviceDisplayName(device.get()));
  }
}

// Tests that server-determined marketing names always take precedence over
// user-defined custom names (high quality client names).
TEST_F(DeviceNameUtilTest,
       GetDeviceDisplayName_ServerDeterminedNameOverridesCustomName) {
  TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
  builder.WithGuid("guid1")
      .WithClientName("My Work Phone")  // Custom high-quality name
      .WithManufacturerName(kSamsungManufacturer)
      .WithModelName(kGalaxyS22UltraModel)
      .WithServerDeterminedModelName(kGalaxyS22UltraMarketingName);
  std::unique_ptr<DeviceInfo> device = builder.Build();

  EXPECT_EQ(kGalaxyS22UltraMarketingName, GetDeviceDisplayName(device.get()));
}

// Tests that if the server-determined name is missing (`std::nullopt`),
// `GetDeviceDisplayName()` falls back to the standard naming logic.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_NoServerDeterminedName) {
  TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
  builder.WithGuid("guid1")
      .WithClientName(kGalaxyS22UltraModel)
      .WithManufacturerName(kSamsungManufacturer)
      .WithModelName(kGalaxyS22UltraModel);
  std::unique_ptr<DeviceInfo> device = builder.Build();

  EXPECT_EQ("Samsung Phone", GetDeviceDisplayName(device.get()));
}

// Tests that if the server-determined name is empty, `GetDeviceDisplayName()`
// safely falls back to the standard naming logic.
TEST_F(DeviceNameUtilTest, GetDeviceDisplayName_EmptyServerDeterminedName) {
  TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
  builder.WithGuid("guid1")
      .WithClientName(kGalaxyS22UltraModel)
      .WithManufacturerName(kSamsungManufacturer)
      .WithModelName(kGalaxyS22UltraModel)
      .WithServerDeterminedModelName("");
  std::unique_ptr<DeviceInfo> device = builder.Build();

  EXPECT_EQ("Samsung Phone", GetDeviceDisplayName(device.get()));
}

// Tests that `GetDeviceDisplayNames()` returns base display names without
// channel labels when `kSyncDisambiguateDeviceNamesWithChannel` is disabled.
TEST_F(DeviceNameUtilTest,
       GetDeviceDisplayNames_ChannelDisambiguationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kSyncDisambiguateDeviceNamesWithChannel);

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

  EXPECT_THAT(GetDeviceDisplayNames({stable_device.get(), canary_device.get()}),
              ElementsAre("Google Phone", "Google Phone"));
}

class GetDeviceDisplayNamesTest : public Test {
 public:
  GetDeviceDisplayNamesTest() {
    scoped_feature_list_.InitAndEnableFeature(
        syncer::kSyncDisambiguateDeviceNamesWithChannel);
  }
  ~GetDeviceDisplayNamesTest() override = default;

  std::string GetDisambiguatedDisplayName(
      const DeviceInfo* target,
      const std::vector<const DeviceInfo*>& devices,
      const DeviceInfo* local_device = nullptr) {
    std::vector<std::string> names =
        GetDeviceDisplayNames(devices, local_device);
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
TEST_F(GetDeviceDisplayNamesTest, UniqueDevices_ReturnsBaseName) {
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
TEST_F(GetDeviceDisplayNamesTest,
       DuplicateBaseNameDifferentChannels_ReturnsChannelLabel) {
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
TEST_F(GetDeviceDisplayNamesTest,
       RemoteCollidesWithLocalStableDevice_ReturnsBaseNameOrLabel) {
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
TEST_F(GetDeviceDisplayNamesTest,
       MultipleChannelsAndProfiles_DisambiguatesCorrectly) {
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
TEST_F(GetDeviceDisplayNamesTest, EmptyUserAgent_ReturnsBaseName) {
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
TEST_F(GetDeviceDisplayNamesTest,
       CollidingNonStableProfiles_ReturnsChannelLabel) {
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
TEST_F(GetDeviceDisplayNamesTest, NullDevicePointersInList_SafelyIgnored) {
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

// Tests that `GetDeviceDisplayNames()` resolves duplicate names across
// different release channels and preserves order.
TEST_F(GetDeviceDisplayNamesTest, DuplicateDevices_ReturnsDisambiguatedNames) {
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
      GetDeviceDisplayNames({stable1.get(), stable2.get(), canary.get()},
                            /*local_device=*/nullptr);

  EXPECT_THAT(result, ElementsAre("Google Phone", "Google Phone",
                                  "Google Phone (Canary)"));
}

}  // namespace
}  // namespace syncer
