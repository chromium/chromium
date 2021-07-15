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
                     chromeos::kInvalidChromeOSSystemExtensionDeclaration);
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidChromeOSSystemExtension) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("chromeos_system_extension.json"));
  EXPECT_TRUE(extension->is_chromeos_system_extension());
}

TEST_F(ExtensionManifestChromeOSSystemExtensionTest,
       ValidNonChromeOSSystemExtension) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  EXPECT_FALSE(extension->is_chromeos_system_extension());
}

}  // namespace chromeos
