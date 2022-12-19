// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests make sure SettingsOverridePermission values are set correctly.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "components/version_info/version_info.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_message_test_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/settings_override_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

class SettingsOverridePermissionTest : public ChromeManifestTest {
 protected:
  SettingsOverridePermissionTest()
#if BUILDFLAG(IS_MAC)
      : scoped_channel_(version_info::Channel::UNKNOWN)
#endif
  {
  }

  enum Flags {
    kHomepage = 1,
    kStartupPages = 1 << 1,
    kSearchProvider = 1 << 2,
  };

  scoped_refptr<Extension> GetPermissionSet(uint32_t flags) {
    base::Value::Dict ext_manifest;
    ext_manifest.Set(manifest_keys::kName, "test");
    ext_manifest.Set(manifest_keys::kVersion, "0.1");
    ext_manifest.Set(manifest_keys::kManifestVersion, 2);

    base::Value::Dict settings_override;
    if (flags & kHomepage)
      settings_override.Set("homepage", "http://www.google.com/home");
    if (flags & kStartupPages) {
      base::Value::List startup_pages;
      startup_pages.Append("http://startup.com/startup.html");
      settings_override.Set("startup_pages", std::move(startup_pages));
    }
    if (flags & kSearchProvider) {
      base::Value::Dict search_provider;
      search_provider.Set("search_url", "http://google.com/search.html");
      search_provider.Set("name", "test");
      search_provider.Set("keyword", "lock");
      search_provider.Set("encoding", "UTF-8");
      search_provider.Set("is_default", true);
      search_provider.Set("favicon_url", "http://wikipedia.org/wiki/Favicon");
      settings_override.Set("search_provider", std::move(search_provider));
    }
    ext_manifest.Set(manifest_keys::kSettingsOverride,
                     std::move(settings_override));

    ManifestData manifest(std::move(ext_manifest));
    return LoadAndExpectSuccess(manifest);
  }

#if BUILDFLAG(IS_MAC)
  // On Mac, this API is limited to trunk.
  extensions::ScopedCurrentChannel scoped_channel_;
#endif  // BUILDFLAG(IS_MAC)
};

TEST_F(SettingsOverridePermissionTest, HomePage) {
  scoped_refptr<Extension> extension(GetPermissionSet(kHomepage));
  const PermissionSet& permission_set =
      extension->permissions_data()->active_permissions();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(permission_set.HasAPIPermission(APIPermissionID::kHomepage));
  VerifyOnePermissionMessage(extension->permissions_data(),
                             "Change your home page to: google.com");
#else
  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kHomepage));
#endif

  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kStartupPages));
  EXPECT_FALSE(
      permission_set.HasAPIPermission(APIPermissionID::kSearchProvider));
}

TEST_F(SettingsOverridePermissionTest, StartupPages) {
  scoped_refptr<Extension> extension(GetPermissionSet(kStartupPages));
  const PermissionSet& permission_set =
      extension->permissions_data()->active_permissions();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(permission_set.HasAPIPermission(APIPermissionID::kStartupPages));
  VerifyOnePermissionMessage(extension->permissions_data(),
                             "Change your start page to: startup.com");
#else
  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kStartupPages));
#endif

  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kHomepage));
  EXPECT_FALSE(
      permission_set.HasAPIPermission(APIPermissionID::kSearchProvider));
}

TEST_F(SettingsOverridePermissionTest, SearchSettings) {
  scoped_refptr<Extension> extension(GetPermissionSet(kSearchProvider));
  const PermissionSet& permission_set =
      extension->permissions_data()->active_permissions();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(
      permission_set.HasAPIPermission(APIPermissionID::kSearchProvider));
  VerifyOnePermissionMessage(extension->permissions_data(),
                             "Change your search settings to: google.com");
#else
  EXPECT_FALSE(
      permission_set.HasAPIPermission(APIPermissionID::kSearchProvider));
#endif

  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kHomepage));
  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kStartupPages));
}

TEST_F(SettingsOverridePermissionTest, All) {
  scoped_refptr<Extension> extension(GetPermissionSet(
      kSearchProvider | kStartupPages | kHomepage));
  const PermissionSet& permission_set =
      extension->permissions_data()->active_permissions();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(permission_set.HasAPIPermission(APIPermissionID::kHomepage));
  EXPECT_TRUE(permission_set.HasAPIPermission(APIPermissionID::kStartupPages));
  EXPECT_TRUE(
      permission_set.HasAPIPermission(APIPermissionID::kSearchProvider));
#else
  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kHomepage));
  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kStartupPages));
  EXPECT_FALSE(
      permission_set.HasAPIPermission(APIPermissionID::kSearchProvider));
#endif
}

TEST_F(SettingsOverridePermissionTest, Some) {
  scoped_refptr<Extension> extension(GetPermissionSet(
      kSearchProvider | kHomepage));
  const PermissionSet& permission_set =
      extension->permissions_data()->active_permissions();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(permission_set.HasAPIPermission(APIPermissionID::kHomepage));
  EXPECT_TRUE(
      permission_set.HasAPIPermission(APIPermissionID::kSearchProvider));
#else
  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kHomepage));
  EXPECT_FALSE(
      permission_set.HasAPIPermission(APIPermissionID::kSearchProvider));
#endif

  EXPECT_FALSE(permission_set.HasAPIPermission(APIPermissionID::kStartupPages));
}

}  // namespace

}  // namespace extensions
