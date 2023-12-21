// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
const char kChromeUntrustedTestURL[] = "chrome-untrusted://test/";
}  // namespace

namespace errors = manifest_errors;

using ChromePermissionManifestTest = ChromeManifestTest;

TEST_F(ChromePermissionManifestTest, ChromeURLPermissionInvalid) {
  LoadAndExpectWarning(
      "permission_chrome_url_invalid.json",
      ErrorUtils::FormatErrorMessage(errors::kInvalidPermissionScheme,
                                     manifest_keys::kPermissions,
                                     chrome::kChromeUINewTabURL));
}

TEST_F(ChromePermissionManifestTest, ChromeUntrustedURLPermissionInvalid) {
  LoadAndExpectWarning(
      "permission_chrome_untrusted_url_invalid.json",
      ErrorUtils::FormatErrorMessage(errors::kPermissionUnknownOrMalformed,
                                     kChromeUntrustedTestURL));
}

TEST_F(ChromePermissionManifestTest, ChromeURLPermissionAllowedWithFlag) {
  // Ignore the policy delegate for this test.
  PermissionsData::SetPolicyDelegate(nullptr);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  std::string error;
  scoped_refptr<Extension> extension =
    LoadAndExpectSuccess("permission_chrome_url_invalid.json");
  EXPECT_EQ("", error);
  const GURL newtab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(
      extension->permissions_data()->CanAccessPage(newtab_url, 0, &error))
      << error;
}

// Tests that extensions can't access chrome-untrusted:// even with the
// kExtensionsOnChromeURLs flag enabled.
TEST_F(ChromePermissionManifestTest,
       ChromeUntrustedURLPermissionDisallowedWithFlag) {
  // Ignore the policy delegate for this test.
  PermissionsData::SetPolicyDelegate(nullptr);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  LoadAndExpectWarning(
      "permission_chrome_untrusted_url_invalid.json",
      ErrorUtils::FormatErrorMessage(errors::kPermissionUnknownOrMalformed,
                                     kChromeUntrustedTestURL));
}

TEST_F(ChromePermissionManifestTest,
       ChromeResourcesPermissionValidOnlyForComponents) {
  LoadAndExpectWarning("permission_chrome_resources_url.json",
                       ErrorUtils::FormatErrorMessage(
                           errors::kInvalidPermissionScheme,
                           manifest_keys::kPermissions, "chrome://resources/"));
  std::string error;
  LoadExtension(ManifestData("permission_chrome_resources_url.json"), &error,
                extensions::mojom::ManifestLocation::kComponent,
                Extension::NO_FLAGS);
  EXPECT_EQ("", error);
}

}  // namespace extensions
