// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  }

  // Helper function that replaces the Manifest held by |manifest| with a copy
  // with its |key| changed to |value|. If |value| is nullptr, then |key| will
  // instead be deleted.
  void MutateManifest(std::unique_ptr<Manifest>* manifest,
                      const std::string& key,
                      std::unique_ptr<base::Value> value) {
    auto manifest_value = manifest->get()->value()->CreateDeepCopy();
    if (value)
      manifest_value->Set(key, std::move(value));
    else
      manifest_value->Remove(key, nullptr);
    manifest->reset(
        new Manifest(Manifest::INTERNAL, std::move(manifest_value)));
  }

  // Helper function that replaces the manifest held by |manifest| with a copy
  // and uses the |for_login_screen| during creation to determine its type.
  void MutateManifestForLoginScreen(std::unique_ptr<Manifest>* manifest,
                                    bool for_login_screen) {
    auto manifest_value = manifest->get()->value()->CreateDeepCopy();
    if (for_login_screen) {
      *manifest = Manifest::CreateManifestForLoginScreen(
          Manifest::EXTERNAL_POLICY, std::move(manifest_value));
    } else {
      *manifest = std::make_unique<Manifest>(Manifest::INTERNAL,
                                             std::move(manifest_value));
    }
  }

  std::string default_value_;
};

// Verifies that extensions can access the correct keys.
TEST_F(ManifestUnitTest, Extension) {
  std::unique_ptr<base::DictionaryValue> manifest_value(
      new base::DictionaryValue());
  manifest_value->SetString(keys::kName, "extension");
  manifest_value->SetString(keys::kVersion, "1");
  manifest_value->SetInteger(keys::kManifestVersion, 2);
  manifest_value->SetString(keys::kBackgroundPage, "bg.html");
  manifest_value->SetString("unknown_key", "foo");

  std::unique_ptr<Manifest> manifest(
      new Manifest(Manifest::INTERNAL, std::move(manifest_value)));
  std::string error;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(manifest->ValidateManifest(&error, &warnings));
  EXPECT_TRUE(error.empty());
  ASSERT_EQ(1u, warnings.size());
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);

  // The known key 'background.page' should be accessible.
  std::string value;
  EXPECT_TRUE(manifest->GetString(keys::kBackgroundPage, &value));
  EXPECT_EQ("bg.html", value);

  // The unknown key 'unknown_key' should be accesible.
  value.clear();
  EXPECT_TRUE(manifest->GetString("unknown_key", &value));
  EXPECT_EQ("foo", value);

  // Test CreateDeepCopy and Equals.
  std::unique_ptr<Manifest> manifest2 = manifest->CreateDeepCopy();
  EXPECT_TRUE(manifest->Equals(manifest2.get()));
  EXPECT_TRUE(manifest2->Equals(manifest.get()));
  MutateManifest(&manifest, "foo", std::make_unique<base::Value>("blah"));
  EXPECT_FALSE(manifest->Equals(manifest2.get()));
}

// Verifies that key restriction based on type works.
TEST_F(ManifestUnitTest, ExtensionTypes) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString(keys::kName, "extension");
  value->SetString(keys::kVersion, "1");

  std::unique_ptr<Manifest> manifest(
      new Manifest(Manifest::INTERNAL, std::move(value)));
  std::string error;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(manifest->ValidateManifest(&error, &warnings));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(warnings.empty());

  // By default, the type is Extension.
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);

  // Login screen extension
  MutateManifestForLoginScreen(&manifest, true);
  AssertType(manifest.get(), Manifest::TYPE_LOGIN_SCREEN_EXTENSION);
  MutateManifestForLoginScreen(&manifest, false);

  // Theme.
  MutateManifest(&manifest, keys::kTheme,
                 std::make_unique<base::DictionaryValue>());
  AssertType(manifest.get(), Manifest::TYPE_THEME);
  MutateManifest(&manifest, keys::kTheme, nullptr);

  // Shared module.
  MutateManifest(&manifest, keys::kExport,
                 std::make_unique<base::DictionaryValue>());
  AssertType(manifest.get(), Manifest::TYPE_SHARED_MODULE);
  MutateManifest(&manifest, keys::kExport, nullptr);

  // Packaged app.
  MutateManifest(&manifest, keys::kApp,
                 std::make_unique<base::DictionaryValue>());
  AssertType(manifest.get(), Manifest::TYPE_LEGACY_PACKAGED_APP);

  // Packaged app for login screen remains a packaged app.
  MutateManifestForLoginScreen(&manifest, true);
  AssertType(manifest.get(), Manifest::TYPE_LEGACY_PACKAGED_APP);
  MutateManifestForLoginScreen(&manifest, false);

  // Platform app with event page.
  MutateManifest(&manifest, keys::kPlatformAppBackground,
                 std::make_unique<base::DictionaryValue>());
  AssertType(manifest.get(), Manifest::TYPE_PLATFORM_APP);
  MutateManifest(&manifest, keys::kPlatformAppBackground, nullptr);

  // Hosted app.
  MutateManifest(&manifest, keys::kWebURLs,
                 std::make_unique<base::ListValue>());
  AssertType(manifest.get(), Manifest::TYPE_HOSTED_APP);
  MutateManifest(&manifest, keys::kWebURLs, nullptr);
  MutateManifest(&manifest, keys::kLaunchWebURL,
                 std::make_unique<base::Value>("foo"));
  AssertType(manifest.get(), Manifest::TYPE_HOSTED_APP);
  MutateManifest(&manifest, keys::kLaunchWebURL, nullptr);
}

// Verifies that the getters filter restricted keys.
TEST_F(ManifestUnitTest, RestrictedKeys) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString(keys::kName, "extension");
  value->SetString(keys::kVersion, "1");

  std::unique_ptr<Manifest> manifest(
      new Manifest(Manifest::INTERNAL, std::move(value)));
  std::string error;
  std::vector<InstallWarning> warnings;
  EXPECT_TRUE(manifest->ValidateManifest(&error, &warnings));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(warnings.empty());

  // "Commands" requires manifest version 2.
  const base::Value* output = nullptr;
  MutateManifest(&manifest, keys::kCommands,
                 std::make_unique<base::DictionaryValue>());
  EXPECT_FALSE(manifest->HasKey(keys::kCommands));
  EXPECT_FALSE(manifest->Get(keys::kCommands, &output));

  MutateManifest(&manifest, keys::kManifestVersion,
                 std::make_unique<base::Value>(2));
  EXPECT_TRUE(manifest->HasKey(keys::kCommands));
  EXPECT_TRUE(manifest->Get(keys::kCommands, &output));

  MutateManifest(&manifest, keys::kPageAction,
                 std::make_unique<base::DictionaryValue>());
  AssertType(manifest.get(), Manifest::TYPE_EXTENSION);
  EXPECT_TRUE(manifest->HasKey(keys::kPageAction));
  EXPECT_TRUE(manifest->Get(keys::kPageAction, &output));

  // Platform apps cannot have a "page_action" key.
  MutateManifest(&manifest, keys::kPlatformAppBackground,
                 std::make_unique<base::DictionaryValue>());
  AssertType(manifest.get(), Manifest::TYPE_PLATFORM_APP);
  EXPECT_FALSE(manifest->HasKey(keys::kPageAction));
  EXPECT_FALSE(manifest->Get(keys::kPageAction, &output));
  MutateManifest(&manifest, keys::kPlatformAppBackground, nullptr);

  // Platform apps also can't have a "Commands" key.
  EXPECT_FALSE(manifest->HasKey(keys::kCommands));
  EXPECT_FALSE(manifest->Get(keys::kCommands, &output));
}

}  // namespace extensions
