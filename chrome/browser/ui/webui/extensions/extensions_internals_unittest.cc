// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_internals_source.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
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

// Test that basic fields (like extension id, guid, name, version, etc.) show up
// correctly in the JSON returned by WriteToString.
TEST_F(ExtensionsInternalsUnitTest, Basic) {
  InitializeEmptyExtensionService();
  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID("ddchlicdkolnonkihahngkmmmjnjlkkf")
          .SetVersion("1.2.3.4")
          .SetLocation(extensions::mojom::ManifestLocation::kExternalPref)
          .Build();
  service()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(source.WriteToString());
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";
  base::Value::Dict& extension_json = extensions_list->GetList()[0].GetDict();

  EXPECT_THAT(extension_json.FindString("id"),
              testing::Pointee(extension->id()));
  EXPECT_THAT(extension_json.FindString("name"),
              testing::Pointee(extension->name()));
  EXPECT_THAT(extension_json.FindString("version"),
              testing::Pointee(extension->VersionString()));
  EXPECT_THAT(extension_json.FindString("location"),
              testing::Pointee(std::string("EXTERNAL_PREF")));
  EXPECT_THAT(extension_json.FindString("guid"),
              testing::Pointee(extension->guid()));
}

// Test that active and optional permissions show up correctly in the JSON
// returned by WriteToString.
TEST_F(ExtensionsInternalsUnitTest, WriteToStringPermissions) {
  InitializeEmptyExtensionService();
  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  extensions::SimpleFeature::ScopedThreadUnsafeAllowlistForTest
      allow_automation("ddchlicdkolnonkihahngkmmmjnjlkkf");
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID("ddchlicdkolnonkihahngkmmmjnjlkkf")
          .AddAPIPermission("activeTab")
          .SetManifestKey("automation", true)
          .SetManifestKey("optional_permissions",
                          base::Value::List().Append("storage"))
          .AddHostPermission("https://example.com/*")
          .AddContentScript("not-real.js", {"https://chromium.org/foo"})
          .Build();

  service()->AddExtension(extension.get());
  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(source.WriteToString());
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";

  EXPECT_EQ(extensions_list->GetList().size(), 1U);

  base::Value* extension_1 = &extensions_list->GetList()[0];
  ASSERT_TRUE(extension_1->is_dict());
  base::Value::Dict* permissions =
      extension_1->GetDict().FindDict("permissions");
  ASSERT_TRUE(permissions);

  // Permissions section should always have four elements: active, optional,
  // tab-specific and withheld.
  EXPECT_EQ(permissions->size(), 4U);

  base::Value::Dict* active = permissions->FindDict("active");
  ASSERT_NE(active->FindList("api"), nullptr);
  EXPECT_EQ(active->FindList("api")->front().GetString(), "activeTab");
  ASSERT_NE(active->FindList("manifest"), nullptr);
  EXPECT_TRUE(active->FindList("manifest")->front().is_dict());
  EXPECT_TRUE(
      active->FindList("manifest")->front().GetDict().FindBool("automation"));
  ASSERT_NE(active->FindList("explicit_hosts"), nullptr);
  EXPECT_EQ(active->FindList("explicit_hosts")->front().GetString(),
            "https://example.com/*");
  ASSERT_NE(active->FindList("scriptable_hosts"), nullptr);
  EXPECT_EQ(active->FindList("scriptable_hosts")->front().GetString(),
            "https://chromium.org/foo");

  base::Value::Dict* optional = permissions->FindDict("optional");
  EXPECT_EQ(optional->FindList("api")->front().GetString(), "storage");
}

// Test that tab-specific permissions show up correctly in the JSON returned by
// WriteToString.
TEST_F(ExtensionsInternalsUnitTest, WriteToStringTabSpecificPermissions) {
  InitializeEmptyExtensionService();
  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .AddAPIPermission("activeTab")
          .Build();
  service()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(source.WriteToString());
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";
  base::Value::Dict* permissions =
      extensions_list->GetList()[0].GetDict().FindDict("permissions");

  // Check that initially there is no tab-scpecific data.
  EXPECT_EQ(permissions->FindDict("tab_specific")->size(), 0U);

  // Grant a tab specific permission to the extension.
  extensions::APIPermissionSet tab_api_permissions;
  tab_api_permissions.insert(extensions::mojom::APIPermissionID::kTab);
  extensions::URLPatternSet tab_hosts;
  tab_hosts.AddOrigin(extensions::UserScript::ValidUserScriptSchemes(),
                      GURL("https://google.com/*"));
  extensions::PermissionSet tab_permissions(
      std::move(tab_api_permissions), extensions::ManifestPermissionSet(),
      tab_hosts.Clone(), tab_hosts.Clone());
  extension->permissions_data()->UpdateTabSpecificPermissions(1,
                                                              tab_permissions);
  extensions_list = base::JSONReader::Read(source.WriteToString());
  EXPECT_TRUE(extensions_list->GetList()[0].is_dict());
  permissions = extensions_list->GetList()[0].GetDict().FindDict("permissions");

  // Check the tab specific data is present now.
  base::Value::Dict* tab_specific = permissions->FindDict("tab_specific");
  EXPECT_EQ(tab_specific->size(), 1U);
  EXPECT_EQ(tab_specific->FindDict("1")
                ->FindList("explicit_hosts")
                ->front()
                .GetString(),
            "https://google.com/*");
  EXPECT_EQ(tab_specific->FindDict("1")
                ->FindList("scriptable_hosts")
                ->front()
                .GetString(),
            "https://google.com/*");
  EXPECT_EQ(tab_specific->FindDict("1")->FindList("api")->front().GetString(),
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
          .AddHostPermission("https://example.com/*")
          .Build();
  service()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(source.WriteToString());
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";
  base::Value::Dict* permissions =
      extensions_list->GetList()[0].GetDict().FindDict("permissions");

  // Check the host is initially in active hosts and there are no withheld
  // entries.
  EXPECT_EQ(permissions->FindDict("active")
                ->FindList("explicit_hosts")
                ->front()
                .GetString(),
            "https://example.com/*");
  EXPECT_EQ(permissions->FindDict("withheld")->FindList("api")->size(), 0U);
  EXPECT_EQ(permissions->FindDict("withheld")->FindList("manifest")->size(),
            0U);
  EXPECT_EQ(
      permissions->FindDict("withheld")->FindList("explicit_hosts")->size(),
      0U);
  EXPECT_EQ(
      permissions->FindDict("withheld")->FindList("scriptable_hosts")->size(),
      0U);

  // Change an active host to be withheld.
  extensions::ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);
  extensions_list = base::JSONReader::Read(source.WriteToString());
  permissions = extensions_list->GetList()[0].GetDict().FindDict("permissions");

  // Check the host that was active is now withheld.
  EXPECT_EQ(permissions->FindDict("active")->FindList("explicit_hosts")->size(),
            0U);
  EXPECT_EQ(permissions->FindDict("withheld")
                ->FindList("explicit_hosts")
                ->front()
                .GetString(),
            "https://example.com/*");
}
