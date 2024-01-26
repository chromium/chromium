// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/api/shared_module.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

// Not named "ManifestTest" because a test utility class has that name.
class ManifestUnitTest : public testing::Test {
 public:
  ManifestUnitTest() : default_value_("test") {}

 protected:
  void AssertType(Manifest* manifest, Manifest::Type type) {
    EXPECT_EQ(type, manifest->type());
    EXPECT_EQ(type == Manifest::TYPE_THEME, manifest->is_theme());
    EXPECT_EQ(type == Manifest::TYPE_PLATFORM_APP,
              manifest->is_platform_app());
    EXPECT_EQ(type == Manifest::TYPE_LEGACY_PACKAGED_APP,
              manifest->is_legacy_packaged_app());
    EXPECT_EQ(type == Manifest::TYPE_HOSTED_APP, manifest->is_hosted_app());
    EXPECT_EQ(type == Manifest::TYPE_SHARED_MODULE,
              manifest->is_shared_module());
    EXPECT_EQ(type == Manifest::TYPE_LOGIN_SCREEN_EXTENSION,
              manifest->is_login_screen_extension());
    EXPECT_EQ(type == Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION,
              manifest->is_chromeos_system_extension());
  }

  // Helper function that replaces the Manifest held by |manifest| with a copy
  // with its |key| changed to |value|.
  void MutateManifest(std::unique_ptr<Manifest>& manifest,
                      const std::string& key,
                      base::Value value) {
    base::Value::Dict manifest_value = manifest->value()->Clone();
    manifest_value.SetByDottedPath(key, std::move(value));
    ExtensionId extension_id = manifest->extension_id();
    manifest = std::make_unique<Manifest>(
        ManifestLocation::kInternal, std::move(manifest_value), extension_id);
  }

  // Helper function to delete the |key| from |manifest|.
  void DeleteManifestKey(std::unique_ptr<Manifest>& manifest,
                         const std::string& key) {
    base::Value::Dict manifest_value = manifest->value()->Clone();
    manifest_value.RemoveByDottedPath(key);
    ExtensionId extension_id = manifest->extension_id();
    manifest = std::make_unique<Manifest>(
        ManifestLocation::kInternal, std::move(manifest_value), extension_id);
  }

  // Helper function that replaces the manifest held by |manifest| with a copy
  // and uses the |for_login_screen| during creation to determine its type.
  void MutateManifestForLoginScreen(std::unique_ptr<Manifest>& manifest,
                                    bool for_login_screen) {
    auto manifest_value = manifest->value()->Clone();
    ExtensionId extension_id = manifest->extension_id();
    if (for_login_screen) {
      manifest = Manifest::CreateManifestForLoginScreen(
          ManifestLocation::kExternalPolicy, std::move(manifest_value),
          extension_id);
    } else {
      manifest = std::make_unique<Manifest>(
          ManifestLocation::kInternal, std::move(manifest_value), extension_id);
    }
  }

  std::string default_value_;
};

// Verifies that extensions can access the correct keys.
TEST_F(ManifestUnitTest, Extension) {
  auto manifest_value = base::Value::Dict()
                            .Set(keys::kName, "extension")
                            .Set(keys::kVersion, "1")
                            .Set(keys::kManifestVersion, 2)
                            .Set("unknown_key", "foo");
  manifest_value.SetByDottedPath(keys::kBackgroundPage, "bg.html");

  std::unique_ptr<Manifest> manifest(
      new Manifest(ManifestLocation::kInternal, std::move(manifest_value),
                   crx_file::id_util::GenerateId("extid")));
  std::vector<InstallWarning> warnings;
  manifest->ValidateManifest(&warnings);
  ASSERT_EQ(1u, warnings.size());
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);

  // The known key 'background.page' should be accessible.
  const std::string* background_page =
      manifest->FindStringPath(keys::kBackgroundPage);
  ASSERT_TRUE(background_page);
  EXPECT_EQ("bg.html", *background_page);

  // The unknown key 'unknown_key' should be accessible.
  const std::string* unknown_key_value =
      manifest->FindStringPath("unknown_key");
  ASSERT_TRUE(unknown_key_value);
  EXPECT_EQ("foo", *unknown_key_value);

  // Test EqualsForTesting.
  auto manifest2 = std::make_unique<Manifest>(
      ManifestLocation::kInternal, manifest->value()->Clone(),
      crx_file::id_util::GenerateId("extid"));
  EXPECT_TRUE(manifest->EqualsForTesting(*manifest2));
  EXPECT_TRUE(manifest2->EqualsForTesting(*manifest));
  MutateManifest(manifest, "foo", base::Value("blah"));
  EXPECT_FALSE(manifest->EqualsForTesting(*manifest2));
}

// Verifies that key restriction based on type works.
TEST_F(ManifestUnitTest, ExtensionTypes) {
  auto value = base::Value::Dict()
                   .Set(keys::kName, "extension")
                   .Set(keys::kVersion, "1");

  std::unique_ptr<Manifest> manifest(
      new Manifest(ManifestLocation::kInternal, std::move(value),
                   crx_file::id_util::GenerateId("extid")));
  std::vector<InstallWarning> warnings;
  manifest->ValidateManifest(&warnings);
  EXPECT_TRUE(warnings.empty());

  // By default, the type is Extension.
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);

  // Login screen extension
  MutateManifestForLoginScreen(manifest, true);
  AssertType(manifest.get(), Manifest::TYPE_LOGIN_SCREEN_EXTENSION);
  MutateManifestForLoginScreen(manifest, false);

  // Theme.
  MutateManifest(manifest, keys::kTheme, base::Value(base::Value::Dict()));
  AssertType(manifest.get(), Manifest::TYPE_THEME);
  DeleteManifestKey(manifest, keys::kTheme);

  // Shared module.
  MutateManifest(manifest, api::shared_module::ManifestKeys::kExport,
                 base::Value(base::Value::Dict()));
  AssertType(manifest.get(), Manifest::TYPE_SHARED_MODULE);
  DeleteManifestKey(manifest, api::shared_module::ManifestKeys::kExport);

  // Packaged app.
  MutateManifest(manifest, keys::kApp, base::Value(base::Value::Dict()));
  AssertType(manifest.get(), Manifest::TYPE_LEGACY_PACKAGED_APP);

  // Packaged app for login screen remains a packaged app.
  MutateManifestForLoginScreen(manifest, true);
  AssertType(manifest.get(), Manifest::TYPE_LEGACY_PACKAGED_APP);
  MutateManifestForLoginScreen(manifest, false);

  // Platform app with event page.
  MutateManifest(manifest, keys::kPlatformAppBackground,
                 base::Value(base::Value::Dict()));
  AssertType(manifest.get(), Manifest::TYPE_PLATFORM_APP);
  DeleteManifestKey(manifest, keys::kPlatformAppBackground);

  // Hosted app.
  MutateManifest(manifest, keys::kWebURLs, base::Value(base::Value::List()));
  AssertType(manifest.get(), Manifest::TYPE_HOSTED_APP);
  DeleteManifestKey(manifest, keys::kWebURLs);
  MutateManifest(manifest, keys::kLaunchWebURL, base::Value("foo"));
  AssertType(manifest.get(), Manifest::TYPE_HOSTED_APP);
  DeleteManifestKey(manifest, keys::kLaunchWebURL);
}

// Verifies that the getters filter restricted keys taking into account the
// manifest version.
TEST_F(ManifestUnitTest, RestrictedKeys_ManifestVersion) {
  base::Value::Dict value = base::Value::Dict()
                                .Set(keys::kName, "extension")
                                .Set(keys::kVersion, "1")
                                .Set(keys::kManifestVersion, 2);

  auto manifest =
      std::make_unique<Manifest>(ManifestLocation::kInternal, std::move(value),
                                 crx_file::id_util::GenerateId("extid"));
  std::vector<InstallWarning> warnings;
  manifest->ValidateManifest(&warnings);
  EXPECT_TRUE(warnings.empty());

  // "host_permissions" requires manifest version 3.
  MutateManifest(manifest, keys::kHostPermissions,
                 base::Value(base::Value::List()));
  EXPECT_FALSE(manifest->FindKey(keys::kHostPermissions));

  // Update the extension to be manifest_version: 3; the host_permissions
  // should then be available.
  MutateManifest(manifest, keys::kManifestVersion, base::Value(3));
  EXPECT_TRUE(manifest->FindKey(keys::kHostPermissions));
}

// Verifies that the getters filter restricted keys taking into account the
// item type.
TEST_F(ManifestUnitTest, RestrictedKeys_ItemType) {
  base::Value::Dict value = base::Value::Dict()
                                .Set(keys::kName, "item")
                                .Set(keys::kVersion, "1")
                                .Set(keys::kManifestVersion, 2)
                                .Set(keys::kPageAction, base::Value::Dict());

  auto manifest =
      std::make_unique<Manifest>(ManifestLocation::kInternal, std::move(value),
                                 crx_file::id_util::GenerateId("extid"));
  std::vector<InstallWarning> warnings;
  manifest->ValidateManifest(&warnings);
  EXPECT_TRUE(warnings.empty());
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);

  // Extensions can specify "page_action"...
  EXPECT_TRUE(manifest->FindKey(keys::kPageAction));

  MutateManifest(manifest, keys::kPlatformAppBackground,
                 base::Value(base::Value::Dict()));
  AssertType(manifest.get(), Manifest::TYPE_PLATFORM_APP);
  // ...But platform apps may not.
  EXPECT_FALSE(manifest->FindKey(keys::kPageAction));
}

}  // namespace extensions
