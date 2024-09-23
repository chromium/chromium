// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos {
namespace {

using ExtensionManifestChromeOSSystemExtensionTest = ChromeManifestTest;

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidChromeOSSystemExtension) {
  LoadAndExpectWarning("chromeos_system_extension_invalid.json",
                       kInvalidChromeOSSystemExtensionId);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ChromeOSSystemExtensionDevIsDisabled) {
  // This is handled by manifest handler, so it is error, not warning.
  LoadAndExpectError("chromeos_system_extension_google_dev.json",
                     kInvalidChromeOSSystemExtensionId);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_Google) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_HP) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_hp.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_ASUS) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_asus.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidNonChromeOSSystemExtension) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  EXPECT_FALSE(extension->is_chromeos_system_extension());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableEmpty) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_empty.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableIds) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_ids.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableTls) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_tls.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesMoreThanOne) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_2_origins.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesEmpty) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_matches_empty."
      "json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesDisallowedOrigin) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_1_origin.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableNotExist) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_not_exist.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_Google_IWA) {
  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionIwaIdOverrideForTesting,
      "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic");
  scoped_info->ApplyCommandLineSwitchesForTesting();

  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google_iwa.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ChromeOSSystemExtensionDevIsEnabled) {
  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates({
      {ash::features::kShimlessRMA3pDiagnosticsDevMode, true},
  });
  scoped_info->ApplyCommandLineSwitchesForTesting();

  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google_dev.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
}  // namespace chromeos
