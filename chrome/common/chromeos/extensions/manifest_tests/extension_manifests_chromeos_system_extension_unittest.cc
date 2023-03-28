// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using ExtensionManifestChromeOSSystemExtensionTest = ChromeManifestTest;

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension_Invalid_Permission_Allowlisted_Google) {
  LoadAndExpectWarning(
      "chromeos_system_extension_google.json",
      "'os.events' requires the 'TelemetryExtensionPendingApprovalApi' feature "
      "flag to be enabled.");
}

class ExtensionManifestChromeOSSystemExtensionTestPendingApproval
    : public ExtensionManifestChromeOSSystemExtensionTest {
 public:
  ExtensionManifestChromeOSSystemExtensionTestPendingApproval() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kTelemetryExtensionPendingApprovalApi);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       InvalidChromeOSSystemExtension) {
  LoadAndExpectWarning(
      "chromeos_system_extension_invalid.json",
      "'chromeos_system_extension' is not allowed for specified extension ID.");
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       ValidChromeOSSystemExtension_Allowlisted_Google) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_google.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       ValidChromeOSSystemExtension_Allowlisted_HP) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_hp.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       ValidChromeOSSystemExtension_Allowlisted_ASUS) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension_asus.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
  EXPECT_TRUE(extension->install_warnings().empty());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       ValidNonChromeOSSystemExtension) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  EXPECT_FALSE(extension->is_chromeos_system_extension());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       InvalidExternallyConnectableEmpty) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_empty.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       InvalidExternallyConnectableIds) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_ids.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       InvalidExternallyConnectableTls) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_tls.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       InvalidExternallyConnectableMatchesMoreThanOne) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_2_origins.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       InvalidExternallyConnectableMatchesEmpty) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_matches_empty."
      "json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       InvalidExternallyConnectableMatchesDisallowedOrigin) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_1_origin.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTestPendingApproval,
       InvalidExternallyConnectableNotExist) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_externally_connectable_not_exist.json",
      chromeos::kInvalidExternallyConnectableDeclaration);
}
}  // namespace chromeos
