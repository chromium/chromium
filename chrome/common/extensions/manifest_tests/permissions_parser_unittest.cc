// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/permissions_parser.h"

#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
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
  EXPECT_THAT(optional_api_names, testing::UnorderedElementsAre("bookmarks"));
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
  EXPECT_THAT(required_hosts.ToStringVector(),
              testing::UnorderedElementsAre("https://example.com/*",
                                            "https://*.google.com/*"));
  EXPECT_THAT(optional_hosts.ToStringVector(),
              testing::UnorderedElementsAre("*://chromium.org/*"));
}

// Same as the above test, except host permissions are specified in
// `host_permissions` and `optional_host_permissions` as the extension is
// running Manifest V3.
TEST_F(PermissionsParserTest, RemoveOverlappingHostPermissions_ManifestV3) {
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      "permissions_overlapping_host_permissions_mv3.json",
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
  EXPECT_THAT(required_hosts.ToStringVector(),
              testing::UnorderedElementsAre("https://example.com/*",
                                            "https://*.google.com/*"));
  EXPECT_THAT(optional_hosts.ToStringVector(),
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
  EXPECT_THAT(optional_hosts.ToStringVector(), testing::IsEmpty());
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
  EXPECT_THAT(required_hosts.ToStringVector(),
              testing::UnorderedElementsAre("https://*.google.com/*"));

  EXPECT_THAT(optional_hosts.ToStringVector(),
              testing::UnorderedElementsAre("*://*/*"));
}

TEST_F(PermissionsParserTest, OptionalHostPermissionsInvalidScheme) {
  std::vector<std::string> expected_warnings;
  expected_warnings.push_back(ErrorUtils::FormatErrorMessage(
      manifest_errors::kInvalidPermissionScheme,
      manifest_keys::kOptionalPermissions, "chrome://extensions/"));

  scoped_refptr<Extension> extension(LoadAndExpectWarnings(
      "optional_permissions_invalid_scheme.json", expected_warnings));
}

TEST_F(PermissionsParserTest, HostPermissionsKey) {
  std::vector<std::string> expected_warnings;
  expected_warnings.push_back(ErrorUtils::FormatErrorMessage(
      manifest_errors::kPermissionUnknownOrMalformed, "https://google.com/*"));
  expected_warnings.push_back(ErrorUtils::FormatErrorMessage(
      manifest_errors::kPermissionUnknownOrMalformed, "http://chromium.org/*"));

  scoped_refptr<Extension> extension(
      LoadAndExpectWarnings("host_permissions_key.json", expected_warnings));

  // Expect that the host specified in |host_permissions| is parsed.
  const URLPatternSet& required_hosts =
      PermissionsParser::GetRequiredPermissions(extension.get())
          .explicit_hosts();

  EXPECT_THAT(required_hosts.ToStringVector(),
              testing::UnorderedElementsAre("https://example.com/*"));

  // Expect that the host specified in |optional_host_permissions| is parsed.
  const URLPatternSet& optional_hosts =
      PermissionsParser::GetOptionalPermissions(extension.get())
          .explicit_hosts();

  EXPECT_THAT(optional_hosts.ToStringVector(),
              testing::UnorderedElementsAre("https://optional.com/*"));
}

TEST_F(PermissionsParserTest, HostPermissionsKeyInvalidHosts) {
  std::vector<std::string> expected_warnings;
  expected_warnings.push_back(ErrorUtils::FormatErrorMessage(
      manifest_errors::kPermissionUnknownOrMalformed, "malformed_host"));
  expected_warnings.push_back(ErrorUtils::FormatErrorMessage(
      manifest_errors::kPermissionUnknownOrMalformed,
      "optional_malformed_host"));

  scoped_refptr<Extension> extension(LoadAndExpectWarnings(
      "host_permissions_key_invalid_hosts.json", expected_warnings));
}

TEST_F(PermissionsParserTest, HostPermissionsKeyInvalidScheme) {
  std::vector<std::string> expected_warnings;
  expected_warnings.push_back(ErrorUtils::FormatErrorMessage(
      manifest_errors::kInvalidPermissionScheme,
      manifest_keys::kHostPermissions, "chrome://extensions/"));
  expected_warnings.push_back(ErrorUtils::FormatErrorMessage(
      manifest_errors::kInvalidPermissionScheme,
      manifest_keys::kOptionalHostPermissions, "chrome://settings/"));

  scoped_refptr<Extension> extension(LoadAndExpectWarnings(
      "host_permissions_key_invalid_scheme.json", expected_warnings));
}

// Tests that listing a permissions as optional when that permission cannot be
// optional produces a warning and doesn't add the permission.
TEST_F(PermissionsParserTest, UnsupportedOptionalPermissionWarning) {
  scoped_refptr<Extension> extension(LoadAndExpectWarning(
      "unsupported_optional_api_permission.json",
      ErrorUtils::FormatErrorMessage(
          manifest_errors::kPermissionCannotBeOptional, "debugger")));

  // Check that the debugger was not included in the optional permissions as it
  // is not allowed to be optional.
  std::set<std::string> optional_api_names =
      PermissionsParser::GetOptionalPermissions(extension.get())
          .GetAPIsAsStrings();
  EXPECT_THAT(optional_api_names, testing::UnorderedElementsAre("tabs"));
}

// Test that chrome://favicon is a supported permission in MV2, but not MV3.
TEST_F(PermissionsParserTest, ChromeFavicon) {
  auto get_manifest_data = [](int manifest_version, const char* permission) {
    constexpr char kManifestStub[] =
        R"({
             "name": "Test",
             "version": "0.1",
             "manifest_version": %d,
             "%s": ["%s"]
           })";

    const char* permissions_key = manifest_version > 2
                                      ? manifest_keys::kHostPermissions
                                      : manifest_keys::kPermissions;
    base::Value manifest_value = base::test::ParseJson(base::StringPrintf(
        kManifestStub, manifest_version, permissions_key, permission));
    EXPECT_TRUE(manifest_value.is_dict());
    return ManifestData(std::move(manifest_value).TakeDict(), permission);
  };

  static constexpr char kFaviconPattern[] = "chrome://favicon/*";
  // <all_urls> implicitly includes chrome://favicon, if it's supported.
  constexpr char kAllUrls[] = "<all_urls>";

  auto has_favicon_access = [](const Extension& extension) {
    const GURL favicon_url("chrome://favicon");
    return extension.permissions_data()->HasHostPermission(favicon_url);
  };

  auto has_install_warning = [](const Extension& extension) {
    const char* permissions_key = extension.manifest_version() > 2
                                      ? manifest_keys::kHostPermissions
                                      : manifest_keys::kPermissions;

    InstallWarning expected_warning(ErrorUtils::FormatErrorMessage(
        manifest_errors::kInvalidPermissionScheme, permissions_key,
        kFaviconPattern));
    return base::Contains(extension.install_warnings(), expected_warning);
  };

  {
    scoped_refptr<const Extension> extension =
        LoadAndExpectSuccess(get_manifest_data(2, kFaviconPattern));
    ASSERT_TRUE(extension);
    EXPECT_TRUE(has_favicon_access(*extension));
    EXPECT_FALSE(has_install_warning(*extension));
  }

  {
    scoped_refptr<const Extension> extension =
        LoadAndExpectSuccess(get_manifest_data(2, kAllUrls));
    ASSERT_TRUE(extension);
    EXPECT_TRUE(has_favicon_access(*extension));
    EXPECT_FALSE(has_install_warning(*extension));
  }
  {
    scoped_refptr<const Extension> extension =
        LoadAndExpectSuccess(get_manifest_data(3, kFaviconPattern));
    ASSERT_TRUE(extension);
    EXPECT_FALSE(has_favicon_access(*extension));
    // Since chrome://favicon is not a valid permission in MV3, we expect a
    // warning to be thrown.
    EXPECT_TRUE(has_install_warning(*extension));
  }
  {
    scoped_refptr<const Extension> extension =
        LoadAndExpectSuccess(get_manifest_data(3, kAllUrls));
    ASSERT_TRUE(extension);
    EXPECT_FALSE(has_favicon_access(*extension));
    // NOTE: We don't expect an install warning here, because the <all_urls>
    // permission is still supported. It just doesn't grant favicon access.
    EXPECT_FALSE(has_install_warning(*extension));
  }
}

TEST_F(PermissionsParserTest, InternalPermissionsAreNotAllowedInTheManifest) {
  static constexpr char kManifest[] =
      R"({
           "name": "My Extension",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["searchProvider"]
         })";
  scoped_refptr<const Extension> extension = LoadAndExpectWarning(
      ManifestData::FromJSON(kManifest),
      "Permission 'searchProvider' is unknown or URL pattern is malformed.");

  ASSERT_TRUE(extension);
  EXPECT_FALSE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kSearchProvider));
}

}  // namespace extensions
