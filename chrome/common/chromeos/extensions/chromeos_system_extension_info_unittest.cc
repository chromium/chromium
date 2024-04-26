// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST(ChromeOSSystemExtensionInfo, GoogleExtension) {
  const auto& google_extension_id = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
  ASSERT_TRUE(chromeos::IsChromeOSSystemExtension(google_extension_id));

  const auto& extension_info =
      chromeos::GetChromeOSExtensionInfoById(google_extension_id);
  EXPECT_THAT(extension_info.manufacturers,
              testing::UnorderedElementsAre("ASUS", "HP"));
  EXPECT_EQ("*://googlechromelabs.github.io/*", extension_info.pwa_origin);
  EXPECT_FALSE(extension_info.iwa_id);
}

TEST(ChromeOSSystemExtensionInfo, HPExtension) {
  const auto& hp_extension_id = "alnedpmllcfpgldkagbfbjkloonjlfjb";
  ASSERT_TRUE(chromeos::IsChromeOSSystemExtension(hp_extension_id));

  const auto& extension_info =
      chromeos::GetChromeOSExtensionInfoById(hp_extension_id);
  EXPECT_THAT(extension_info.manufacturers,
              testing::UnorderedElementsAre("HP"));
  EXPECT_EQ("https://hpcs-appschr.hpcloud.hp.com/*", extension_info.pwa_origin);
  EXPECT_FALSE(extension_info.iwa_id);
}

TEST(ChromeOSSystemExtensionInfo, ASUSExtension) {
  const auto& asus_extension_id = "hdnhcpcfohaeangjpkcjkgmgmjanbmeo";
  ASSERT_TRUE(chromeos::IsChromeOSSystemExtension(asus_extension_id));

  const auto& extension_info =
      chromeos::GetChromeOSExtensionInfoById(asus_extension_id);
  EXPECT_THAT(extension_info.manufacturers,
              testing::UnorderedElementsAre("ASUS"));
  EXPECT_EQ("https://dlcdnccls.asus.com/*", extension_info.pwa_origin);
  EXPECT_FALSE(extension_info.iwa_id);
}

TEST(ChromeOSSystemExtensionInfo, DevExtension) {
  ASSERT_FALSE(chromeos::IsChromeOSSystemExtension(
      chromeos::kChromeOSSystemExtensionDevExtensionId));

  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kShimlessRMA3pDiagnosticsDevMode);
  scoped_info->ApplyCommandLineSwitchesForTesting();

  ASSERT_TRUE(chromeos::IsChromeOSSystemExtension(
      chromeos::kChromeOSSystemExtensionDevExtensionId));
  const auto& extension_info = chromeos::GetChromeOSExtensionInfoById(
      chromeos::kChromeOSSystemExtensionDevExtensionId);
  EXPECT_TRUE(extension_info.manufacturers.contains("Google"));
  EXPECT_TRUE(extension_info.pwa_origin);
  EXPECT_TRUE(extension_info.iwa_id);
}

TEST(ChromeOSSystemExtensionInfo, ManufacturerOverride) {
  constexpr char kManufacturerOverride[] = "TEST_OEM";

  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionManufacturerOverrideForTesting,
      kManufacturerOverride);
  scoped_info->ApplyCommandLineSwitchesForTesting();

  const auto& google_extension_info = chromeos::GetChromeOSExtensionInfoById(
      "gogonhoemckpdpadfnjnpgbjpbjnodgc");
  EXPECT_EQ("*://googlechromelabs.github.io/*",
            google_extension_info.pwa_origin);
  EXPECT_THAT(google_extension_info.manufacturers,
              testing::UnorderedElementsAre(kManufacturerOverride));
  EXPECT_FALSE(google_extension_info.iwa_id);

  const auto& hp_extension_info = chromeos::GetChromeOSExtensionInfoById(
      "alnedpmllcfpgldkagbfbjkloonjlfjb");
  EXPECT_EQ("https://hpcs-appschr.hpcloud.hp.com/*",
            hp_extension_info.pwa_origin);
  EXPECT_THAT(hp_extension_info.manufacturers,
              testing::UnorderedElementsAre(kManufacturerOverride));
  EXPECT_FALSE(hp_extension_info.iwa_id);
}

TEST(ChromeOSSystemExtensionInfo, PwaOriginOverride) {
  constexpr char kPwaOriginOverride[] = "*://pwa.website.com/*";

  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionPwaOriginOverrideForTesting,
      kPwaOriginOverride);
  scoped_info->ApplyCommandLineSwitchesForTesting();

  const auto& google_extension_info = chromeos::GetChromeOSExtensionInfoById(
      "gogonhoemckpdpadfnjnpgbjpbjnodgc");
  EXPECT_EQ(kPwaOriginOverride, google_extension_info.pwa_origin);
  EXPECT_THAT(google_extension_info.manufacturers,
              testing::UnorderedElementsAre("HP", "ASUS"));
  EXPECT_FALSE(google_extension_info.iwa_id);

  const auto& hp_extension_info = chromeos::GetChromeOSExtensionInfoById(
      "alnedpmllcfpgldkagbfbjkloonjlfjb");
  EXPECT_EQ(kPwaOriginOverride, hp_extension_info.pwa_origin);
  EXPECT_THAT(hp_extension_info.manufacturers,
              testing::UnorderedElementsAre("HP"));
  EXPECT_FALSE(hp_extension_info.iwa_id);
}

TEST(ChromeOSSystemExtensionInfo, IwaIdOverride) {
  constexpr char kIwaIdOverride[] =
      "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic";

  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionIwaIdOverrideForTesting,
      kIwaIdOverride);
  scoped_info->ApplyCommandLineSwitchesForTesting();

  const auto& google_extension_info = chromeos::GetChromeOSExtensionInfoById(
      "gogonhoemckpdpadfnjnpgbjpbjnodgc");
  EXPECT_EQ("*://googlechromelabs.github.io/*",
            google_extension_info.pwa_origin);
  EXPECT_THAT(google_extension_info.manufacturers,
              testing::UnorderedElementsAre("HP", "ASUS"));
  EXPECT_EQ(kIwaIdOverride, google_extension_info.iwa_id->id());

  const auto& hp_extension_info = chromeos::GetChromeOSExtensionInfoById(
      "alnedpmllcfpgldkagbfbjkloonjlfjb");
  EXPECT_EQ("https://hpcs-appschr.hpcloud.hp.com/*",
            hp_extension_info.pwa_origin);
  EXPECT_THAT(hp_extension_info.manufacturers,
              testing::UnorderedElementsAre("HP"));
  EXPECT_EQ(kIwaIdOverride, hp_extension_info.iwa_id->id());
}

TEST(ChromeOSSystemExtensionInfo, IsChromeOSSystemExtensionProvider) {
  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();

  EXPECT_TRUE(chromeos::IsChromeOSSystemExtensionProvider("HP"));
  EXPECT_TRUE(chromeos::IsChromeOSSystemExtensionProvider("ASUS"));

  EXPECT_FALSE(chromeos::IsChromeOSSystemExtensionProvider("NotAProvider"));
  // "Google" is only for dev extension.
  EXPECT_FALSE(chromeos::IsChromeOSSystemExtensionProvider("Google"));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kShimlessRMA3pDiagnosticsDevMode);
  scoped_info->ApplyCommandLineSwitchesForTesting();
  EXPECT_TRUE(chromeos::IsChromeOSSystemExtensionProvider("Google"));
}

TEST(ChromeOSSystemExtensionInfo, Is3pDiagnosticsIwaId) {
  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  auto dev_iwa_id =
      web_package::SignedWebBundleId::Create(
          "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic")
          .value();
  // The dev IWA ID is only allowed when feature flag is on.
  EXPECT_FALSE(chromeos::Is3pDiagnosticsIwaId(dev_iwa_id));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::features::kShimlessRMA3pDiagnosticsDevMode);
  scoped_info->ApplyCommandLineSwitchesForTesting();
  EXPECT_TRUE(chromeos::Is3pDiagnosticsIwaId(dev_iwa_id));
}
