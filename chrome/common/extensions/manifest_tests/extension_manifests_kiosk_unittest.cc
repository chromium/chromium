// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using ExtensionManifestKioskModeTest = ChromeManifestTest;

TEST_F(ExtensionManifestKioskModeTest, InvalidKioskEnabled) {
  LoadAndExpectError("kiosk_enabled_invalid.json",
                     manifest_errors::kInvalidKioskEnabled);
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledHostedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_hosted_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledPackagedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_packaged_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledExtension) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_extension.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_platform_app.json"));
  EXPECT_TRUE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskDisabledPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_disabled_platform_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskDefaultPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_default_platform_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskEnabled(extension.get()));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskEnabledDefaultRequired) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_enabled_platform_app.json"));
  EXPECT_TRUE(KioskModeInfo::IsKioskEnabled(extension.get()));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

// 'kiosk_only' key should be set only from ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ExtensionManifestKioskModeTest, KioskOnlyPlatformApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_only_platform_app.json"));
  EXPECT_TRUE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyInvalid) {
  LoadAndExpectError("kiosk_only_invalid.json",
                     manifest_errors::kInvalidKioskOnly);
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyButNotEnabled) {
  LoadAndExpectError("kiosk_only_not_enabled.json",
                     manifest_errors::kInvalidKioskOnlyButNotEnabled);
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyHostedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_only_hosted_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyPackagedApp) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_only_packaged_app.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}

TEST_F(ExtensionManifestKioskModeTest, KioskOnlyExtension) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("kiosk_only_extension.json"));
  EXPECT_FALSE(KioskModeInfo::IsKioskOnly(extension.get()));
}
#else
TEST_F(ExtensionManifestKioskModeTest, KioskOnlyFromNonChromeos) {
  LoadAndExpectWarning("kiosk_only_platform_app.json",
                       "'kiosk_only' is not allowed for specified platform.");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
