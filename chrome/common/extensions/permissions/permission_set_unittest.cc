// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_message_test_util.h"
#include "extensions/common/permissions/permission_message_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/socket_permission.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using extension_test_util::LoadManifest;
using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

size_t IndexOf(const PermissionMessages& warnings, const std::string& warning) {
  std::u16string warning16 = base::ASCIIToUTF16(warning);
  size_t i = 0;
  for (const PermissionMessage& msg : warnings) {
    if (msg.message() == warning16)
      return i;
    ++i;
  }

  return warnings.size();
}

PermissionIDSet MakePermissionIDSet(APIPermissionID id1, APIPermissionID id2) {
  PermissionIDSet set;
  set.insert(id1);
  set.insert(id2);
  return set;
}

PermissionIDSet MakePermissionIDSet(const APIPermissionSet& permissions) {
  PermissionIDSet set;
  for (const APIPermission* permission : permissions)
    set.insert(permission->id());
  return set;
}

std::string PermissionIDsToString(const PermissionIDSet& ids) {
  std::vector<std::string> strs;
  for (const PermissionID& id : ids)
    strs.push_back(base::NumberToString(static_cast<int>(id.id())));
  return base::StringPrintf("[ %s ]", base::JoinString(strs, ", ").c_str());
}

std::string CoalescedPermissionIDsToString(const PermissionMessages& msgs) {
  std::vector<std::string> strs;
  for (const PermissionMessage& msg : msgs)
    strs.push_back(PermissionIDsToString(msg.permissions()));
  return base::JoinString(strs, " ");
}

// Check that the given |permissions| produce a single warning message,
// identified by the set of |expected_ids|.
testing::AssertionResult PermissionSetProducesMessage(
    const PermissionSet& permissions,
    Manifest::Type extension_type,
    const PermissionIDSet& expected_ids) {
  const PermissionMessageProvider* provider = PermissionMessageProvider::Get();

  PermissionMessages msgs = provider->GetPermissionMessages(
      provider->GetAllPermissionIDs(permissions, extension_type));
  if (msgs.size() != 1) {
    return testing::AssertionFailure()
           << "Expected single permission message with IDs "
           << PermissionIDsToString(expected_ids) << " but got " << msgs.size()
           << " messages: " << CoalescedPermissionIDsToString(msgs);
  }
  if (!msgs.front().permissions().Equals(expected_ids)) {
    return testing::AssertionFailure()
           << "Expected permission IDs " << PermissionIDsToString(expected_ids)
           << " but got " << PermissionIDsToString(msgs.front().permissions());
  }

  return testing::AssertionSuccess();
}

}  // namespace

// Tests GetByID.
TEST(PermissionsTest, GetByID) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  APIPermissionSet apis = info->GetAllForTest();
  for (const auto* api : apis)
    EXPECT_EQ(api->id(), api->info()->id());
}

// Tests that GetByName works with normal permission names and aliases.
TEST(PermissionsTest, GetByName) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  EXPECT_EQ(APIPermissionID::kTab, info->GetByName("tabs")->id());
  EXPECT_EQ(APIPermissionID::kManagement, info->GetByName("management")->id());
  EXPECT_FALSE(info->GetByName("alsdkfjasldkfj"));
}

TEST(PermissionsTest, GetAll) {
  size_t count = 0;
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  APIPermissionSet apis = info->GetAllForTest();
  for (const auto* api : apis) {
    // Make sure only the valid permission IDs get returned.
    EXPECT_NE(APIPermissionID::kInvalid, api->id());
    EXPECT_NE(APIPermissionID::kUnknown, api->id());
    count++;
  }
  EXPECT_EQ(count, info->get_permission_count());
}

TEST(PermissionsTest, GetAllByName) {
  std::set<std::string> names;
  names.insert("background");
  names.insert("management");

  // This is an alias of kTab
  names.insert("windows");

  // This unknown name should get dropped.
  names.insert("sdlkfjasdlkfj");

  APIPermissionSet expected;
  expected.insert(APIPermissionID::kBackground);
  expected.insert(APIPermissionID::kManagement);
  expected.insert(APIPermissionID::kTab);

  EXPECT_EQ(expected,
            PermissionsInfo::GetInstance()->GetAllByNameForTest(names));
}

// Tests that the aliases are properly mapped.
TEST(PermissionsTest, Aliases) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  // tabs: tabs, windows
  std::string tabs_name = "tabs";
  EXPECT_EQ(tabs_name, info->GetByID(APIPermissionID::kTab)->name());
  EXPECT_EQ(APIPermissionID::kTab, info->GetByName("tabs")->id());
  EXPECT_EQ(APIPermissionID::kTab, info->GetByName("windows")->id());

  // unlimitedStorage: unlimitedStorage, unlimited_storage
  std::string storage_name = "unlimitedStorage";
  EXPECT_EQ(storage_name,
            info->GetByID(APIPermissionID::kUnlimitedStorage)->name());
  EXPECT_EQ(APIPermissionID::kUnlimitedStorage,
            info->GetByName("unlimitedStorage")->id());
  EXPECT_EQ(APIPermissionID::kUnlimitedStorage,
            info->GetByName("unlimited_storage")->id());
}

TEST(PermissionsTest, EffectiveHostPermissions) {
  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "empty.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_EQ(0u, extension->permissions_data()
                      ->GetEffectiveHostPermissions()
                      .patterns()
                      .size());
    EXPECT_FALSE(
        permissions.HasEffectiveAccessToURL(GURL("http://www.google.com")));
    EXPECT_FALSE(permissions.HasEffectiveAccessToAllHosts());
  }

  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "one_host.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("http://www.google.com")));
    EXPECT_FALSE(
        permissions.HasEffectiveAccessToURL(GURL("https://www.google.com")));
    EXPECT_FALSE(permissions.HasEffectiveAccessToAllHosts());
  }

  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "one_host_wildcard.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(permissions.HasEffectiveAccessToURL(GURL("http://google.com")));
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("http://foo.google.com")));
    EXPECT_FALSE(permissions.HasEffectiveAccessToAllHosts());
  }

  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "two_hosts.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("http://www.google.com")));
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("http://www.reddit.com")));
    EXPECT_FALSE(permissions.HasEffectiveAccessToAllHosts());
  }

  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "https_not_considered.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(permissions.HasEffectiveAccessToURL(GURL("http://google.com")));
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("https://google.com")));
    EXPECT_FALSE(permissions.HasEffectiveAccessToAllHosts());
  }

  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "two_content_scripts.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(permissions.HasEffectiveAccessToURL(GURL("http://google.com")));
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("http://www.reddit.com")));
    EXPECT_TRUE(permissions.HasEffectiveAccessToURL(
        GURL("http://news.ycombinator.com")));
    EXPECT_FALSE(permissions.HasEffectiveAccessToAllHosts());
  }

  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "all_hosts.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(permissions.HasEffectiveAccessToURL(GURL("http://test/")));
    EXPECT_FALSE(permissions.HasEffectiveAccessToURL(GURL("https://test/")));
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("http://www.google.com")));
    EXPECT_TRUE(permissions.HasEffectiveAccessToAllHosts());
  }

  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "all_hosts2.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(permissions.HasEffectiveAccessToURL(GURL("http://test/")));
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("http://www.google.com")));
    EXPECT_TRUE(permissions.HasEffectiveAccessToAllHosts());
  }

  {
    scoped_refptr<const Extension> extension =
        LoadManifest("effective_host_permissions", "all_hosts3.json");
    const PermissionSet& permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_FALSE(permissions.HasEffectiveAccessToURL(GURL("http://test/")));
    EXPECT_TRUE(permissions.HasEffectiveAccessToURL(GURL("https://test/")));
    EXPECT_TRUE(
        permissions.HasEffectiveAccessToURL(GURL("http://www.google.com")));
    EXPECT_TRUE(permissions.HasEffectiveAccessToAllHosts());
  }
}

TEST(PermissionsTest, ExplicitAccessToOrigin) {
  APIPermissionSet apis;
  ManifestPermissionSet manifest_permissions;
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;

  AddPattern(&explicit_hosts, "http://*.google.com/*");
  // The explicit host paths should get set to /*.
  AddPattern(&explicit_hosts, "http://www.example.com/a/particular/path/*");

  PermissionSet perm_set(std::move(apis), std::move(manifest_permissions),
                         std::move(explicit_hosts),
                         std::move(scriptable_hosts));
  ASSERT_TRUE(
      perm_set.HasExplicitAccessToOrigin(GURL("http://www.google.com/")));
  ASSERT_TRUE(
      perm_set.HasExplicitAccessToOrigin(GURL("http://test.google.com/")));
  ASSERT_TRUE(
      perm_set.HasExplicitAccessToOrigin(GURL("http://www.example.com")));
  ASSERT_TRUE(perm_set.HasEffectiveAccessToURL(GURL("http://www.example.com")));
  ASSERT_FALSE(
      perm_set.HasExplicitAccessToOrigin(GURL("http://test.example.com")));
}

TEST(PermissionsTest, CreateUnion) {
  ManifestPermissionSet manifest_permissions;
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  URLPatternSet effective_hosts;

  std::unique_ptr<const PermissionSet> set1;
  std::unique_ptr<const PermissionSet> set2;
  std::unique_ptr<const PermissionSet> union_set;

  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermissionID::kSocket);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }

  // Union with an empty set.
  apis1.insert(APIPermissionID::kTab);
  apis1.insert(APIPermissionID::kBackground);
  apis1.insert(permission->Clone());
  expected_apis.insert(APIPermissionID::kTab);
  expected_apis.insert(APIPermissionID::kBackground);
  expected_apis.insert(std::move(permission));

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.google.com/*");
  AddPattern(&effective_hosts, "http://*.google.com/*");

  set1 = std::make_unique<PermissionSet>(
      apis1.Clone(), manifest_permissions.Clone(), explicit_hosts1.Clone(),
      scriptable_hosts1.Clone());
  set2 = std::make_unique<PermissionSet>(
      apis2.Clone(), manifest_permissions.Clone(), explicit_hosts2.Clone(),
      scriptable_hosts2.Clone());
  union_set = PermissionSet::CreateUnion(*set1, *set2);
  EXPECT_TRUE(set1->Contains(*set2));
  EXPECT_TRUE(set1->Contains(*union_set));
  EXPECT_FALSE(set2->Contains(*set1));
  EXPECT_FALSE(set2->Contains(*union_set));
  EXPECT_TRUE(union_set->Contains(*set1));
  EXPECT_TRUE(union_set->Contains(*set2));

  EXPECT_EQ(expected_apis, union_set->apis());
  EXPECT_EQ(expected_explicit_hosts, union_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, union_set->scriptable_hosts());
  EXPECT_EQ(expected_explicit_hosts, union_set->effective_hosts());

  // Now use a real second set.
  apis2.insert(APIPermissionID::kTab);
  apis2.insert(APIPermissionID::kProxy);
  apis2.insert(APIPermissionID::kClipboardWrite);

  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-send-to::8899");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermissionID::kTab);
  expected_apis.insert(APIPermissionID::kProxy);
  expected_apis.insert(APIPermissionID::kClipboardWrite);

  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    list.Append("udp-send-to::8899");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  // Insert a new permission socket permisssion which will replace the old one.
  expected_apis.insert(std::move(permission));

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.example.com/*");
  AddPattern(&expected_scriptable_hosts, "http://*.google.com/*");

  effective_hosts =
      URLPatternSet::CreateUnion(explicit_hosts2, scriptable_hosts2);

  set2 = std::make_unique<PermissionSet>(
      apis2.Clone(), manifest_permissions.Clone(), explicit_hosts2.Clone(),
      scriptable_hosts2.Clone());
  union_set = PermissionSet::CreateUnion(*set1, *set2);

  EXPECT_FALSE(set1->Contains(*set2));
  EXPECT_FALSE(set1->Contains(*union_set));
  EXPECT_FALSE(set2->Contains(*set1));
  EXPECT_FALSE(set2->Contains(*union_set));
  EXPECT_TRUE(union_set->Contains(*set1));
  EXPECT_TRUE(union_set->Contains(*set2));

  EXPECT_TRUE(union_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, union_set->apis());
  EXPECT_EQ(expected_explicit_hosts, union_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, union_set->scriptable_hosts());
  EXPECT_EQ(effective_hosts, union_set->effective_hosts());
}

TEST(PermissionsTest, CreateIntersection) {
  ManifestPermissionSet manifest_permissions;
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  URLPatternSet effective_hosts;

  std::unique_ptr<const PermissionSet> set1;
  std::unique_ptr<const PermissionSet> set2;
  std::unique_ptr<const PermissionSet> new_set;

  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermissionID::kSocket);

  // Intersection with an empty set.
  apis1.insert(APIPermissionID::kTab);
  apis1.insert(APIPermissionID::kBackground);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  apis1.insert(std::move(permission));

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&scriptable_hosts1, "http://www.reddit.com/*");

  set1 = std::make_unique<PermissionSet>(
      apis1.Clone(), manifest_permissions.Clone(), explicit_hosts1.Clone(),
      scriptable_hosts1.Clone());
  set2 = std::make_unique<PermissionSet>(
      apis2.Clone(), manifest_permissions.Clone(), explicit_hosts2.Clone(),
      scriptable_hosts2.Clone());
  new_set = PermissionSet::CreateIntersection(*set1, *set2);
  EXPECT_TRUE(set1->Contains(*new_set));
  EXPECT_TRUE(set2->Contains(*new_set));
  EXPECT_TRUE(set1->Contains(*set2));
  EXPECT_FALSE(set2->Contains(*set1));
  EXPECT_FALSE(new_set->Contains(*set1));
  EXPECT_TRUE(new_set->Contains(*set2));

  EXPECT_TRUE(new_set->IsEmpty());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(expected_explicit_hosts, new_set->effective_hosts());

  // Now use a real second set.
  apis2.insert(APIPermissionID::kTab);
  apis2.insert(APIPermissionID::kProxy);
  apis2.insert(APIPermissionID::kClipboardWrite);
  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    list.Append("udp-send-to::8899");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermissionID::kTab);
  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  expected_apis.insert(std::move(permission));

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&explicit_hosts2, "http://*.google.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_explicit_hosts, "http://*.google.com/*");

  effective_hosts.ClearPatterns();
  AddPattern(&effective_hosts, "http://*.google.com/*");

  set2 = std::make_unique<PermissionSet>(
      apis2.Clone(), manifest_permissions.Clone(), explicit_hosts2.Clone(),
      scriptable_hosts2.Clone());
  new_set = PermissionSet::CreateIntersection(*set1, *set2);

  EXPECT_TRUE(set1->Contains(*new_set));
  EXPECT_TRUE(set2->Contains(*new_set));
  EXPECT_FALSE(set1->Contains(*set2));
  EXPECT_FALSE(set2->Contains(*set1));
  EXPECT_FALSE(new_set->Contains(*set1));
  EXPECT_FALSE(new_set->Contains(*set2));

  EXPECT_FALSE(new_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(effective_hosts, new_set->effective_hosts());
}

TEST(PermissionsTest, CreateDifference) {
  ManifestPermissionSet manifest_permissions;
  APIPermissionSet apis1;
  APIPermissionSet apis2;
  APIPermissionSet expected_apis;

  URLPatternSet explicit_hosts1;
  URLPatternSet explicit_hosts2;
  URLPatternSet expected_explicit_hosts;

  URLPatternSet scriptable_hosts1;
  URLPatternSet scriptable_hosts2;
  URLPatternSet expected_scriptable_hosts;

  URLPatternSet effective_hosts;

  std::unique_ptr<const PermissionSet> set1;
  std::unique_ptr<const PermissionSet> set2;
  std::unique_ptr<const PermissionSet> new_set;

  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(APIPermissionID::kSocket);

  // Difference with an empty set.
  apis1.insert(APIPermissionID::kTab);
  apis1.insert(APIPermissionID::kBackground);
  std::unique_ptr<APIPermission> permission =
      permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  apis1.insert(std::move(permission));

  AddPattern(&explicit_hosts1, "http://*.google.com/*");
  AddPattern(&scriptable_hosts1, "http://www.reddit.com/*");

  set1 = std::make_unique<PermissionSet>(
      apis1.Clone(), manifest_permissions.Clone(), explicit_hosts1.Clone(),
      scriptable_hosts1.Clone());
  set2 = std::make_unique<PermissionSet>(
      apis2.Clone(), manifest_permissions.Clone(), explicit_hosts2.Clone(),
      scriptable_hosts2.Clone());
  new_set = PermissionSet::CreateDifference(*set1, *set2);
  EXPECT_EQ(*set1, *new_set);

  // Now use a real second set.
  apis2.insert(APIPermissionID::kTab);
  apis2.insert(APIPermissionID::kProxy);
  apis2.insert(APIPermissionID::kClipboardWrite);
  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("tcp-connect:*.example.com:80");
    list.Append("udp-send-to::8899");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  apis2.insert(std::move(permission));

  expected_apis.insert(APIPermissionID::kBackground);
  permission = permission_info->CreateAPIPermission();
  {
    base::Value::List list;
    list.Append("udp-bind::8080");
    list.Append("udp-send-to::8888");
    base::Value value(std::move(list));
    ASSERT_TRUE(permission->FromValue(&value, nullptr, nullptr));
  }
  expected_apis.insert(std::move(permission));

  AddPattern(&explicit_hosts2, "http://*.example.com/*");
  AddPattern(&explicit_hosts2, "http://*.google.com/*");
  AddPattern(&scriptable_hosts2, "http://*.google.com/*");
  AddPattern(&expected_scriptable_hosts, "http://www.reddit.com/*");

  effective_hosts.ClearPatterns();
  AddPattern(&effective_hosts, "http://www.reddit.com/*");

  set2 = std::make_unique<PermissionSet>(
      apis2.Clone(), manifest_permissions.Clone(), explicit_hosts2.Clone(),
      scriptable_hosts2.Clone());
  new_set = PermissionSet::CreateDifference(*set1, *set2);

  EXPECT_TRUE(set1->Contains(*new_set));
  EXPECT_FALSE(set2->Contains(*new_set));

  EXPECT_FALSE(new_set->HasEffectiveAccessToAllHosts());
  EXPECT_EQ(expected_apis, new_set->apis());
  EXPECT_EQ(expected_explicit_hosts, new_set->explicit_hosts());
  EXPECT_EQ(expected_scriptable_hosts, new_set->scriptable_hosts());
  EXPECT_EQ(effective_hosts, new_set->effective_hosts());

  // |set3| = |set1| - |set2| --> |set3| intersect |set2| == empty_set
  set1 = PermissionSet::CreateIntersection(*new_set, *set2);
  EXPECT_TRUE(set1->IsEmpty());
}

TEST(PermissionsTest, IsPrivilegeIncrease) {
  const struct {
    const char* base_name;
    bool expect_increase;
  } kTests[] = {
      {"allhosts1", false},     // all -> all
      {"allhosts2", false},     // all -> one
      {"allhosts3", true},      // one -> all
      {"hosts1", false},        // http://a,http://b -> http://a,http://b
      {"hosts2", true},         // http://a,http://b -> https://a,http://*.b
      {"hosts3", false},        // http://a,http://b -> http://a
      {"hosts4", true},         // http://a -> http://a,http://b
      {"hosts5", false},        // http://a,b,c -> http://a,b,c + https://a,b,c
      {"hosts6", false},        // http://a.com -> http://a.com + http://a.co.uk
      {"permissions1", false},  // tabs -> tabs
      {"permissions2", true},   // tabs -> tabs,bookmarks
      {"permissions3", false},  // http://*/* -> http://*/*,tabs
      {"permissions5", true},   // bookmarks -> bookmarks,history
      {"equivalent_warnings", false},  // tabs --> tabs, webNavigation

      // The plugins manifest key is deprecated and doesn't correspond to any
      // permissions now.
      {"permissions4", true},  // plugin -> plugin,tabs
      {"plugin1", false},      // plugin -> plugin
      {"plugin2", false},      // plugin -> none
      {"plugin3", false},      // none -> plugin

      {"storage", false},           // none -> storage
      {"notifications", true},      // none -> notifications
      {"platformapp1", false},      // host permissions for platform apps
      {"platformapp2", true},       // API permissions for platform apps
      {"media_galleries1", true},   // all -> read|all
      {"media_galleries2", true},   // read|all -> read|delete|copyTo|all
      {"media_galleries3", true},   // all -> read|delete|all
      {"media_galleries4", false},  // read|all -> all
      {"media_galleries5", false},  // read|copyTo|delete|all -> read|all
      {"media_galleries6", false},  // read|all -> read|all
      {"media_galleries7", true},   // read|delete|all -> read|copyTo|delete|all
      {"sockets1", true},           // none -> tcp:*:*
      {"sockets2", false},          // tcp:*:* -> tcp:*:*
      {"sockets3", true},           // tcp:a.com:80 -> tcp:*:*
  };

  for (size_t i = 0; i < std::size(kTests); ++i) {
    scoped_refptr<Extension> old_extension(
        LoadManifest("allow_silent_upgrade",
                     std::string(kTests[i].base_name) + "_old.json"));
    scoped_refptr<Extension> new_extension(
        LoadManifest("allow_silent_upgrade",
                     std::string(kTests[i].base_name) + "_new.json"));

    EXPECT_TRUE(new_extension.get()) << kTests[i].base_name << "_new.json";
    if (!new_extension.get())
      continue;

    const PermissionSet& old_p =
        old_extension->permissions_data()->active_permissions();
    const PermissionSet& new_p =
        new_extension->permissions_data()->active_permissions();
    Manifest::Type extension_type = old_extension->GetType();

    bool increased = PermissionMessageProvider::Get()->IsPrivilegeIncrease(
        old_p, new_p, extension_type);
    EXPECT_EQ(kTests[i].expect_increase, increased) << kTests[i].base_name;
  }
}

// Tests that swapping out a permission for a less powerful one is not
// considered a privilege increase.
// Regression test for https://crbug.com/841938.
TEST(PermissionsTest,
     IsNotPrivilegeIncreaseWhenSwitchingForLowerPrivilegePermission) {
  APIPermissionSet apis1;
  apis1.insert(APIPermissionID::kHistory);
  PermissionSet permissions1(apis1.Clone(), ManifestPermissionSet(),
                             URLPatternSet(), URLPatternSet());

  APIPermissionSet apis2;
  apis2.insert(APIPermissionID::kTopSites);
  PermissionSet permissions2(apis2.Clone(), ManifestPermissionSet(),
                             URLPatternSet(), URLPatternSet());

  EXPECT_FALSE(PermissionMessageProvider::Get()->IsPrivilegeIncrease(
      permissions1, permissions2, Manifest::TYPE_EXTENSION));
}

TEST(PermissionsTest, PermissionMessages) {
  // Ensure that all permissions that needs to show install UI actually have
  // strings associated with them.
  APIPermissionSet skip;

  // These are considered "nuisance" or "trivial" permissions that don't need
  // a prompt.
  skip.insert(APIPermissionID::kActiveTab);
  skip.insert(APIPermissionID::kAlarms);
  skip.insert(APIPermissionID::kAlphaEnabled);
  skip.insert(APIPermissionID::kAlwaysOnTopWindows);
  skip.insert(APIPermissionID::kAppView);
  skip.insert(APIPermissionID::kAudio);
  skip.insert(APIPermissionID::kBrowsingData);
  skip.insert(APIPermissionID::kCommandsAccessibility);
  skip.insert(APIPermissionID::kContextMenus);
  skip.insert(APIPermissionID::kDiagnostics);
  skip.insert(APIPermissionID::kDns);
  skip.insert(APIPermissionID::kDownloadsShelf);
  skip.insert(APIPermissionID::kDownloadsUi);
  skip.insert(APIPermissionID::kFontSettings);
  skip.insert(APIPermissionID::kFullscreen);
  skip.insert(APIPermissionID::kGcm);
  skip.insert(APIPermissionID::kIdle);
  skip.insert(APIPermissionID::kImeWindowEnabled);
  skip.insert(APIPermissionID::kIdltest);
  skip.insert(APIPermissionID::kLoginState);
  skip.insert(APIPermissionID::kOffscreen);
  skip.insert(APIPermissionID::kOverrideEscFullscreen);
  skip.insert(APIPermissionID::kPointerLock);
  skip.insert(APIPermissionID::kPower);
  skip.insert(APIPermissionID::kPrinterProvider);
  skip.insert(APIPermissionID::kSearch);
  skip.insert(APIPermissionID::kSessions);
  skip.insert(APIPermissionID::kSidePanel);
  skip.insert(APIPermissionID::kStorage);
  skip.insert(APIPermissionID::kSystemCpu);
  skip.insert(APIPermissionID::kSystemDisplay);
  skip.insert(APIPermissionID::kSystemMemory);
  skip.insert(APIPermissionID::kSystemNetwork);
  skip.insert(APIPermissionID::kTts);
  skip.insert(APIPermissionID::kUnlimitedStorage);
  skip.insert(APIPermissionID::kWebcamPrivate);
  skip.insert(APIPermissionID::kWebView);
  skip.insert(APIPermissionID::kWindowShape);

  // TODO(erikkay) add a string for this permission.
  skip.insert(APIPermissionID::kBackground);

  skip.insert(APIPermissionID::kClipboard);

  // The cookie permission does nothing unless you have associated host
  // permissions.
  skip.insert(APIPermissionID::kCookie);

  // These are warned as part of host permission checks.
  skip.insert(APIPermissionID::kDeclarativeContent);
  skip.insert(APIPermissionID::kPageCapture);
  skip.insert(APIPermissionID::kProxy);
  skip.insert(APIPermissionID::kScripting);
  skip.insert(APIPermissionID::kTabCapture);
  skip.insert(APIPermissionID::kUserScripts);
  skip.insert(APIPermissionID::kWebRequest);
  skip.insert(APIPermissionID::kWebRequestBlocking);
  skip.insert(APIPermissionID::kWebRequestAuthProvider);
  skip.insert(APIPermissionID::kDeclarativeNetRequestWithHostAccess);

  // This permission requires explicit user action (context menu handler)
  // so we won't prompt for it for now.
  skip.insert(APIPermissionID::kFileBrowserHandler);

  // These permissions require explicit user action (configuration dialog)
  // so we don't prompt for them at install time.
  skip.insert(APIPermissionID::kMediaGalleries);

  // If you've turned on the experimental command-line flag, we don't need
  // to warn you further.
  skip.insert(APIPermissionID::kExperimental);

  // The Experimental AI Data API is gated on commandline switches, in
  // addition to the permission in the manifest. If you've turned on the
  // experimental AI Data command-line flag, we don't need to warn you further.
  skip.insert(APIPermissionID::kExperimentalAiData);

  // The Identity API has its own server-driven permission prompts.
  skip.insert(APIPermissionID::kIdentity);

  // This API is still in origin trial so we don't want to show a permission
  // prompt.
  skip.insert(APIPermissionID::kAIAssistantOriginTrial);

  // These are private.
  skip.insert(APIPermissionID::kAccessibilityPrivate);
  skip.insert(APIPermissionID::kAccessibilityServicePrivate);
  skip.insert(APIPermissionID::kArcAppsPrivate);
  skip.insert(APIPermissionID::kAutoTestPrivate);
  skip.insert(APIPermissionID::kBrailleDisplayPrivate);
  skip.insert(APIPermissionID::kCecPrivate);
  skip.insert(APIPermissionID::kChromeosInfoPrivate);
  skip.insert(APIPermissionID::kCommandLinePrivate);
  skip.insert(APIPermissionID::kCrashReportPrivate);
  skip.insert(APIPermissionID::kDeveloperPrivate);
  skip.insert(APIPermissionID::kEchoPrivate);
  skip.insert(APIPermissionID::kEnterprisePlatformKeysPrivate);
  skip.insert(APIPermissionID::kFeedbackPrivate);
  skip.insert(APIPermissionID::kFileManagerPrivate);
  skip.insert(APIPermissionID::kFirstRunPrivate);
  skip.insert(APIPermissionID::kSharedStoragePrivate);
  skip.insert(APIPermissionID::kImageLoaderPrivate);
  skip.insert(APIPermissionID::kInputMethodPrivate);
  skip.insert(APIPermissionID::kLanguageSettingsPrivate);
  skip.insert(APIPermissionID::kLockWindowFullscreenPrivate);
  skip.insert(APIPermissionID::kMediaPlayerPrivate);
  skip.insert(APIPermissionID::kMediaPerceptionPrivate);
  skip.insert(APIPermissionID::kMetricsPrivate);
  skip.insert(APIPermissionID::kPdfViewerPrivate);
  skip.insert(APIPermissionID::kImageWriterPrivate);
  skip.insert(APIPermissionID::kResourcesPrivate);
  skip.insert(APIPermissionID::kRtcPrivate);
  skip.insert(APIPermissionID::kSafeBrowsingPrivate);
  skip.insert(APIPermissionID::kSmartCardProviderPrivate);
  skip.insert(APIPermissionID::kSystemPrivate);
  skip.insert(APIPermissionID::kTabCaptureForTab);
  skip.insert(APIPermissionID::kTerminalPrivate);
  skip.insert(APIPermissionID::kVirtualKeyboardPrivate);
  skip.insert(APIPermissionID::kWebrtcAudioPrivate);
  skip.insert(APIPermissionID::kWebrtcDesktopCapturePrivate);
  skip.insert(APIPermissionID::kWebrtcLoggingPrivate);
  skip.insert(APIPermissionID::kWebrtcLoggingPrivateAudioDebug);
  skip.insert(APIPermissionID::kWebstorePrivate);
  skip.insert(APIPermissionID::kWmDesksPrivate);
  skip.insert(APIPermissionID::kSystemLog);
  skip.insert(APIPermissionID::kOdfsConfigPrivate);

  // Warned as part of host permissions.
  skip.insert(APIPermissionID::kDevtools);

  // Platform apps.
  skip.insert(APIPermissionID::kBrowser);
  skip.insert(APIPermissionID::kHid);
  skip.insert(APIPermissionID::kFileSystem);
  skip.insert(APIPermissionID::kFileSystemProvider);
  skip.insert(APIPermissionID::kFileSystemRequestFileSystem);
  skip.insert(APIPermissionID::kFileSystemRetainEntries);
  skip.insert(APIPermissionID::kFileSystemWrite);
  skip.insert(APIPermissionID::kSocket);
  skip.insert(APIPermissionID::kUsb);
  skip.insert(APIPermissionID::kVirtualKeyboard);

  // The lock screen apps are set by user through settings, no need to warn at
  // installation time.
  skip.insert(APIPermissionID::kLockScreen);

  // We already have a generic message for declaring externally_connectable.
  skip.insert(APIPermissionID::kDeprecated_ExternallyConnectableAllUrls);

  const PermissionMessageProvider* provider = PermissionMessageProvider::Get();
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  APIPermissionSet permissions = info->GetAllForTest();
  for (const auto* permission : permissions) {
    const APIPermissionInfo* permission_info = permission->info();
    EXPECT_TRUE(permission_info);

    PermissionIDSet id;
    id.insert(permission_info->id());
    bool has_message = !provider->GetPermissionMessages(id).empty();
    bool should_have_message = !skip.count(permission->id());
    EXPECT_EQ(should_have_message, has_message) << permission_info->name();
  }
}

TEST(PermissionsTest, FileSystemPermissionMessages) {
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermissionID::kFileSystemWrite);
  api_permissions.insert(APIPermissionID::kFileSystemDirectory);
  PermissionSet permissions(api_permissions.Clone(), ManifestPermissionSet(),
                            URLPatternSet(), URLPatternSet());
  EXPECT_TRUE(
      PermissionSetProducesMessage(permissions, Manifest::TYPE_PLATFORM_APP,
                                   MakePermissionIDSet(api_permissions)));
}

TEST(PermissionsTest, HiddenFileSystemPermissionMessages) {
  APIPermissionSet api_permissions;
  api_permissions.insert(APIPermissionID::kFileSystemWrite);
  api_permissions.insert(APIPermissionID::kFileSystemDirectory);
  PermissionSet permissions(api_permissions.Clone(), ManifestPermissionSet(),
                            URLPatternSet(), URLPatternSet());
  EXPECT_TRUE(
      PermissionSetProducesMessage(permissions, Manifest::TYPE_PLATFORM_APP,
                                   MakePermissionIDSet(api_permissions)));
}

TEST(PermissionsTest, SuppressedPermissionMessages) {
  {
    // Tabs warning suppresses favicon warning.
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kTab);
    URLPatternSet hosts;
    hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                                "chrome://favicon/"));
    PermissionSet permissions(std::move(api_permissions),
                              ManifestPermissionSet(), std::move(hosts),
                              URLPatternSet());
    EXPECT_TRUE(PermissionSetProducesMessage(
        permissions, Manifest::TYPE_EXTENSION,
        MakePermissionIDSet(APIPermissionID::kTab, APIPermissionID::kFavicon)));
  }
  {
    // History warning suppresses favicon warning.
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kHistory);
    URLPatternSet hosts;
    hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                                "chrome://favicon/"));
    PermissionSet permissions(api_permissions.Clone(), ManifestPermissionSet(),
                              std::move(hosts), URLPatternSet());
    EXPECT_TRUE(PermissionSetProducesMessage(
        permissions, Manifest::TYPE_EXTENSION,
        MakePermissionIDSet(APIPermissionID::kHistory,
                            APIPermissionID::kFavicon)));
  }
  {
    // All sites warning suppresses tabs warning.
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kTab);
    URLPatternSet hosts;
    hosts.AddPattern(URLPattern(URLPattern::SCHEME_HTTP, "*://*/*"));
    PermissionSet permissions(std::move(api_permissions),
                              ManifestPermissionSet(), std::move(hosts),
                              URLPatternSet());
    EXPECT_TRUE(PermissionSetProducesMessage(
        permissions, Manifest::TYPE_EXTENSION,
        MakePermissionIDSet(APIPermissionID::kHostsAll,
                            APIPermissionID::kTab)));
  }
  {
    // All sites warning suppresses topSites warning.
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kTopSites);
    URLPatternSet hosts;
    hosts.AddPattern(URLPattern(URLPattern::SCHEME_HTTP, "*://*/*"));
    PermissionSet permissions(std::move(api_permissions),
                              ManifestPermissionSet(), std::move(hosts),
                              URLPatternSet());
    EXPECT_TRUE(PermissionSetProducesMessage(
        permissions, Manifest::TYPE_EXTENSION,
        MakePermissionIDSet(APIPermissionID::kHostsAll,
                            APIPermissionID::kTopSites)));
  }
  {
    // All sites warning suppresses declarativeWebRequest warning.
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kDeclarativeWebRequest);
    URLPatternSet hosts;
    hosts.AddPattern(URLPattern(URLPattern::SCHEME_HTTP, "*://*/*"));
    PermissionSet permissions(std::move(api_permissions),
                              ManifestPermissionSet(), std::move(hosts),
                              URLPatternSet());
    EXPECT_TRUE(PermissionSetProducesMessage(
        permissions, Manifest::TYPE_EXTENSION,
        MakePermissionIDSet(APIPermissionID::kHostsAll,
                            APIPermissionID::kDeclarativeWebRequest)));
  }
  {
    // BrowsingHistory warning suppresses all history read/write warnings.
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kHistory);
    api_permissions.insert(APIPermissionID::kTab);
    api_permissions.insert(APIPermissionID::kTopSites);
    api_permissions.insert(APIPermissionID::kProcesses);
    api_permissions.insert(APIPermissionID::kWebNavigation);
    PermissionSet permissions(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet());
    EXPECT_TRUE(
        PermissionSetProducesMessage(permissions, Manifest::TYPE_EXTENSION,
                                     MakePermissionIDSet(api_permissions)));
  }
  {
    // Tabs warning suppresses all read-only history warnings.
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kTab);
    api_permissions.insert(APIPermissionID::kTopSites);
    api_permissions.insert(APIPermissionID::kProcesses);
    api_permissions.insert(APIPermissionID::kWebNavigation);
    PermissionSet permissions(api_permissions.Clone(), ManifestPermissionSet(),
                              URLPatternSet(), URLPatternSet());
    EXPECT_TRUE(
        PermissionSetProducesMessage(permissions, Manifest::TYPE_EXTENSION,
                                     MakePermissionIDSet(api_permissions)));
  }
}

TEST(PermissionsTest, AccessToDevicesMessages) {
  {
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kSerial);
    PermissionSet permissions(std::move(api_permissions),
                              ManifestPermissionSet(), URLPatternSet(),
                              URLPatternSet());
    VerifyOnePermissionMessage(
        permissions, Manifest::TYPE_EXTENSION,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_SERIAL));
  }
  {
    // Testing that multiple permissions will show the one message.
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kSerial);
    api_permissions.insert(APIPermissionID::kSerial);
    PermissionSet permissions(std::move(api_permissions),
                              ManifestPermissionSet(), URLPatternSet(),
                              URLPatternSet());
    VerifyOnePermissionMessage(
        permissions, Manifest::TYPE_EXTENSION,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_SERIAL));
  }
  {
    scoped_refptr<Extension> extension =
        LoadManifest("permissions", "access_to_devices_bluetooth.json");
    PermissionSet& set = const_cast<PermissionSet&>(
        extension->permissions_data()->active_permissions());
    VerifyOnePermissionMessage(
        set, extension->GetType(),
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH));

    // Test Bluetooth and Serial
    set.apis_.insert(APIPermissionID::kSerial);
    VerifyOnePermissionMessage(
        set, extension->GetType(),
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_SERIAL));
  }
}

TEST(PermissionsTest, MergedFileSystemPermissionComparison) {
  APIPermissionSet write_api_permissions;
  write_api_permissions.insert(APIPermissionID::kFileSystemWrite);
  PermissionSet write_permissions(write_api_permissions.Clone(),
                                  ManifestPermissionSet(), URLPatternSet(),
                                  URLPatternSet());

  APIPermissionSet directory_api_permissions;
  directory_api_permissions.insert(APIPermissionID::kFileSystemDirectory);
  PermissionSet directory_permissions(directory_api_permissions.Clone(),
                                      ManifestPermissionSet(), URLPatternSet(),
                                      URLPatternSet());

  APIPermissionSet write_directory_api_permissions;
  write_directory_api_permissions.insert(APIPermissionID::kFileSystemWrite);
  write_directory_api_permissions.insert(APIPermissionID::kFileSystemDirectory);
  PermissionSet write_directory_permissions(
      write_directory_api_permissions.Clone(), ManifestPermissionSet(),
      URLPatternSet(), URLPatternSet());

  const PermissionMessageProvider* provider = PermissionMessageProvider::Get();
  EXPECT_FALSE(provider->IsPrivilegeIncrease(write_directory_permissions,
                                             write_permissions,
                                             Manifest::TYPE_PLATFORM_APP));
  EXPECT_FALSE(provider->IsPrivilegeIncrease(write_directory_permissions,
                                             directory_permissions,
                                             Manifest::TYPE_PLATFORM_APP));
  EXPECT_TRUE(provider->IsPrivilegeIncrease(write_permissions,
                                            write_directory_permissions,
                                            Manifest::TYPE_PLATFORM_APP));
  EXPECT_TRUE(provider->IsPrivilegeIncrease(directory_permissions,
                                            write_directory_permissions,
                                            Manifest::TYPE_PLATFORM_APP));
  // Tricky case: going from kFileSystemWrite to kFileSystemDirectory (or vice
  // versa). A warning is only shown if *both* kFileSystemWrite and
  // kFileSystemDirectory are present. Even though kFileSystemWrite is not in
  // the new set of permissions, it will still be a granted permission.
  // Therefore, we should consider this a privilege increase.
  EXPECT_TRUE(provider->IsPrivilegeIncrease(
      write_permissions, directory_permissions, Manifest::TYPE_PLATFORM_APP));
  EXPECT_TRUE(provider->IsPrivilegeIncrease(
      directory_permissions, write_permissions, Manifest::TYPE_PLATFORM_APP));
}

TEST(PermissionsTest, GetWarningMessages_ManyHosts) {
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "many-hosts.json");
  EXPECT_TRUE(VerifyOnePermissionMessage(
      extension->permissions_data(),
      "Read and change your data on encrypted.google.com and www.google.com"));
}

TEST(PermissionsTest, GetWarningMessages_AudioVideo) {
  const std::string kAudio("Use your microphone");
  const std::string kVideo("Use your camera");
  const std::string kBoth("Use your microphone and camera");

  // Both audio and video present.
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "audio-video.json");
  const PermissionMessageProvider* provider = PermissionMessageProvider::Get();
  PermissionSet& set = const_cast<PermissionSet&>(
      extension->permissions_data()->active_permissions());
  EXPECT_FALSE(VerifyHasPermissionMessage(set, extension->GetType(), kAudio));
  EXPECT_FALSE(VerifyHasPermissionMessage(set, extension->GetType(), kVideo));
  EXPECT_TRUE(VerifyHasPermissionMessage(set, extension->GetType(), kBoth));
  PermissionMessages warnings = provider->GetPermissionMessages(
      provider->GetAllPermissionIDs(set, extension->GetType()));
  size_t combined_index = IndexOf(warnings, kBoth);
  size_t combined_size = warnings.size();

  // Just audio present.
  set.apis_.erase(APIPermissionID::kVideoCapture);
  EXPECT_TRUE(VerifyHasPermissionMessage(set, extension->GetType(), kAudio));
  EXPECT_FALSE(VerifyHasPermissionMessage(set, extension->GetType(), kVideo));
  EXPECT_FALSE(VerifyHasPermissionMessage(set, extension->GetType(), kBoth));
  PermissionMessages warnings2 = provider->GetPermissionMessages(
      provider->GetAllPermissionIDs(set, extension->GetType()));
  EXPECT_EQ(combined_size, warnings2.size());
  EXPECT_EQ(combined_index, IndexOf(warnings2, kAudio));

  // Just video present.
  set.apis_.erase(APIPermissionID::kAudioCapture);
  set.apis_.insert(APIPermissionID::kVideoCapture);
  EXPECT_FALSE(VerifyHasPermissionMessage(set, extension->GetType(), kAudio));
  EXPECT_TRUE(VerifyHasPermissionMessage(set, extension->GetType(), kVideo));
  EXPECT_FALSE(VerifyHasPermissionMessage(set, extension->GetType(), kBoth));
  PermissionMessages warnings3 = provider->GetPermissionMessages(
      provider->GetAllPermissionIDs(set, extension->GetType()));
  EXPECT_EQ(combined_size, warnings3.size());
  EXPECT_EQ(combined_index, IndexOf(warnings3, kVideo));
}

TEST(PermissionsTest, GetWarningMessages_CombinedSessions) {
  {
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kTab);
    api_permissions.insert(APIPermissionID::kTopSites);
    api_permissions.insert(APIPermissionID::kProcesses);
    api_permissions.insert(APIPermissionID::kWebNavigation);
    api_permissions.insert(APIPermissionID::kSessions);
    PermissionSet permissions(std::move(api_permissions),
                              ManifestPermissionSet(), URLPatternSet(),
                              URLPatternSet());
    EXPECT_TRUE(VerifyOnePermissionMessage(
        permissions, Manifest::TYPE_EXTENSION,
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ_ON_ALL_DEVICES)));
  }
  {
    APIPermissionSet api_permissions;
    api_permissions.insert(APIPermissionID::kHistory);
    api_permissions.insert(APIPermissionID::kTab);
    api_permissions.insert(APIPermissionID::kTopSites);
    api_permissions.insert(APIPermissionID::kProcesses);
    api_permissions.insert(APIPermissionID::kWebNavigation);
    api_permissions.insert(APIPermissionID::kSessions);
    PermissionSet permissions(std::move(api_permissions),
                              ManifestPermissionSet(), URLPatternSet(),
                              URLPatternSet());
    EXPECT_TRUE(VerifyOnePermissionMessage(
        permissions, Manifest::TYPE_EXTENSION,
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE_ON_ALL_DEVICES)));
  }
}

TEST(PermissionsTest, GetWarningMessages_DeclarativeWebRequest) {
  // Test that if the declarativeWebRequest permission is present
  // in combination with all hosts permission, then only the warning
  // for host permissions is shown, because that covers the use of
  // declarativeWebRequest.

  // Until Declarative Web Request is in stable, let's make sure it is enabled
  // on the current channel.
  ScopedCurrentChannel sc(version_info::Channel::CANARY);

  {
    // First verify that declarativeWebRequest produces a message when host
    // permissions do not cover all hosts.
    scoped_refptr<const Extension> extension = LoadManifest(
        "permissions", "web_request_not_all_host_permissions.json");
    const PermissionSet& set =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(VerifyHasPermissionMessage(set, extension->GetType(),
                                           "Block parts of web pages"));
    EXPECT_FALSE(VerifyHasPermissionMessage(
        set, extension->GetType(),
        "Read and change all your data on all websites"));
  }

  {
  // Now verify that declarativeWebRequest does not produce a message when host
  // permissions do cover all hosts.
  scoped_refptr<const Extension> extension =
      LoadManifest("permissions", "web_request_all_host_permissions.json");
  const PermissionSet& set =
      extension->permissions_data()->active_permissions();
  EXPECT_FALSE(VerifyHasPermissionMessage(set, extension->GetType(),
                                          "Block parts of web pages"));
  EXPECT_TRUE(VerifyHasPermissionMessage(
      set, extension->GetType(),
      "Read and change all your data on all websites"));
  }
}

TEST(PermissionsTest, GetWarningMessages_Serial) {
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "serial.json");

  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kSerial));
  EXPECT_TRUE(VerifyOnePermissionMessage(extension->permissions_data(),
                                         "Access your serial devices"));
}

TEST(PermissionsTest, GetWarningMessages_Socket_AnyHost) {
  ScopedCurrentChannel channel(version_info::Channel::DEV);

  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "socket_any_host.json");
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kSocket));
  EXPECT_TRUE(VerifyOnePermissionMessage(
      extension->permissions_data(),
      "Exchange data with any device on the local network or internet"));
}

TEST(PermissionsTest, GetWarningMessages_Socket_OneDomainTwoHostnames) {
  ScopedCurrentChannel channel(version_info::Channel::DEV);

  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "socket_one_domain_two_hostnames.json");
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kSocket));

  // Verify the warnings, including support for unicode characters, the fact
  // that domain host warnings come before specific host warnings, and the fact
  // that domains and hostnames are in alphabetical order regardless of the
  // order in the manifest file.
  EXPECT_TRUE(VerifyTwoPermissionMessages(
      extension->permissions_data(),
      "Exchange data with any device in the domain example.org",
      "Exchange data with the devices named: "
      "b\xC3\xA5r.example.com foo.example.com",
      // "\xC3\xA5" = UTF-8 for lowercase A with ring above
      true));
}

TEST(PermissionsTest, GetWarningMessages_Socket_TwoDomainsOneHostname) {
  ScopedCurrentChannel channel(version_info::Channel::DEV);

  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "socket_two_domains_one_hostname.json");
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kSocket));

  // Verify the warnings, including the fact that domain host warnings come
  // before specific host warnings and the fact that domains and hostnames are
  // in alphabetical order regardless of the order in the manifest file.
  EXPECT_TRUE(VerifyTwoPermissionMessages(
      extension->permissions_data(),
      "Exchange data with any device in the domains: "
      "example.com foo.example.org",
      "Exchange data with the device named bar.example.org", true));
}

// Since platform apps always use isolated storage, they can't (silently)
// access user data on other domains, so there's no need to prompt about host
// permissions. See crbug.com/255229.
TEST(PermissionsTest, GetWarningMessages_PlatformAppHosts) {
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "platform_app_hosts.json");
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(VerifyNoPermissionMessages(extension->permissions_data()));

  extension = LoadManifest("permissions", "platform_app_all_urls.json");
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(VerifyNoPermissionMessages(extension->permissions_data()));
}

testing::AssertionResult ShowsAllHostsWarning(const std::string& pattern) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TLDWildCardTest").AddHostPermission(pattern).Build();

  return VerifyHasPermissionMessage(
      extension->permissions_data(),
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS));
}

TEST(PermissionsTest, GetWarningMessages_TLDWildcardTreatedAsAllHosts) {
  EXPECT_TRUE(ShowsAllHostsWarning("http://*.com/*"));    // most popular.
  EXPECT_TRUE(ShowsAllHostsWarning("http://*.org/*"));    // sanity check.
  EXPECT_TRUE(ShowsAllHostsWarning("http://*.co.uk/*"));  // eTLD.
  EXPECT_TRUE(ShowsAllHostsWarning("http://*.de/*"));  // foreign country tld.

  // We should still show the normal permissions (i.e., "Can access your data on
  // *.rdcronin.com") for things that are not TLDs.
  EXPECT_FALSE(ShowsAllHostsWarning("http://*.rdcronin.com/*"));

  // Pseudo-TLDs, like appspot.com, should not show all hosts.
  EXPECT_FALSE(ShowsAllHostsWarning("http://*.appspot.com/*"));

  // Non-TLDs should be likewise exempt.
  EXPECT_FALSE(ShowsAllHostsWarning("http://*.notarealtld/*"));

  // Our internal checks use "foo", so let's make sure we're not messing
  // something up with it.
  EXPECT_FALSE(ShowsAllHostsWarning("http://*.foo.com"));
  EXPECT_FALSE(ShowsAllHostsWarning("http://foo.com"));
  // This will fail if foo becomes a recognized TLD. Which could be soon.
  // Update as needed.
  EXPECT_FALSE(ShowsAllHostsWarning("http://*.foo"));
}

TEST(PermissionsTest, GetDistinctHosts) {
  URLPatternSet explicit_hosts;
  std::set<std::string> expected;
  expected.insert("www.foo.com");
  expected.insert("www.bar.com");
  expected.insert("www.baz.com");

  {
    SCOPED_TRACE("no dupes");

    // Simple list with no dupes.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.baz.com/path"));
    EXPECT_EQ(expected,
              permission_message_util::GetDistinctHosts(
                  explicit_hosts, true, true));
  }

  {
    SCOPED_TRACE("two dupes");

    // Add some dupes.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.baz.com/path"));
    EXPECT_EQ(expected,
              permission_message_util::GetDistinctHosts(
                  explicit_hosts, true, true));
  }

  {
    SCOPED_TRACE("schemes differ");

    // Add a pattern that differs only by scheme. This should be filtered out.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTPS, "https://www.bar.com/path"));
    EXPECT_EQ(expected,
              permission_message_util::GetDistinctHosts(
                  explicit_hosts, true, true));
  }

  {
    SCOPED_TRACE("paths differ");

    // Add some dupes by path.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/pathypath"));
    EXPECT_EQ(expected,
              permission_message_util::GetDistinctHosts(
                  explicit_hosts, true, true));
  }

  {
    SCOPED_TRACE("subdomains differ");

    // We don't do anything special for subdomains.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://monkey.www.bar.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://bar.com/path"));

    expected.insert("monkey.www.bar.com");
    expected.insert("bar.com");

    EXPECT_EQ(expected,
              permission_message_util::GetDistinctHosts(
                  explicit_hosts, true, true));
  }

  {
    SCOPED_TRACE("RCDs differ");

    // Now test for RCD uniquing.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.de/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca.us/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.net/path"));
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com.my/path"));

    // This is an unknown RCD, which shouldn't be uniqued out.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.xyzzy/path"));
    // But it should only occur once.
    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.xyzzy/path"));

    expected.insert("www.foo.xyzzy");

    EXPECT_EQ(expected,
              permission_message_util::GetDistinctHosts(
                  explicit_hosts, true, true));
  }

  {
    SCOPED_TRACE("wildcards");

    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com/*"));

    expected.insert("*.google.com");

    EXPECT_EQ(expected,
              permission_message_util::GetDistinctHosts(
                  explicit_hosts, true, true));
  }

  {
    SCOPED_TRACE("scriptable hosts");

    explicit_hosts.ClearPatterns();
    URLPatternSet scriptable_hosts;
    expected.clear();

    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://*.google.com/*"));
    scriptable_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_HTTP, "http://*.example.com/*"));

    expected.insert("*.google.com");
    expected.insert("*.example.com");

    PermissionSet perm_set(APIPermissionSet(), ManifestPermissionSet(),
                           std::move(explicit_hosts),
                           std::move(scriptable_hosts));
    EXPECT_EQ(expected, permission_message_util::GetDistinctHosts(
                            perm_set.effective_hosts(), true, true));
  }

  {
    // We don't display warnings for file URLs because they are off by default.
    SCOPED_TRACE("file urls");

    explicit_hosts.ClearPatterns();
    expected.clear();

    explicit_hosts.AddPattern(
        URLPattern(URLPattern::SCHEME_FILE, "file:///*"));

    EXPECT_EQ(expected,
              permission_message_util::GetDistinctHosts(
                  explicit_hosts, true, true));
  }
}

TEST(PermissionsTest, GetDistinctHosts_ComIsBestRcd) {
  URLPatternSet explicit_hosts;
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.org/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.net/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.jp/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));

  std::set<std::string> expected;
  expected.insert("www.foo.com");
  EXPECT_EQ(expected,
            permission_message_util::GetDistinctHosts(
                explicit_hosts, true, true));
}

TEST(PermissionsTest, GetDistinctHosts_NetIs2ndBestRcd) {
  URLPatternSet explicit_hosts;
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.org/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.net/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.jp/path"));
  // No http://www.foo.com/path

  std::set<std::string> expected;
  expected.insert("www.foo.net");
  EXPECT_EQ(expected,
            permission_message_util::GetDistinctHosts(
                explicit_hosts, true, true));
}

TEST(PermissionsTest, GetDistinctHosts_OrgIs3rdBestRcd) {
  URLPatternSet explicit_hosts;
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.org/path"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
  // No http://www.foo.net/path
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.jp/path"));
  // No http://www.foo.com/path

  std::set<std::string> expected;
  expected.insert("www.foo.org");
  EXPECT_EQ(expected,
            permission_message_util::GetDistinctHosts(
                explicit_hosts, true, true));
}

TEST(PermissionsTest, GetDistinctHosts_FirstInListIs4thBestRcd) {
  URLPatternSet explicit_hosts;
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.ca/path"));
  // No http://www.foo.org/path
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.co.uk/path"));
  // No http://www.foo.net/path
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.jp/path"));
  // No http://www.foo.com/path

  std::set<std::string> expected;
  expected.insert("www.foo.ca");
  EXPECT_EQ(expected,
            permission_message_util::GetDistinctHosts(
                explicit_hosts, true, true));
}

TEST(PermissionsTest, IsHostPrivilegeIncrease) {
  const struct {
    struct host_spec {
      int schemes;
      std::string pattern;
    };
    std::vector<host_spec> initial_hosts;
    std::vector<host_spec> final_hosts;
    Manifest::Type type;
    bool is_increase;
    bool reverse_is_increase;
  } test_cases[] = {
      // Order doesn't matter.
      {{{URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"},
        {URLPattern::SCHEME_HTTP, "http://www.google.com/path"}},
       {{URLPattern::SCHEME_HTTP, "http://www.google.com/path"},
        {URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"}},
       Manifest::TYPE_EXTENSION,
       false,
       false},
      // Paths are ignored.
      {{{URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"},
        {URLPattern::SCHEME_HTTP, "http://www.google.com/path"}},
       {{URLPattern::SCHEME_HTTP, "http://www.google.com/*"}},
       Manifest::TYPE_EXTENSION,
       false,
       false},
      // RCDs are ignored.
      {{{URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"},
        {URLPattern::SCHEME_HTTP, "http://www.google.com/path"}},
       {{URLPattern::SCHEME_HTTP, "http://www.google.com.hk/*"}},
       Manifest::TYPE_EXTENSION,
       false,
       false},
      // Subdomain wildcards are handled properly.
      {{{URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"},
        {URLPattern::SCHEME_HTTP, "http://www.google.com/path"}},
       {{URLPattern::SCHEME_HTTP, "http://*.google.com.hk/*"}},
       Manifest::TYPE_EXTENSION,
       true,
       false},
      // Different domains count as different hosts.
      {{{URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"},
        {URLPattern::SCHEME_HTTP, "http://www.google.com/path"}},
       {{URLPattern::SCHEME_HTTP, "http://www.google.com/path"},
        {URLPattern::SCHEME_HTTP, "http://www.example.org/path"}},
       Manifest::TYPE_EXTENSION,
       true,
       false},
      // Different subdomains count as different hosts.
      {{{URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"},
        {URLPattern::SCHEME_HTTP, "http://www.google.com/path"}},
       {{URLPattern::SCHEME_HTTP, "http://mail.google.com/*"}},
       Manifest::TYPE_EXTENSION,
       true,
       true},
      // Moving from all subdomains to the domain should not be
      // an increase in permissions. However, moving from just
      // the domain to all of the subdomains should be.
      {{{URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
         "*://*.google.com/*"}},
       {{URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
         "*://google.com/*"}},
       Manifest::TYPE_EXTENSION,
       false,
       true},
      // Platform apps should not have host permissions increases.
      {{{URLPattern::SCHEME_HTTP, "http://www.google.com.hk/path"},
        {URLPattern::SCHEME_HTTP, "http://www.google.com/path"}},
       {{URLPattern::SCHEME_HTTP, "http://mail.google.com/*"}},
       Manifest::TYPE_PLATFORM_APP,
       false,
       false},
      // Test that subdomain wildcard matching from crbug.com://65337
      // works.
      {{{URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
         "*://*.google.com/"},
        {URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
         "*://mail.google.com/"}},
       {{URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
         "*://inbox.google.com/"}},
       Manifest::TYPE_EXTENSION,
       false,
       true},
      // Test the "all_urls" meta-pattern.
      {{{URLPattern::SCHEME_ALL, "<all_urls>"}},
       {{URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
         "*://inbox.google.com/"}},
       Manifest::TYPE_EXTENSION,
       false,
       true},
      // Test expanding from any .com host to any host in any TLD.
      // TODO(crbug.com/40579475): Should this really be a permissions increase?
      {{{URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS, "*://*.com/*"}},
       {{URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS, "*://*/*"}},
       Manifest::TYPE_EXTENSION,
       true,
       false},
  };
  const PermissionMessageProvider* provider = PermissionMessageProvider::Get();
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    URLPatternSet explicit_hosts1;
    URLPatternSet explicit_hosts2;
    const auto& test_case = test_cases[i];
    for (const auto& initial_host : test_case.initial_hosts) {
      explicit_hosts1.AddPattern(
          URLPattern(initial_host.schemes, initial_host.pattern));
    }
    for (const auto& final_host : test_case.final_hosts) {
      explicit_hosts2.AddPattern(
          URLPattern(final_host.schemes, final_host.pattern));
    }
    const PermissionSet set1(APIPermissionSet(), ManifestPermissionSet(),
                             std::move(explicit_hosts1), URLPatternSet());
    const PermissionSet set2(APIPermissionSet(), ManifestPermissionSet(),
                             std::move(explicit_hosts2), URLPatternSet());
    EXPECT_EQ(test_case.is_increase,
              provider->IsPrivilegeIncrease(set1, set2, test_case.type))
        << "Failure at index " << i;
    EXPECT_EQ(test_case.reverse_is_increase,
              provider->IsPrivilegeIncrease(set2, set1, test_case.type))
        << "Failure at index " << i;
  }
}

TEST(PermissionsTest, GetAPIsAsStrings) {
  APIPermissionSet apis;

  apis.insert(APIPermissionID::kProxy);
  apis.insert(APIPermissionID::kBackground);
  apis.insert(APIPermissionID::kNotifications);
  apis.insert(APIPermissionID::kTab);

  PermissionSet perm_set(apis.Clone(), ManifestPermissionSet(), URLPatternSet(),
                         URLPatternSet());
  std::set<std::string> api_names = perm_set.GetAPIsAsStrings();

  // The result is correct if it has the same number of elements
  // and we can convert it back to the id set.
  EXPECT_EQ(4u, api_names.size());
  EXPECT_EQ(apis,
            PermissionsInfo::GetInstance()->GetAllByNameForTest(api_names));
}

TEST(PermissionsTest, IsEmpty) {
  std::unique_ptr<const PermissionSet> empty(new PermissionSet());
  EXPECT_TRUE(empty->IsEmpty());
  std::unique_ptr<const PermissionSet> perm_set;

  perm_set = std::make_unique<PermissionSet>(APIPermissionSet(),
                                             ManifestPermissionSet(),
                                             URLPatternSet(), URLPatternSet());
  EXPECT_TRUE(perm_set->IsEmpty());

  APIPermissionSet non_empty_apis;
  non_empty_apis.insert(APIPermissionID::kBackground);
  perm_set = std::make_unique<PermissionSet>(std::move(non_empty_apis),
                                             ManifestPermissionSet(),
                                             URLPatternSet(), URLPatternSet());
  EXPECT_FALSE(perm_set->IsEmpty());

  // Try non standard host
  URLPatternSet non_empty_extent;
  AddPattern(&non_empty_extent, "http://www.google.com/*");

  perm_set = std::make_unique<PermissionSet>(
      APIPermissionSet(), ManifestPermissionSet(), non_empty_extent.Clone(),
      URLPatternSet());
  EXPECT_FALSE(perm_set->IsEmpty());

  perm_set = std::make_unique<PermissionSet>(
      APIPermissionSet(), ManifestPermissionSet(), URLPatternSet(),
      non_empty_extent.Clone());
  EXPECT_FALSE(perm_set->IsEmpty());
}

TEST(PermissionsTest, SyncFileSystemPermission) {
  scoped_refptr<Extension> extension = LoadManifest(
      "permissions", "sync_file_system.json");
  APIPermissionSet apis;
  apis.insert(APIPermissionID::kSyncFileSystem);
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermissionID::kSyncFileSystem));
  EXPECT_TRUE(
      VerifyOnePermissionMessage(extension->permissions_data(),
                                 "Store data in your Google Drive account"));
}

// Make sure that we don't crash when we're trying to show the permissions
// even though everything with a chrome:// scheme except chrome://favicon is
// not a valid permission.
// More details here: crbug/246314.
TEST(PermissionsTest, ChromeURLs) {
  URLPatternSet allowed_hosts;
  allowed_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "http://www.google.com/"));
  allowed_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "chrome://favicon/"));
  allowed_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "chrome://not-favicon/"));
  PermissionSet permissions(APIPermissionSet(), ManifestPermissionSet(),
                            std::move(allowed_hosts), URLPatternSet());
  PermissionMessageProvider::Get()->GetPermissionMessages(
      PermissionMessageProvider::Get()->GetAllPermissionIDs(
          permissions, Manifest::TYPE_EXTENSION));
}

TEST(PermissionsTest, IsPrivilegeIncrease_DeclarativeWebRequest) {
  scoped_refptr<Extension> extension(
      LoadManifest("permissions", "permissions_all_urls.json"));
  const PermissionSet& permissions =
      extension->permissions_data()->active_permissions();

  scoped_refptr<Extension> extension_dwr(
      LoadManifest("permissions", "web_request_all_host_permissions.json"));
  const PermissionSet& permissions_dwr =
      extension_dwr->permissions_data()->active_permissions();

  EXPECT_FALSE(PermissionMessageProvider::Get()->IsPrivilegeIncrease(
      permissions, permissions_dwr, extension->GetType()));
}

// Exercises setting different members in the PermissionSet. Due to varying
// amounts of initialization, these can be non-trivial and have side-effects.
TEST(PermissionsTest, SettingMembers) {
  URLPattern first_host(URLPattern::SCHEME_ALL, "http://first.example/*");
  URLPattern second_host(URLPattern::SCHEME_ALL, "http://second.example/*");
  URLPattern third_host(URLPattern::SCHEME_ALL, "http://third.example/*");
  URLPattern all_hosts(URLPattern::SCHEME_ALL, "<all_urls>");

  {
    // Setting explicit hosts also sets effective hosts.
    PermissionSet set(APIPermissionSet(), ManifestPermissionSet(),
                      URLPatternSet({first_host}),
                      URLPatternSet({second_host}));
    set.SetExplicitHosts(URLPatternSet({third_host}));
    EXPECT_EQ(URLPatternSet({third_host}), set.explicit_hosts());
    EXPECT_EQ(URLPatternSet({second_host}), set.scriptable_hosts());
    EXPECT_EQ(URLPatternSet({second_host, third_host}), set.effective_hosts());
  }

  {
    // Setting scriptable hosts also sets effective hosts.
    PermissionSet set(APIPermissionSet(), ManifestPermissionSet(),
                      URLPatternSet({first_host}),
                      URLPatternSet({second_host}));
    set.SetScriptableHosts(URLPatternSet({third_host}));
    EXPECT_EQ(URLPatternSet({first_host}), set.explicit_hosts());
    EXPECT_EQ(URLPatternSet({third_host}), set.scriptable_hosts());
    EXPECT_EQ(URLPatternSet({first_host, third_host}), set.effective_hosts());
  }

  {
    // Setting explicit hosts recalculates whether to warn for all URLs.
    PermissionSet set(APIPermissionSet(), ManifestPermissionSet(),
                      URLPatternSet({first_host}),
                      URLPatternSet({second_host}));
    EXPECT_FALSE(set.ShouldWarnAllHosts());
    set.SetExplicitHosts(URLPatternSet({all_hosts}));
    EXPECT_TRUE(set.ShouldWarnAllHosts());
  }

  {
    // Setting scriptable hosts recalculates whether to warn for all URLs.
    PermissionSet set(APIPermissionSet(), ManifestPermissionSet(),
                      URLPatternSet({first_host}),
                      URLPatternSet({second_host}));
    EXPECT_FALSE(set.ShouldWarnAllHosts());
    set.SetExplicitHosts(URLPatternSet({all_hosts}));
    EXPECT_TRUE(set.ShouldWarnAllHosts());
  }

  {
    // Newly-set explicit hosts have their paths set to "/*".
    PermissionSet set(APIPermissionSet(), ManifestPermissionSet(),
                      URLPatternSet({first_host}),
                      URLPatternSet({second_host}));
    URLPattern custom_path(URLPattern::SCHEME_ALL, "https://path.example/path");
    URLPattern cleaned_path(URLPattern::SCHEME_ALL, "https://path.example/*");
    set.SetExplicitHosts(URLPatternSet({custom_path}));
    EXPECT_EQ(URLPatternSet({cleaned_path}), set.explicit_hosts());
  }

  {
    // Setting API permissions recalculates whether to warn for all URLs.
    APIPermissionSet apis;
    apis.insert(APIPermissionID::kTab);
    PermissionSet set(std::move(apis), ManifestPermissionSet(), URLPatternSet(),
                      URLPatternSet());
    EXPECT_FALSE(set.ShouldWarnAllHosts());
    APIPermissionSet new_apis;
    new_apis.insert(APIPermissionID::kDebugger);
    set.SetAPIPermissions(std::move(new_apis));
    EXPECT_TRUE(set.ShouldWarnAllHosts());
  }

  {
    // Setting API permissions adds implicit permissions.
    PermissionSet set;
    APIPermissionSet new_apis;
    new_apis.insert(APIPermissionID::kFileBrowserHandler);
    set.SetAPIPermissions(std::move(new_apis));
    EXPECT_TRUE(set.HasAPIPermission(APIPermissionID::kFileBrowserHandler));
  }
}

}  // namespace extensions
