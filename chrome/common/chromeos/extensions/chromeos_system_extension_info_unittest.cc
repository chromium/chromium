// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"

#include "base/command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeOSSystemExtensionInfo, CheckAllowlistedExtensionsSize) {
  ASSERT_EQ(3u, chromeos::GetChromeOSSystemExtensionInfosSize());
}

TEST(ChromeOSSystemExtensionInfo, GoogleExtension) {
  const auto& google_extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  ASSERT_TRUE(chromeos::IsChromeOSSystemExtension(google_extension_id));

  const auto& extension_info =
      chromeos::GetChromeOSExtensionInfoForId(google_extension_id);
  EXPECT_THAT(extension_info.manufacturers,
              testing::UnorderedElementsAre("ASUS", "HP"));
  EXPECT_EQ("*://googlechromelabs.github.io/*", extension_info.pwa_origin);
}

TEST(ChromeOSSystemExtensionInfo, HPExtension) {
  const auto& hp_extension_id = "alnedpmllcfpgldkagbfbjkloonjlfjb";
  ASSERT_TRUE(chromeos::IsChromeOSSystemExtension(hp_extension_id));

  const auto extension_info =
      chromeos::GetChromeOSExtensionInfoForId(hp_extension_id);
  EXPECT_THAT(extension_info.manufacturers,
              testing::UnorderedElementsAre("HP"));
  EXPECT_EQ("https://hpcs-appschr.hpcloud.hp.com/*", extension_info.pwa_origin);
}

TEST(ChromeOSSystemExtensionInfo, ASUSExtension) {
  const auto& asus_extension_id = "hdnhcpcfohaeangjpkcjkgmgmjanbmeo";
  ASSERT_TRUE(chromeos::IsChromeOSSystemExtension(asus_extension_id));

  const auto extension_info =
      chromeos::GetChromeOSExtensionInfoForId(asus_extension_id);
  EXPECT_THAT(extension_info.manufacturers,
              testing::UnorderedElementsAre("ASUS"));
  EXPECT_EQ("https://dlcdnccls.asus.com/*", extension_info.pwa_origin);
}

TEST(ChromeOSSystemExtensionInfo, ManufacturerOverride) {
  constexpr char kManufacturerOverride[] = "TEST_OEM";

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionManufacturerOverrideForTesting,
      kManufacturerOverride);

  const auto google_extension_info = chromeos::GetChromeOSExtensionInfoForId(
      "gogonhoemckpdpadfnjnpgbjpbjnodgc");
  EXPECT_EQ("*://googlechromelabs.github.io/*",
            google_extension_info.pwa_origin);
  EXPECT_THAT(google_extension_info.manufacturers,
              testing::UnorderedElementsAre(kManufacturerOverride));

  const auto hp_extension_info = chromeos::GetChromeOSExtensionInfoForId(
      "alnedpmllcfpgldkagbfbjkloonjlfjb");
  EXPECT_EQ("https://hpcs-appschr.hpcloud.hp.com/*",
            hp_extension_info.pwa_origin);
  EXPECT_THAT(hp_extension_info.manufacturers,
              testing::UnorderedElementsAre(kManufacturerOverride));
}

TEST(ChromeOSSystemExtensionInfo, PwaOriginOverride) {
  constexpr char kPwaOriginOverride[] = "*://pwa.website.com/*";

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionPwaOriginOverrideForTesting,
      kPwaOriginOverride);

  const auto google_extension_info = chromeos::GetChromeOSExtensionInfoForId(
      "gogonhoemckpdpadfnjnpgbjpbjnodgc");
  EXPECT_EQ(kPwaOriginOverride, google_extension_info.pwa_origin);
  EXPECT_THAT(google_extension_info.manufacturers,
              testing::UnorderedElementsAre("HP", "ASUS"));

  const auto hp_extension_info = chromeos::GetChromeOSExtensionInfoForId(
      "alnedpmllcfpgldkagbfbjkloonjlfjb");
  EXPECT_EQ(kPwaOriginOverride, hp_extension_info.pwa_origin);
  EXPECT_THAT(hp_extension_info.manufacturers,
              testing::UnorderedElementsAre("HP"));
}
