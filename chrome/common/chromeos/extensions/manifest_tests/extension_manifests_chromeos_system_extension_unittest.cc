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
#include "chromeos/constants/chromeos_features.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos {
namespace {

struct TestParam {
  bool is_iwa_enabled;
  std::string external_connectable_error_message;
};

class ExtensionManifestChromeOSSystemExtensionTest
    : public ChromeManifestTest,
      public testing::WithParamInterface<TestParam> {
 protected:
  void SetUp() override {
    ChromeManifestTest::SetUp();

    feature_list_.InitWithFeatureState(features::kIWAForTelemetryExtensionAPI,
                                       GetParam().is_iwa_enabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidChromeOSSystemExtension) {
  LoadAndExpectWarning("chromeos_system_extension_invalid.json",
                       kInvalidChromeOSSystemExtensionId);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       ChromeOSSystemExtensionDevIsDisabled) {
  // This is handled by manifest handler, so it is error, not warning.
  LoadAndExpectError("chromeos_system_extension_google_dev.json",
                     kInvalidChromeOSSystemExtensionId);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_Google) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_HP) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_hp.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_ASUS) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_asus.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       ValidNonChromeOSSystemExtension) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  EXPECT_FALSE(extension->is_chromeos_system_extension());
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableEmpty) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_empty.json",
      GetParam().external_connectable_error_message);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableIds) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_ids.json",
      GetParam().external_connectable_error_message);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableTls) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_tls.json",
      GetParam().external_connectable_error_message);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesMoreThanOne) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_2_origins.json",
      GetParam().external_connectable_error_message);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesEmpty) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_matches_empty."
      "json",
      GetParam().external_connectable_error_message);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableMatchesDisallowedOrigin) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_1_origin.json",
      GetParam().external_connectable_error_message);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidExternallyConnectableNotExist) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_not_exist.json",
      GetParam().external_connectable_error_message);
}

TEST_P(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Allowlisted_Google_IWA) {
  auto scoped_info =
      chromeos::ScopedChromeOSSystemExtensionInfo::CreateForTesting();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionIwaIdOverrideForTesting,
      "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic");
  scoped_info->ApplyCommandLineSwitchesForTesting();

  if (GetParam().is_iwa_enabled) {
    scoped_refptr<extensions::Extension> extension(
        LoadAndExpectSuccess("chromeos_system_extension_google_iwa.json"));
    EXPECT_TRUE(extension->is_chromeos_system_extension());
    EXPECT_TRUE(extension->install_warnings().empty());
  } else {
    LoadAndExpectError("chromeos_system_extension_google_iwa.json",
                       GetParam().external_connectable_error_message);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionManifestChromeOSSystemExtensionFeatureTest,
    ExtensionManifestChromeOSSystemExtensionTest,
    testing::Values(
        TestParam{
            .is_iwa_enabled = false,
            .external_connectable_error_message =
                chromeos::kInvalidExternallyConnectableDeclaration,
        },
        TestParam{
            .is_iwa_enabled = true,
            .external_connectable_error_message =
                chromeos::kInvalidExternallyConnectableDeclarationWithIWA,
        }));

using ExtensionManifestChromeOSSystemExtensionDevTest = ChromeManifestTest;

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ExtensionManifestChromeOSSystemExtensionDevTest,
       ChromeOSSystemExtensionDevIsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureStates({
      {features::kIWAForTelemetryExtensionAPI, true},
      {ash::features::kShimlessRMA3pDiagnosticsDevMode, true},
  });
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google_dev.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
}  // namespace chromeos
