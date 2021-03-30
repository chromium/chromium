// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

TEST(OsSettingsSectionTest, SectionWithFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kOsSettingsDeepLinking);

  // Sections should not incur modification.
  EXPECT_EQ("internet", OsSettingsSection::GetDefaultModifiedUrl(
                            /*type=*/mojom::SearchResultType::kSection,
                            /*id=*/{.section = mojom::Section::kNetwork},
                            /*url_to_modify=*/"internet"));
}

TEST(OsSettingsSectionTest, SectionNoFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      chromeos::features::kOsSettingsDeepLinking);

  // Deep linking disabled, should not modify.
  EXPECT_EQ("internet", OsSettingsSection::GetDefaultModifiedUrl(
                            /*type=*/mojom::SearchResultType::kSection,
                            /*id=*/{.section = mojom::Section::kNetwork},
                            /*url_to_modify=*/"internet"));
}

TEST(OsSettingsSectionTest, SubpageWithFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kOsSettingsDeepLinking);

  // Subpages should not incur modification.
  EXPECT_EQ("networks?type=WiFi",
            OsSettingsSection::GetDefaultModifiedUrl(
                /*type=*/mojom::SearchResultType::kSubpage,
                /*id=*/{.subpage = mojom::Subpage::kWifiNetworks},
                /*url_to_modify=*/"networks?type=WiFi"));
}

TEST(OsSettingsSectionTest, SubpageNoFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      chromeos::features::kOsSettingsDeepLinking);

  // Deep linking disabled, should not modify.
  EXPECT_EQ("networks?type=WiFi",
            OsSettingsSection::GetDefaultModifiedUrl(
                /*type=*/mojom::SearchResultType::kSubpage,
                /*id=*/{.subpage = mojom::Subpage::kWifiNetworks},
                /*url_to_modify=*/"networks?type=WiFi"));
}

TEST(OsSettingsSectionTest, SettingWithFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kOsSettingsDeepLinking);

  // Settings should have settingId added
  EXPECT_EQ("networks?settingId=4",
            OsSettingsSection::GetDefaultModifiedUrl(
                /*type=*/mojom::SearchResultType::kSetting,
                /*id=*/{.setting = mojom::Setting::kWifiOnOff},
                /*url_to_modify=*/"networks"));
}

TEST(OsSettingsSectionTest, SettingExistingQueryWithFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kOsSettingsDeepLinking);

  // Settings with existing query parameters should have settingId added.
  EXPECT_EQ("networks?type=WiFi&settingId=4",
            OsSettingsSection::GetDefaultModifiedUrl(
                /*type=*/mojom::SearchResultType::kSetting,
                /*id=*/{.setting = mojom::Setting::kWifiOnOff},
                /*url_to_modify=*/"networks?type=WiFi"));
}

TEST(OsSettingsSectionTest, SettingNoFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      chromeos::features::kOsSettingsDeepLinking);

  // Deep linking disabled, should not modify.
  EXPECT_EQ("networks?type=WiFi",
            OsSettingsSection::GetDefaultModifiedUrl(
                /*type=*/mojom::SearchResultType::kSetting,
                /*id=*/{.setting = mojom::Setting::kWifiOnOff},
                /*url_to_modify=*/"networks?type=WiFi"));
}

}  // namespace settings
}  // namespace chromeos
