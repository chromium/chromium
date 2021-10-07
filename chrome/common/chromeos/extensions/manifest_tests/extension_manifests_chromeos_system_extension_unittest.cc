// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extensions_manifest_constants.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using ExtensionManifestChromeOSSystemExtensionTest = ChromeManifestTest;

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidChromeOSSystemExtension) {
  LoadAndExpectError("chromeos_system_extension_invalid.json",
                     chromeos::kInvalidChromeOSSystemExtensionId);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       InvalidSerialNumberPermissionInRequiredPermissions) {
  LoadAndExpectError(
      "chromeos_system_extension_invalid_serial_number_permission.json",
      chromeos::kSerialNumberPermissionMustBeOptional);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension.json"));
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
}  // namespace chromeos
