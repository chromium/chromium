// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using PermissionsParserTest = ChromeManifestTest;

TEST_F(PermissionsParserTest, RemoveOverlappingAPIPermissions) {
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      "permissions_overlapping_api_permissions.json",
      ErrorUtils::FormatErrorMessage(
          manifest_errors::kPermissionMarkedOptionalAndRequired, "tabs")));

  std::set<std::string> required_api_names =
      PermissionsParser::GetRequiredPermissions(extension.get())
          .GetAPIsAsStrings();

  std::set<std::string> optional_api_names =
      PermissionsParser::GetOptionalPermissions(extension.get())
          .GetAPIsAsStrings();

  // Check that required api permissions have not changed while "tabs" is
  // removed from optional permissions as it is specified as required.
  EXPECT_THAT(required_api_names,
              testing::UnorderedElementsAre("tabs", "storage"));
  EXPECT_THAT(optional_api_names, testing::UnorderedElementsAre("geolocation"));
}

TEST_F(PermissionsParserTest, RemoveOverlappingHostPermissions) {
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      "permissions_overlapping_host_permissions.json",
      ErrorUtils::FormatErrorMessage(
          manifest_errors::kPermissionMarkedOptionalAndRequired,
          "https://google.com/*")));

  const URLPatternSet& required_hosts =
      PermissionsParser::GetRequiredPermissions(extension.get())
          .explicit_hosts();

  const URLPatternSet& optional_hosts =
      PermissionsParser::GetOptionalPermissions(extension.get())
          .explicit_hosts();

  // Check that required hosts have not changed at all while
  // "https://google.com/maps" is removed from optional hosts as it is a strict
  // subset of hosts specified as required.
  EXPECT_THAT(*required_hosts.ToStringVector(),
              testing::UnorderedElementsAre("https://example.com/*",
                                            "https://*.google.com/*"));
  EXPECT_THAT(*optional_hosts.ToStringVector(),
              testing::UnorderedElementsAre("*://chromium.org/*"));
}

TEST_F(PermissionsParserTest, RequiredHostPermissionsAllURLs) {
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      "permissions_all_urls_host_permissions.json",
      ErrorUtils::FormatErrorMessage(
          manifest_errors::kPermissionMarkedOptionalAndRequired,
          "http://*/*")));

  const URLPatternSet& optional_hosts =
      PermissionsParser::GetOptionalPermissions(extension.get())
          .explicit_hosts();

  // Since we specified <all_urls> as a required host permission,
  // there should be no optional host permissions.
  EXPECT_THAT(*optional_hosts.ToStringVector(), testing::IsEmpty());
}

TEST_F(PermissionsParserTest, OptionalHostPermissionsAllURLs) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("permissions_optional_all_urls_permissions.json"));

  const URLPatternSet& required_hosts =
      PermissionsParser::GetRequiredPermissions(extension.get())
          .explicit_hosts();

  const URLPatternSet& optional_hosts =
      PermissionsParser::GetOptionalPermissions(extension.get())
          .explicit_hosts();

  // This time, required permissions are a subset of optional permissions
  // so we make sure that permissions remain the same
  // as what's specified in the manifest.
  EXPECT_THAT(*required_hosts.ToStringVector(),
              testing::UnorderedElementsAre("https://*.google.com/*"));

  EXPECT_THAT(*optional_hosts.ToStringVector(),
              testing::UnorderedElementsAre("*://*/*"));
}

}  // namespace extensions
