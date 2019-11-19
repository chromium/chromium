// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "components/version_info/version_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

class ExtensionManifestBackgroundTest : public ChromeManifestTest {
};

// TODO(devlin): Can this file move to //extensions?

TEST_F(ExtensionManifestBackgroundTest, BackgroundPermission) {
  LoadAndExpectError("background_permission.json",
                     errors::kBackgroundPermissionNeeded);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundScripts) {
  std::string error;
  base::Value manifest = LoadManifest("background_scripts.json", &error);
  ASSERT_TRUE(manifest.is_dict());

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(ManifestData(&manifest, "")));
  ASSERT_TRUE(extension.get());
  const std::vector<std::string>& background_scripts =
      BackgroundInfo::GetBackgroundScripts(extension.get());
  ASSERT_EQ(2u, background_scripts.size());
  EXPECT_EQ("foo.js", background_scripts[0u]);
  EXPECT_EQ("bar/baz.js", background_scripts[1u]);

  EXPECT_TRUE(BackgroundInfo::HasBackgroundPage(extension.get()));
  EXPECT_EQ(
      std::string("/") + kGeneratedBackgroundPageFilename,
      BackgroundInfo::GetBackgroundURL(extension.get()).path());

  manifest.SetPath({"background", "page"}, base::Value("monkey.html"));
  LoadAndExpectError(ManifestData(&manifest, ""),
                     errors::kInvalidBackgroundCombination);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPage) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  ASSERT_TRUE(extension.get());
  EXPECT_EQ("/foo.html",
            BackgroundInfo::GetBackgroundURL(extension.get()).path());
  EXPECT_TRUE(BackgroundInfo::AllowJSAccess(extension.get()));
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundAllowNoJsAccess) {
  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("background_allow_no_js_access.json");
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(BackgroundInfo::AllowJSAccess(extension.get()));

  extension = LoadAndExpectSuccess("background_allow_no_js_access2.json");
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(BackgroundInfo::AllowJSAccess(extension.get()));
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPageWebRequest) {
  ScopedCurrentChannel current_channel(version_info::Channel::DEV);

  std::string error;
  base::Value manifest = LoadManifest("background_page.json", &error);
  ASSERT_FALSE(manifest.is_none());
  manifest.SetPath({"background", "persistent"}, base::Value(false));
  manifest.SetKey(keys::kManifestVersion, base::Value(2));
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(ManifestData(&manifest, "")));
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(BackgroundInfo::HasLazyBackgroundPage(extension.get()));

  base::Value permissions(base::Value::Type::LIST);
  permissions.Append(base::Value("webRequest"));
  manifest.SetKey(keys::kPermissions, std::move(permissions));
  LoadAndExpectError(ManifestData(&manifest, ""),
                     errors::kWebRequestConflictsWithLazyBackground);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPageTransientBackground) {
  ScopedCurrentChannel current_channel(version_info::Channel::DEV);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(ManifestData(base::test::ParseJson(R"(
          {
            "name": "test",
            "manifest_version": 2,
            "version": "1",
            "background": {
              "page": "foo.html"
            }
          })"),
                                        "")));
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));

  LoadAndExpectError(
      ManifestData(base::test::ParseJson(R"(
          {
            "name": "test",
            "manifest_version": 2,
            "version": "1",
            "permissions": [
              "transientBackground"
            ],
            "background": {
              "page": "foo.html"
            }
          })"),
                   ""),
      errors::kTransientBackgroundConflictsWithPersistentBackground);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPagePersistentPlatformApp) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("background_page_persistent_app.json");
  ASSERT_TRUE(extension->is_platform_app());
  ASSERT_TRUE(BackgroundInfo::HasBackgroundPage(extension.get()));
  EXPECT_FALSE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));

  std::string error;
  std::vector<InstallWarning> warnings;
  ManifestHandler::ValidateExtension(extension.get(), &error, &warnings);
  // Persistent background pages are not supported for packaged apps.
  // The persistent flag is ignored and a warining is printed.
  EXPECT_EQ(1U, warnings.size());
  EXPECT_EQ(errors::kInvalidBackgroundPersistentInPlatformApp,
            warnings[0].message);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPagePersistentInvalidKey) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("background_page_invalid_persistent_key_app.json");
  ASSERT_TRUE(extension->is_platform_app());
  ASSERT_TRUE(BackgroundInfo::HasBackgroundPage(extension.get()));
  EXPECT_FALSE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));

  std::string error;
  std::vector<InstallWarning> warnings;
  ManifestHandler::ValidateExtension(extension.get(), &error, &warnings);
  // The key 'background.persistent' is not supported for packaged apps.
  EXPECT_EQ(1U, warnings.size());
  EXPECT_EQ(errors::kBackgroundPersistentInvalidForPlatformApps,
            warnings[0].message);
}

// Tests channel restriction on "background.service_worker" key.
TEST_F(ExtensionManifestBackgroundTest, ServiceWorkerBasedBackgroundKey) {
  // TODO(lazyboy): Add exhaustive tests here, e.g.
  //   - specifying a non-existent file.
  //   - specifying multiple files.
  //   - specifying invalid type (non-string) values.
  {
    ScopedCurrentChannel dev(version_info::Channel::DEV);
    scoped_refptr<Extension> extension =
        LoadAndExpectWarning("service_worker_based_background.json",
                             "'background.service_worker' requires canary "
                             "channel or newer, but this is the dev channel.");
  }
  {
    ScopedCurrentChannel canary(version_info::Channel::CANARY);
    scoped_refptr<Extension> extension =
        LoadAndExpectSuccess("service_worker_based_background.json");
  }
  {
    ScopedCurrentChannel trunk(version_info::Channel::UNKNOWN);
    scoped_refptr<Extension> extension =
        LoadAndExpectSuccess("service_worker_based_background.json");
  }
}

TEST_F(ExtensionManifestBackgroundTest, ManifestV3Restrictions) {
  auto get_expected_error = [](base::StringPiece key) {
    return ErrorUtils::FormatErrorMessage(
        errors::kBackgroundSpecificationInvalidForManifestV3, key);
  };

  {
    constexpr char kManifestBackgroundPage[] =
        R"({
             "name": "MV3 Test",
             "manifest_version": 3,
             "version": "0.1",
             "background": {
               "page": "background.html"
             }
           })";
    base::Value manifest_value = base::test::ParseJson(kManifestBackgroundPage);
    LoadAndExpectError(
        ManifestData(std::move(manifest_value), "background page"),
        get_expected_error(keys::kBackgroundPage));
  }
  {
    constexpr char kManifestBackgroundScripts[] =
        R"({
             "name": "MV3 Test",
             "manifest_version": 3,
             "version": "0.1",
             "background": {
               "scripts": ["background.js"]
             }
           })";
    base::Value manifest_value =
        base::test::ParseJson(kManifestBackgroundScripts);
    LoadAndExpectError(
        ManifestData(std::move(manifest_value), "background scripts"),
        get_expected_error(keys::kBackgroundScripts));
  }
  {
    constexpr char kManifestBackgroundPersistent[] =
        R"({
             "name": "MV3 Test",
             "manifest_version": 3,
             "version": "0.1",
             "background": {
               "service_worker": "worker.js",
               "persistent": true
             }
           })";
    base::Value manifest_value =
        base::test::ParseJson(kManifestBackgroundPersistent);
    LoadAndExpectError(
        ManifestData(std::move(manifest_value), "persistent background"),
        get_expected_error(keys::kBackgroundPersistent));
  }
  {
    // An extension with no background key present should still be allowed.
    constexpr char kManifestBackgroundPersistent[] =
        R"({
             "name": "MV3 Test",
             "manifest_version": 3,
             "version": "0.1"
           })";
    base::Value manifest_value =
        base::test::ParseJson(kManifestBackgroundPersistent);
    LoadAndExpectSuccess(
        ManifestData(std::move(manifest_value), "no background"));
  }
}

}  // namespace extensions
