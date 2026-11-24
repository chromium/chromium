// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/common/extensions/api/sidebar/sidebar_info.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

// Returns MV3 manifest JSON with `sidebar` as the "sidebar" key's value.
std::string GetManifestJson(const std::string& sidebar) {
  constexpr char kManifestStub[] =
      R"({
        "name": "Test",
        "version": "1.0",
        "manifest_version": 3,
        "sidebar": %s
      })";
  return base::StringPrintf(kManifestStub, sidebar.c_str());
}

}  // namespace

// Loads manifests with the `kApiSidebar` feature enabled.
class SidebarManifestTest : public ChromeManifestTest {
 protected:
  // ManifestTest:
  std::string GetTestExtensionID() const override { return kTestExtensionId; }

  // Returns manifest data with `sidebar` as the "sidebar" key's value.
  ManifestData GetManifestData(const std::string& sidebar) {
    return ManifestData::FromJSON(GetManifestJson(sidebar));
  }

 private:
  base::test::ScopedFeatureList feature_list_{extensions_features::kApiSidebar};
};

// Tests that `SidebarInfo` exposes a valid "sidebar.default_path" without
// install warnings.
TEST_F(SidebarManifestTest, DefaultPath) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      GetManifestData(R"({"default_path": "panel.html"})"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_TRUE(SidebarInfo::HasSidebar(*extension));
  EXPECT_EQ(extension->GetResourceURL("panel.html"),
            SidebarInfo::GetDefaultUrl(*extension));
}

// Tests that an empty "sidebar" key loads without warnings, since
// "default_path" is optional.
TEST_F(SidebarManifestTest, EmptyDictionary) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(GetManifestData("{}"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_TRUE(SidebarInfo::HasSidebar(*extension));
  EXPECT_TRUE(SidebarInfo::GetDefaultUrl(*extension).is_empty());
}

// Tests that an extension without a "sidebar" key loads without sidebar info or
// install warnings.
TEST_F(SidebarManifestTest, NoSidebarKey) {
  constexpr char kManifest[] = R"({
    "name": "Test",
    "version": "1.0",
    "manifest_version": 3
  })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->install_warnings().empty());
  EXPECT_FALSE(SidebarInfo::HasSidebar(*extension));
  EXPECT_TRUE(SidebarInfo::GetDefaultUrl(*extension).is_empty());
}

// Tests that invalid "sidebar.default_path" values fail the load.
TEST_F(SidebarManifestTest, InvalidDefaultPaths) {
  // Reject empty, root, query-only, directory, encoded backslash, other-origin,
  // external, and same-origin blob or filesystem URLs.
  const std::string kInvalidPaths[] = {
      "",
      "/",
      "?",
      "dir/",
      "dir%5Cpanel.html",
      "//other-host/panel.html",
      "https://example.com",
      base::StringPrintf("blob:chrome-extension://%s/panel.html",
                         kTestExtensionId),
      base::StringPrintf(
          "filesystem:chrome-extension://%s/temporary/panel.html",
          kTestExtensionId),
  };

  for (const std::string& path : kInvalidPaths) {
    SCOPED_TRACE(path);
    LoadAndExpectError(GetManifestData(base::StringPrintf(
                           R"({"default_path": "%s"})", path.c_str())),
                       manifest_errors::kSidebarManifestDefaultPathInvalid);
  }
}

// Tests that "sidebar.default_path" values that are not plain relative paths,
// but resolve inside the extension, load and store the resolved URL.
TEST_F(SidebarManifestTest, SameOriginDefaultPathsResolveInsideExtension) {
  const std::string kSameOriginPaths[] = {
      "../panel.html",
      "..\\\\panel.html",
      "dir\\\\..\\\\panel.html",
      base::StringPrintf("//%s/panel.html", kTestExtensionId),
      base::StringPrintf("chrome-extension://%s/panel.html", kTestExtensionId),
  };
  const GURL expected_url(
      base::StringPrintf("chrome-extension://%s/panel.html", kTestExtensionId));

  for (const std::string& path : kSameOriginPaths) {
    SCOPED_TRACE(path);
    scoped_refptr<Extension> extension = LoadAndExpectSuccess(GetManifestData(
        base::StringPrintf(R"({"default_path": "%s"})", path.c_str())));
    ASSERT_TRUE(extension);
    EXPECT_EQ(expected_url, SidebarInfo::GetDefaultUrl(*extension));
  }
}

// Tests that a "sidebar.default_path" that is not a string fails the load.
TEST_F(SidebarManifestTest, DefaultPathWrongType) {
  LoadAndExpectError(GetManifestData(R"({"default_path": {}})"),
                     "Error at key 'sidebar.default_path'. Type is invalid. "
                     "Expected string, found dictionary.");
}

// Tests that a "sidebar" key that is not a dictionary fails the load.
TEST_F(SidebarManifestTest, SidebarWrongType) {
  LoadAndExpectError(GetManifestData(R"("")"),
                     "Error at key 'sidebar'. Type is invalid. Expected "
                     "dictionary, found string.");
}

// Loads manifests with the `kApiSidebar` feature on or off.
class SidebarManifestFeatureTest : public base::test::WithFeatureOverride,
                                   public ChromeManifestTest {
 public:
  SidebarManifestFeatureTest()
      : WithFeatureOverride(extensions_features::kApiSidebar) {}
};

// Tests that `kApiSidebar` gates parsing of the "sidebar" key.
// - With the feature on, an invalid "default_path" fails the load.
// - With the feature off, Chrome warns and loads the extension without sidebar
//   info.
TEST_P(SidebarManifestFeatureTest, FeatureGating) {
  ManifestData manifest = ManifestData::FromJSON(
      GetManifestJson(R"({"default_path": "https://example.com"})"));
  if (IsParamFeatureEnabled()) {
    LoadAndExpectError(manifest,
                       manifest_errors::kSidebarManifestDefaultPathInvalid);
    return;
  }

  scoped_refptr<Extension> extension = LoadAndExpectWarning(
      manifest,
      "'sidebar' requires the 'ApiSidebar' feature flag to be enabled.");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(SidebarInfo::HasSidebar(*extension));
  EXPECT_TRUE(SidebarInfo::GetDefaultUrl(*extension).is_empty());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SidebarManifestFeatureTest);

}  // namespace extensions
