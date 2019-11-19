// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_internals_source.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/channel.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ExtensionsInternalsUnitTest = extensions::ExtensionServiceTestBase;

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(
      profile, extensions::ExtensionPrefs::Get(profile));
}

}  // namespace

// Test that active and optional permissions show up correctly in the JSON
// returned by WriteToString.
TEST_F(ExtensionsInternalsUnitTest, WriteToStringPermissions) {
  // The automation manifest entry is restricted to the dev channel, so we do
  // this so the test is fine on stable/beta.
  extensions::ScopedCurrentChannel current_channel(version_info::Channel::DEV);

  InitializeEmptyExtensionService();
  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .AddPermission("activeTab")
          .SetManifestKey("automation", true)
          .SetManifestKey("optional_permissions",
                          extensions::ListBuilder().Append("storage").Build())
          .AddPermission("https://example.com/*")
          .AddContentScript("not-real.js", {"https://chromium.org/foo"})
          .Build();
  service()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(source.WriteToString());
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";

  EXPECT_EQ(extensions_list->GetList().size(), 1U);

  base::Value* extension_1 = &extensions_list->GetList()[0];
  ASSERT_TRUE(extension_1->is_dict());
  base::Value* permissions = extension_1->FindDictKey("permissions");
  ASSERT_TRUE(permissions);

  // Permissions section should always have four elements: active, optional,
  // tab-specific and withheld.
  EXPECT_EQ(permissions->DictSize(), 4U);

  base::Value* active = permissions->FindDictKey("active");
  ASSERT_NE(active->FindListKey("api"), nullptr);
  EXPECT_EQ(active->FindListKey("api")->GetList()[0].GetString(), "activeTab");
  ASSERT_NE(active->FindListKey("manifest"), nullptr);
  EXPECT_TRUE(
      active->FindListKey("manifest")->GetList()[0].FindBoolKey("automation"));
  ASSERT_NE(active->FindListKey("explicit_hosts"), nullptr);
  EXPECT_EQ(active->FindListKey("explicit_hosts")->GetList()[0].GetString(),
            "https://example.com/*");
  ASSERT_NE(active->FindListKey("scriptable_hosts"), nullptr);
  EXPECT_EQ(active->FindListKey("scriptable_hosts")->GetList()[0].GetString(),
            "https://chromium.org/foo");

  base::Value* optional = permissions->FindDictKey("optional");
  EXPECT_EQ(optional->FindListKey("api")->GetList()[0].GetString(), "storage");
}

// Test that tab-specific permissions show up correctly in the JSON returned by
// WriteToString.
TEST_F(ExtensionsInternalsUnitTest, WriteToStringTabSpecificPermissions) {
  InitializeEmptyExtensionService();
  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test").AddPermission("activeTab").Build();
  service()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(source.WriteToString());
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";
  base::Value* permissions =
      extensions_list->GetList()[0].FindDictKey("permissions");

  // Check that initially there is no tab-scpecific data.
  EXPECT_EQ(permissions->FindDictKey("tab_specific")->DictSize(), 0U);

  // Grant a tab specific permission to the extension.
  extensions::APIPermissionSet tab_api_permissions;
  tab_api_permissions.insert(extensions::APIPermission::kTab);
  extensions::URLPatternSet tab_hosts;
  tab_hosts.AddOrigin(extensions::UserScript::ValidUserScriptSchemes(),
                      GURL("https://google.com/*"));
  extensions::PermissionSet tab_permissions(
      std::move(tab_api_permissions), extensions::ManifestPermissionSet(),
      tab_hosts.Clone(), tab_hosts.Clone());
  extension->permissions_data()->UpdateTabSpecificPermissions(1,
                                                              tab_permissions);
  extensions_list = base::JSONReader::Read(source.WriteToString());
  permissions = extensions_list->GetList()[0].FindDictKey("permissions");

  // Check the tab specific data is present now.
  base::Value* tab_specific = permissions->FindDictKey("tab_specific");
  EXPECT_TRUE(tab_specific->is_dict());
  EXPECT_EQ(tab_specific->DictSize(), 1U);
  EXPECT_EQ(tab_specific->FindDictKey("1")
                ->FindListKey("explicit_hosts")
                ->GetList()[0]
                .GetString(),
            "https://google.com/*");
  EXPECT_EQ(tab_specific->FindDictKey("1")
                ->FindListKey("scriptable_hosts")
                ->GetList()[0]
                .GetString(),
            "https://google.com/*");
  EXPECT_EQ(tab_specific->FindDictKey("1")
                ->FindListKey("api")
                ->GetList()[0]
                .GetString(),
            "tabs");
}

// Test that withheld permissions show up correctly in the JSON returned by
// WriteToString.
TEST_F(ExtensionsInternalsUnitTest, WriteToStringWithheldPermissions) {
  InitializeEmptyExtensionService();
  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .AddPermission("https://example.com/*")
          .Build();
  service()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(source.WriteToString());
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";
  base::Value* permissions =
      extensions_list->GetList()[0].FindDictKey("permissions");

  // Check the host is initially in active hosts and there are no withheld
  // entries.
  EXPECT_EQ(permissions->FindDictKey("active")
                ->FindListKey("explicit_hosts")
                ->GetList()[0]
                .GetString(),
            "https://example.com/*");
  EXPECT_EQ(permissions->FindDictKey("withheld")
                ->FindListKey("api")
                ->GetList()
                .size(),
            0U);
  EXPECT_EQ(permissions->FindDictKey("withheld")
                ->FindListKey("manifest")
                ->GetList()
                .size(),
            0U);
  EXPECT_EQ(permissions->FindDictKey("withheld")
                ->FindListKey("explicit_hosts")
                ->GetList()
                .size(),
            0U);
  EXPECT_EQ(permissions->FindDictKey("withheld")
                ->FindListKey("scriptable_hosts")
                ->GetList()
                .size(),
            0U);

  // Change an active host to be withheld.
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);
  extensions_list = base::JSONReader::Read(source.WriteToString());
  permissions = extensions_list->GetList()[0].FindDictKey("permissions");

  // Check the host that was active is now withheld.
  EXPECT_EQ(permissions->FindDictKey("active")
                ->FindListKey("explicit_hosts")
                ->GetList()
                .size(),
            0U);
  EXPECT_EQ(permissions->FindDictKey("withheld")
                ->FindListKey("explicit_hosts")
                ->GetList()[0]
                .GetString(),
            "https://example.com/*");
}
