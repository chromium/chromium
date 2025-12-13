// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/ui/webui/extensions/extensions_internals_source.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions/permissions_updater.h"
#include "extensions/browser/permissions/scripting_permissions_modifier.h"
#include "extensions/buildflags/buildflags.h"
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

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
  registrar()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(
      source.WriteToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  EXPECT_THAT(extension_json.FindString("registry_status"),
              testing::Pointee(std::string("ENABLED")));
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

  registrar()->AddExtension(extension.get());
  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(
      source.WriteToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  registrar()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(
      source.WriteToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  extensions_list = base::JSONReader::Read(
      source.WriteToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  registrar()->AddExtension(extension.get());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(
      source.WriteToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  extensions_list = base::JSONReader::Read(
      source.WriteToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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

// Test that extensions in different ExtensionSets in the extension registry are
// marked correctly as such.
TEST_F(ExtensionsInternalsUnitTest, RegistryExtensionStatus) {
  InitializeEmptyExtensionService();
  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  scoped_refptr<const extensions::Extension> enabled_extension =
      extensions::ExtensionBuilder("enabled").Build();
  registrar()->AddExtension(enabled_extension.get());

  scoped_refptr<const extensions::Extension> disabled_extension =
      extensions::ExtensionBuilder("disabled").Build();
  registrar()->AddExtension(disabled_extension.get());
  registrar()->DisableExtension(
      disabled_extension->id(),
      {extensions::disable_reason::DISABLE_USER_ACTION});

  scoped_refptr<const extensions::Extension> terminated_extension =
      extensions::ExtensionBuilder("terminated").Build();
  registrar()->AddExtension(terminated_extension.get());
  registrar()->TerminateExtension(terminated_extension->id());

  scoped_refptr<const extensions::Extension> blocklisted_extension =
      extensions::ExtensionBuilder("blocklisted").Build();
  registrar()->AddExtension(blocklisted_extension.get());
  registrar()->BlocklistExtensionForTest(blocklisted_extension->id());

  ExtensionsInternalsSource source(profile());
  auto extensions_list = base::JSONReader::Read(
      source.WriteToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";
  for (const auto& info : extensions_list->GetList()) {
    const base::Value::Dict& extension_json = info.GetDict();
    const std::string* registry_status =
        extension_json.FindString("registry_status");
    ASSERT_TRUE(registry_status);
    const std::string* extension_id = extension_json.FindString("id");
    ASSERT_TRUE(extension_id);
    if (*extension_id == enabled_extension->id()) {
      EXPECT_EQ("ENABLED", *registry_status);
    } else if (*extension_id == disabled_extension->id()) {
      EXPECT_EQ("DISABLED", *registry_status);
    } else if (*extension_id == terminated_extension->id()) {
      EXPECT_EQ("TERMINATED", *registry_status);
    } else if (*extension_id == blocklisted_extension->id()) {
      EXPECT_EQ("BLOCKLISTED", *registry_status);
    } else {
      ADD_FAILURE() << "Unexpected extension found in regsitry";
    }
  }

  // There's no easy way to put a single extension into the BLOCKED state, so
  // instead we just block them all to check that. We do have to unblocklist
  // the blocklisted extension first though, as that takes priority otherwise.
  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      blocklisted_extension->id(),
      extensions::BitMapBlocklistState::NOT_BLOCKLISTED,
      extensions::ExtensionPrefs::Get(profile()));
  registrar()->OnBlocklistStateRemoved(blocklisted_extension->id());
  registrar()->BlockAllExtensions();
  extensions_list = base::JSONReader::Read(
      source.WriteToString(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(extensions_list) << "Failed to parse extensions internals json.";
  for (const auto& info : extensions_list->GetList()) {
    EXPECT_EQ("BLOCKED", *info.GetDict().FindString("registry_status"));
  }
}
