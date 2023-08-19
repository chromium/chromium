// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/sync_helper.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/app_display_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

class ExtensionSyncTypeTest : public testing::Test {
 protected:
  enum SyncTestExtensionType {
    EXTENSION,
    APP,
    USER_SCRIPT,
    THEME
  };

  static scoped_refptr<Extension> MakeSyncTestExtension(
      SyncTestExtensionType type,
      const GURL& update_url,
      const GURL& launch_url,
      mojom::ManifestLocation location,
      const base::FilePath& extension_path,
      int creation_flags) {
    auto source = base::Value::Dict()
                      .Set(keys::kName, "PossiblySyncableExtension")
                      .Set(keys::kVersion, "0.0.0.0")
                      .Set(keys::kManifestVersion, 2);
    if (type == APP && launch_url.is_empty())
      source.Set(keys::kApp, "true");
    if (type == THEME)
      source.Set(keys::kTheme, base::Value::Dict());
    if (!update_url.is_empty()) {
      source.Set(keys::kUpdateURL, update_url.spec());
    }
    if (!launch_url.is_empty()) {
      source.SetByDottedPath(keys::kLaunchWebURL, launch_url.spec());
    }
    if (type != THEME)
      source.Set(keys::kConvertedFromUserScript, type == USER_SCRIPT);

    std::string error;
    scoped_refptr<Extension> extension = Extension::Create(
        extension_path, location, source, creation_flags, &error);
    EXPECT_TRUE(extension.get());
    EXPECT_TRUE(error.empty());
    return extension;
  }

  static const char kValidUpdateUrl1[];
  static const char kValidUpdateUrl2[];
};

const char ExtensionSyncTypeTest::kValidUpdateUrl1[] =
    "http://clients2.google.com/service/update2/crx";
const char ExtensionSyncTypeTest::kValidUpdateUrl2[] =
    "https://clients2.google.com/service/update2/crx";

TEST_F(ExtensionSyncTypeTest, NormalExtensionNoUpdateUrl) {
  scoped_refptr<Extension> extension(MakeSyncTestExtension(
      EXTENSION, GURL(), GURL(), mojom::ManifestLocation::kInternal,
      base::FilePath(), Extension::NO_FLAGS));
  EXPECT_TRUE(extension->is_extension());
  EXPECT_TRUE(sync_helper::IsSyncable(extension.get()));
}

TEST_F(ExtensionSyncTypeTest, UserScriptValidUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(USER_SCRIPT, GURL(kValidUpdateUrl1), GURL(),
                            mojom::ManifestLocation::kInternal,
                            base::FilePath(), Extension::NO_FLAGS));
  EXPECT_TRUE(extension->is_extension());
  EXPECT_TRUE(sync_helper::IsSyncable(extension.get()));
}

TEST_F(ExtensionSyncTypeTest, UserScriptNoUpdateUrl) {
  scoped_refptr<Extension> extension(MakeSyncTestExtension(
      USER_SCRIPT, GURL(), GURL(), mojom::ManifestLocation::kInternal,
      base::FilePath(), Extension::NO_FLAGS));
  EXPECT_TRUE(extension->is_extension());
  EXPECT_FALSE(sync_helper::IsSyncable(extension.get()));
}

TEST_F(ExtensionSyncTypeTest, ThemeNoUpdateUrl) {
  scoped_refptr<Extension> extension(MakeSyncTestExtension(
      THEME, GURL(), GURL(), mojom::ManifestLocation::kInternal,
      base::FilePath(), Extension::NO_FLAGS));
  EXPECT_TRUE(extension->is_theme());
  EXPECT_TRUE(sync_helper::IsSyncable(extension.get()));
}

TEST_F(ExtensionSyncTypeTest, AppWithLaunchUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL("http://www.google.com"),
                            mojom::ManifestLocation::kInternal,
                            base::FilePath(), Extension::NO_FLAGS));
  EXPECT_TRUE(extension->is_app());
  EXPECT_TRUE(sync_helper::IsSyncable(extension.get()));
}

TEST_F(ExtensionSyncTypeTest, ExtensionExternal) {
  scoped_refptr<Extension> extension(MakeSyncTestExtension(
      EXTENSION, GURL(), GURL(), mojom::ManifestLocation::kExternalPref,
      base::FilePath(), Extension::NO_FLAGS));
  EXPECT_TRUE(extension->is_extension());
  EXPECT_FALSE(sync_helper::IsSyncable(extension.get()));
}

TEST_F(ExtensionSyncTypeTest, UserScriptThirdPartyUpdateUrl) {
  scoped_refptr<Extension> extension(MakeSyncTestExtension(
      USER_SCRIPT, GURL("http://third-party.update_url.com"), GURL(),
      mojom::ManifestLocation::kInternal, base::FilePath(),
      Extension::NO_FLAGS));
  EXPECT_TRUE(extension->is_extension());
  EXPECT_FALSE(sync_helper::IsSyncable(extension.get()));
}

TEST_F(ExtensionSyncTypeTest, OnlyDisplayAppsInLauncher) {
  scoped_refptr<Extension> extension(MakeSyncTestExtension(
      EXTENSION, GURL(), GURL(), mojom::ManifestLocation::kInternal,
      base::FilePath(), Extension::NO_FLAGS));

  EXPECT_FALSE(AppDisplayInfo::ShouldDisplayInAppLauncher(*extension));
  EXPECT_FALSE(AppDisplayInfo::ShouldDisplayInNewTabPage(*extension));

  scoped_refptr<Extension> app(
      MakeSyncTestExtension(APP, GURL(), GURL("http://www.google.com"),
                            mojom::ManifestLocation::kInternal,
                            base::FilePath(), Extension::NO_FLAGS));
  EXPECT_TRUE(AppDisplayInfo::ShouldDisplayInAppLauncher(*app));
  EXPECT_TRUE(AppDisplayInfo::ShouldDisplayInNewTabPage(*app));
}

TEST_F(ExtensionSyncTypeTest, DisplayInXManifestProperties) {
  auto manifest = base::Value::Dict()
                      .Set(keys::kName, "TestComponentApp")
                      .Set(keys::kVersion, "0.0.0.0");
  manifest.SetByDottedPath(keys::kPlatformAppBackgroundPage, std::string());

  // Default to true.
  std::string error;
  scoped_refptr<Extension> app =
      Extension::Create(base::FilePath(), mojom::ManifestLocation::kComponent,
                        manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_TRUE(AppDisplayInfo::ShouldDisplayInAppLauncher(*app));
  EXPECT_TRUE(AppDisplayInfo::ShouldDisplayInNewTabPage(*app));

  // Value display_in_NTP defaults to display_in_launcher.
  manifest.Set(keys::kDisplayInLauncher, false);
  app = Extension::Create(base::FilePath(), mojom::ManifestLocation::kComponent,
                          manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_FALSE(AppDisplayInfo::ShouldDisplayInAppLauncher(*app));
  EXPECT_FALSE(AppDisplayInfo::ShouldDisplayInNewTabPage(*app));

  // Value display_in_NTP = true overriding display_in_launcher = false.
  manifest.Set(keys::kDisplayInNewTabPage, true);
  app = Extension::Create(base::FilePath(), mojom::ManifestLocation::kComponent,
                          manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_FALSE(AppDisplayInfo::ShouldDisplayInAppLauncher(*app));
  EXPECT_TRUE(AppDisplayInfo::ShouldDisplayInNewTabPage(*app));

  // Value display_in_NTP = false only, overrides default = true.
  manifest.Remove(keys::kDisplayInLauncher);
  manifest.Set(keys::kDisplayInNewTabPage, false);
  app = Extension::Create(base::FilePath(), mojom::ManifestLocation::kComponent,
                          manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_TRUE(AppDisplayInfo::ShouldDisplayInAppLauncher(*app));
  EXPECT_FALSE(AppDisplayInfo::ShouldDisplayInNewTabPage(*app));

  // Error checking.
  manifest.Set(keys::kDisplayInNewTabPage, "invalid");
  app = Extension::Create(base::FilePath(), mojom::ManifestLocation::kComponent,
                          manifest, 0, &error);
  EXPECT_EQ(error, base::UTF16ToUTF8(errors::kInvalidDisplayInNewTabPage));
}

TEST_F(ExtensionSyncTypeTest, OnlySyncInternal) {
  scoped_refptr<Extension> extension_internal(MakeSyncTestExtension(
      EXTENSION, GURL(), GURL(), mojom::ManifestLocation::kInternal,
      base::FilePath(), Extension::NO_FLAGS));
  EXPECT_TRUE(sync_helper::IsSyncable(extension_internal.get()));

  scoped_refptr<Extension> extension_noninternal(MakeSyncTestExtension(
      EXTENSION, GURL(), GURL(), mojom::ManifestLocation::kComponent,
      base::FilePath(), Extension::NO_FLAGS));
  EXPECT_FALSE(sync_helper::IsSyncable(extension_noninternal.get()));
}

TEST_F(ExtensionSyncTypeTest, DontSyncDefault) {
  scoped_refptr<Extension> extension_default(MakeSyncTestExtension(
      EXTENSION, GURL(), GURL(), mojom::ManifestLocation::kInternal,
      base::FilePath(), Extension::WAS_INSTALLED_BY_DEFAULT));
  EXPECT_FALSE(sync_helper::IsSyncable(extension_default.get()));
}

}  // namespace extensions
