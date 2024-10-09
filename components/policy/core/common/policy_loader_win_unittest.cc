// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_win.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <userenv.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace policy {

namespace {

// Constants for registry key names.
const wchar_t kPathSep[] = L"\\";
const wchar_t kThirdParty[] = L"3rdparty";
const wchar_t kMandatory[] = L"policy";
const wchar_t kRecommended[] = L"recommended";
const wchar_t kTestPolicyKey[] = L"chrome.policy.key";

// Installs |value| in the given registry |path| and |hive|, under the key
// |name|. Returns false on errors.
// Some of the possible Value types are stored after a conversion (e.g. doubles
// are stored as strings), and can only be retrieved if a corresponding schema
// is written.
bool InstallValue(const base::Value& value,
                  HKEY hive,
                  const std::wstring& path,
                  const std::wstring& name) {
  // KEY_ALL_ACCESS causes the ctor to create the key if it does not exist yet.
  RegKey key(hive, path.c_str(), KEY_ALL_ACCESS);
  EXPECT_TRUE(key.Valid());
  switch (value.type()) {
    case base::Value::Type::NONE:
      return key.WriteValue(name.c_str(), L"") == ERROR_SUCCESS;

    case base::Value::Type::BOOLEAN: {
      if (!value.is_bool())
        return false;
      return key.WriteValue(name.c_str(), value.GetBool() ? 1 : 0) ==
             ERROR_SUCCESS;
    }

    case base::Value::Type::INTEGER: {
      if (!value.is_int())
        return false;
      return key.WriteValue(name.c_str(), value.GetInt()) == ERROR_SUCCESS;
    }

    case base::Value::Type::DOUBLE: {
      std::wstring str_value = base::NumberToWString(value.GetDouble());
      return key.WriteValue(name.c_str(), str_value.c_str()) == ERROR_SUCCESS;
    }

    case base::Value::Type::STRING: {
      if (!value.is_string())
        return false;
      return key.WriteValue(
                 name.c_str(),
                 base::as_wcstr(base::UTF8ToUTF16(value.GetString()))) ==
             ERROR_SUCCESS;
    }

    case base::Value::Type::DICT: {
      if (!value.is_dict())
        return false;
      for (auto key_value : value.GetDict()) {
        if (!InstallValue(key_value.second, hive, path + kPathSep + name,
                          base::UTF8ToWide(key_value.first))) {
          return false;
        }
      }
      return true;
    }

    case base::Value::Type::LIST: {
      if (!value.is_list())
        return false;
      const base::Value::List& list = value.GetList();
      for (size_t i = 0; i < list.size(); ++i) {
        if (!InstallValue(list[i], hive, path + kPathSep + name,
                          base::NumberToWString(i + 1))) {
          return false;
        }
      }
      return true;
    }

    case base::Value::Type::BINARY:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// This class provides sandboxing and mocking for the parts of the Windows
// Registry implementing Group Policy. It prepares two temporary sandbox keys,
// one for HKLM and one for HKCU. A test's calls to the registry are redirected
// by Windows to these sandboxes, allowing the tests to manipulate and access
// policy as if it were active, but without actually changing the parts of the
// Registry that are managed by Group Policy.
class ScopedGroupPolicyRegistrySandbox {
 public:
  ScopedGroupPolicyRegistrySandbox();
  ScopedGroupPolicyRegistrySandbox(const ScopedGroupPolicyRegistrySandbox&) =
      delete;
  ScopedGroupPolicyRegistrySandbox& operator=(
      const ScopedGroupPolicyRegistrySandbox&) = delete;
  ~ScopedGroupPolicyRegistrySandbox();

  // Activates the registry keys overrides. This must be called before doing any
  // writes to registry and the call should be wrapped in
  // ASSERT_NO_FATAL_FAILURE.
  void ActivateOverrides();

 private:
  void RemoveOverrides();

  // Deletes the sandbox keys.
  void DeleteKeys();

  std::wstring key_name_;

  // Keys are created for the lifetime of a test to contain
  // the sandboxed HKCU and HKLM hives, respectively.
  RegKey temp_hkcu_hive_key_;
  RegKey temp_hklm_hive_key_;
};

// A test harness that feeds policy via the Chrome GPO registry subtree.
class RegistryTestHarness : public PolicyProviderTestHarness {
 public:
  RegistryTestHarness(HKEY hive, PolicyScope scope);
  RegistryTestHarness(const RegistryTestHarness&) = delete;
  RegistryTestHarness& operator=(const RegistryTestHarness&) = delete;
  ~RegistryTestHarness() override;

  // PolicyProviderTestHarness:
  void SetUp() override;

  ConfigurationPolicyProvider* CreateProvider(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;

  void InstallEmptyPolicy() override;
  void InstallStringPolicy(const std::string& policy_name,
                           const std::string& policy_value) override;
  void InstallIntegerPolicy(const std::string& policy_name,
                            int policy_value) override;
  void InstallBooleanPolicy(const std::string& policy_name,
                            bool policy_value) override;
  void InstallStringListPolicy(const std::string& policy_name,
                               const base::Value::List& policy_value) override;
  void InstallDictionaryPolicy(const std::string& policy_name,
                               const base::Value::Dict& policy_value) override;
  void Install3rdPartyPolicy(const base::Value::Dict& policies) override;

  // Creates a harness instance that will install policy in HKCU or HKLM,
  // respectively.
  static PolicyProviderTestHarness* CreateHKCU();
  static PolicyProviderTestHarness* CreateHKLM();

 private:
  HKEY hive_;

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;
};

ScopedGroupPolicyRegistrySandbox::ScopedGroupPolicyRegistrySandbox() = default;

ScopedGroupPolicyRegistrySandbox::~ScopedGroupPolicyRegistrySandbox() {
  RemoveOverrides();
  DeleteKeys();
}

void ScopedGroupPolicyRegistrySandbox::ActivateOverrides() {
  // Generate a unique registry key for the override for each test. This
  // makes sure that tests executing in parallel won't delete each other's
  // key, at DeleteKeys().
  key_name_ = base::ASCIIToWide(base::StringPrintf(
      "SOFTWARE\\chromium unittest %" CrPRIdPid, base::GetCurrentProcId()));
  std::wstring hklm_key_name = key_name_ + L"\\HKLM";
  std::wstring hkcu_key_name = key_name_ + L"\\HKCU";

  // Delete the registry test keys if they already exist (this could happen if
  // the process id got recycled and the last test running under the same
  // process id crashed ).
  DeleteKeys();

  // Create the subkeys to hold the overridden HKLM and HKCU
  // policy settings.
  ASSERT_EQ(temp_hklm_hive_key_.Create(HKEY_CURRENT_USER, hklm_key_name.c_str(),
                                       KEY_ALL_ACCESS),
            ERROR_SUCCESS);
  ASSERT_EQ(temp_hkcu_hive_key_.Create(HKEY_CURRENT_USER, hkcu_key_name.c_str(),
                                       KEY_ALL_ACCESS),
            ERROR_SUCCESS);

  auto result_override_hklm =
      RegOverridePredefKey(HKEY_LOCAL_MACHINE, temp_hklm_hive_key_.Handle());
  auto result_override_hkcu =
      RegOverridePredefKey(HKEY_CURRENT_USER, temp_hkcu_hive_key_.Handle());

  if (result_override_hklm != ERROR_SUCCESS ||
      result_override_hkcu != ERROR_SUCCESS) {
    // We need to remove the overrides first in case one succeeded and one
    // failed, otherwise deleting the keys fails.
    RemoveOverrides();
    DeleteKeys();

    // Assert on the actual results to print the error code in failure case.
    ASSERT_HRESULT_SUCCEEDED(result_override_hklm);
    ASSERT_HRESULT_SUCCEEDED(result_override_hkcu);
  }
}

void ScopedGroupPolicyRegistrySandbox::RemoveOverrides() {
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_LOCAL_MACHINE, nullptr));
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_CURRENT_USER, nullptr));
}

void ScopedGroupPolicyRegistrySandbox::DeleteKeys() {
  RegKey key(HKEY_CURRENT_USER, key_name_.c_str(), KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.DeleteKey(L"");
}

RegistryTestHarness::RegistryTestHarness(HKEY hive, PolicyScope scope)
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, scope,
                                POLICY_SOURCE_PLATFORM),
      hive_(hive) {
}

RegistryTestHarness::~RegistryTestHarness() = default;

void RegistryTestHarness::SetUp() {
  // SetUp is called at gtest SetUp time, and gtest documentation guarantees
  // that the test will not be executed if SetUp has a fatal failure. This is
  // important, see crbug.com/721691.
  ASSERT_NO_FATAL_FAILURE(registry_sandbox_.ActivateOverrides());
}

ConfigurationPolicyProvider* RegistryTestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  base::win::ScopedDomainStateForTesting scoped_domain(true);
  std::unique_ptr<AsyncPolicyLoader> loader(new PolicyLoaderWin(
      task_runner, PlatformManagementService::GetInstance(), kTestPolicyKey));
  return new AsyncPolicyProvider(registry, std::move(loader));
}

void RegistryTestHarness::InstallEmptyPolicy() {}

void RegistryTestHarness::InstallStringPolicy(
    const std::string& policy_name,
    const std::string& policy_value) {
  RegKey key(hive_, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  ASSERT_HRESULT_SUCCEEDED(
      key.WriteValue(base::UTF8ToWide(policy_name).c_str(),
                     base::UTF8ToWide(policy_value).c_str()));
}

void RegistryTestHarness::InstallIntegerPolicy(
    const std::string& policy_name,
    int policy_value) {
  RegKey key(hive_, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(base::UTF8ToWide(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void RegistryTestHarness::InstallBooleanPolicy(
    const std::string& policy_name,
    bool policy_value) {
  RegKey key(hive_, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(base::UTF8ToWide(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void RegistryTestHarness::InstallStringListPolicy(
    const std::string& policy_name,
    const base::Value::List& policy_value) {
  RegKey key(
      hive_,
      (std::wstring(kTestPolicyKey) + L"\\" + base::UTF8ToWide(policy_name))
          .c_str(),
      KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  int index = 1;
  for (const auto& element : policy_value) {
    if (!element.is_string())
      continue;

    std::string name(base::NumberToString(index++));
    key.WriteValue(base::UTF8ToWide(name).c_str(),
                   base::UTF8ToWide(element.GetString()).c_str());
  }
}

void RegistryTestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::Value::Dict& policy_value) {
  std::string json;
  base::JSONWriter::Write(policy_value, &json);
  RegKey key(hive_, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(base::UTF8ToWide(policy_name).c_str(),
                 base::UTF8ToWide(json).c_str());
}

void RegistryTestHarness::Install3rdPartyPolicy(
    const base::Value::Dict& policies) {
  // The first level entries are domains, and the second level entries map
  // components to their policy.
  const std::wstring kPathPrefix =
      std::wstring(kTestPolicyKey) + kPathSep + kThirdParty + kPathSep;
  for (auto domain : policies) {
    const base::Value& components = domain.second;
    if (!components.is_dict()) {
      ADD_FAILURE();
      continue;
    }
    for (auto component : components.GetDict()) {
      const std::wstring path = kPathPrefix + base::UTF8ToWide(domain.first) +
                                kPathSep + base::UTF8ToWide(component.first);
      InstallValue(component.second, hive_, path, kMandatory);
    }
  }
}

// static
PolicyProviderTestHarness* RegistryTestHarness::CreateHKCU() {
  return new RegistryTestHarness(HKEY_CURRENT_USER, POLICY_SCOPE_USER);
}

// static
PolicyProviderTestHarness* RegistryTestHarness::CreateHKLM() {
  return new RegistryTestHarness(HKEY_LOCAL_MACHINE, POLICY_SCOPE_MACHINE);
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_SUITE_P(PolicyProviderWinTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(RegistryTestHarness::CreateHKCU,
                                         RegistryTestHarness::CreateHKLM));

// Instantiate abstract test case for 3rd party policy reading tests.
INSTANTIATE_TEST_SUITE_P(ThirdPartyPolicyProviderWinTest,
                         Configuration3rdPartyPolicyProviderTest,
                         testing::Values(RegistryTestHarness::CreateHKCU,
                                         RegistryTestHarness::CreateHKLM));

// Test cases for windows policy provider specific functionality.
class PolicyLoaderWinTest : public PolicyTestBase {
 protected:
  // The policy key this tests places data under. This must match the data
  // files in chrome/test/data/policy/gpo.
  static const wchar_t kTestPolicyKey[];

  PolicyLoaderWinTest() : scoped_domain_(false) {}
  ~PolicyLoaderWinTest() override = default;

  void SetUp() override {
    PolicyTestBase::SetUp();

    // Activate overrides of registry keys. gtest documentation guarantees
    // that the test will not be executed if SetUp has a fatal failure. This is
    // important, see crbug.com/721691.
    ASSERT_NO_FATAL_FAILURE(registry_sandbox_.ActivateOverrides());
  }

  bool Matches(const PolicyBundle& expected) {
    PolicyLoaderWin loader(task_environment_.GetMainThreadTaskRunner(),
                           PlatformManagementService::GetInstance(),
                           kTestPolicyKey);
    PolicyBundle loaded = loader.InitialLoad(schema_registry_.schema_map());
    return loaded.Equals(expected);
  }

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;
  base::win::ScopedDomainStateForTesting scoped_domain_;
};

const wchar_t PolicyLoaderWinTest::kTestPolicyKey[] =
    L"SOFTWARE\\Policies\\Chromium";

TEST_F(PolicyLoaderWinTest, HKLMOverHKCU) {
  RegKey hklm_key(HKEY_LOCAL_MACHINE, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hklm_key.Valid());
  hklm_key.WriteValue(base::UTF8ToWide(test_keys::kKeyString).c_str(),
                      base::UTF8ToWide("hklm").c_str());
  RegKey hkcu_key(HKEY_CURRENT_USER, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hkcu_key.Valid());
  hkcu_key.WriteValue(base::UTF8ToWide(test_keys::kKeyString).c_str(),
                      base::UTF8ToWide("hkcu").c_str());

  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
           POLICY_SOURCE_PLATFORM, base::Value("hklm"), nullptr);
  expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .GetMutable(test_keys::kKeyString)
      ->AddMessage(PolicyMap::MessageType::kWarning,
                   IDS_POLICY_CONFLICT_DIFF_VALUE);

  PolicyMap::Entry conflict(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                            POLICY_SOURCE_PLATFORM, base::Value("hkcu"),
                            nullptr);
  expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .GetMutable(test_keys::kKeyString)
      ->AddConflictingPolicy(std::move(conflict));
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, Merge3rdPartyPolicies) {
  // Policy for the same extension will be provided at the 4 level/scope
  // combinations, to verify that they overlap as expected.
  const PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, "merge");
  ASSERT_TRUE(RegisterSchema(
      ns,
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"a\": { \"type\": \"string\" },"
      "    \"b\": { \"type\": \"string\" },"
      "    \"c\": { \"type\": \"string\" },"
      "    \"d\": { \"type\": \"string\" }"
      "  }"
      "}"));

  const std::wstring kPathSuffix =
      kTestPolicyKey + std::wstring(L"\\3rdparty\\extensions\\merge");

  const char kUserMandatory[] = "user-mandatory";
  const char kUserRecommended[] = "user-recommended";
  const char kMachineMandatory[] = "machine-mandatory";
  const char kMachineRecommended[] = "machine-recommended";

  base::Value::Dict policy;
  policy.Set("a", kMachineMandatory);
  EXPECT_TRUE(InstallValue(base::Value(policy.Clone()), HKEY_LOCAL_MACHINE,
                           kPathSuffix, kMandatory));
  policy.Set("a", kUserMandatory);
  policy.Set("b", kUserMandatory);
  EXPECT_TRUE(InstallValue(base::Value(policy.Clone()), HKEY_CURRENT_USER,
                           kPathSuffix, kMandatory));
  policy.Set("a", kMachineRecommended);
  policy.Set("b", kMachineRecommended);
  policy.Set("c", kMachineRecommended);
  EXPECT_TRUE(InstallValue(base::Value(policy.Clone()), HKEY_LOCAL_MACHINE,
                           kPathSuffix, kRecommended));
  policy.Set("a", kUserRecommended);
  policy.Set("b", kUserRecommended);
  policy.Set("c", kUserRecommended);
  policy.Set("d", kUserRecommended);
  EXPECT_TRUE(InstallValue(base::Value(policy.Clone()), HKEY_CURRENT_USER,
                           kPathSuffix, kRecommended));

  PolicyBundle expected;
  PolicyMap& expected_policy = expected.Get(ns);
  expected_policy.Set("a", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, base::Value(kMachineMandatory),
                      nullptr);
  expected_policy.GetMutable("a")->AddMessage(PolicyMap::MessageType::kWarning,
                                              IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected_policy.GetMutable("a")->AddMessage(PolicyMap::MessageType::kWarning,
                                              IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected_policy.GetMutable("a")->AddMessage(PolicyMap::MessageType::kWarning,
                                              IDS_POLICY_CONFLICT_DIFF_VALUE);

  PolicyMap::Entry a_conflict_1(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_PLATFORM,
                                base::Value(kMachineRecommended), nullptr);
  PolicyMap::Entry a_conflict_2(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                POLICY_SOURCE_PLATFORM,
                                base::Value(kUserMandatory), nullptr);
  PolicyMap::Entry a_conflict_3(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                                POLICY_SOURCE_PLATFORM,
                                base::Value(kUserRecommended), nullptr);
  expected_policy.GetMutable("a")->AddConflictingPolicy(
      std::move(a_conflict_1));
  expected_policy.GetMutable("a")->AddConflictingPolicy(
      std::move(a_conflict_2));
  expected_policy.GetMutable("a")->AddConflictingPolicy(
      std::move(a_conflict_3));

  expected_policy.Set("b", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(kUserMandatory),
                      nullptr);
  expected_policy.GetMutable("b")->AddMessage(PolicyMap::MessageType::kWarning,
                                              IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected_policy.GetMutable("b")->AddMessage(PolicyMap::MessageType::kWarning,
                                              IDS_POLICY_CONFLICT_DIFF_VALUE);

  PolicyMap::Entry b_conflict_1(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_PLATFORM,
                                base::Value(kMachineRecommended), nullptr);
  PolicyMap::Entry b_conflict_2(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                                POLICY_SOURCE_PLATFORM,
                                base::Value(kUserRecommended), nullptr);
  expected_policy.GetMutable("b")->AddConflictingPolicy(
      std::move(b_conflict_1));
  expected_policy.GetMutable("b")->AddConflictingPolicy(
      std::move(b_conflict_2));

  expected_policy.Set("c", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM, base::Value(kMachineRecommended),
                      nullptr);
  expected_policy.GetMutable("c")->AddMessage(PolicyMap::MessageType::kWarning,
                                              IDS_POLICY_CONFLICT_DIFF_VALUE);

  PolicyMap::Entry c_conflict_1(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                                POLICY_SOURCE_PLATFORM,
                                base::Value(kUserRecommended), nullptr);
  expected_policy.GetMutable("c")->AddConflictingPolicy(
      std::move(c_conflict_1));

  expected_policy.Set("d", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM, base::Value(kUserRecommended),
                      nullptr);
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, LoadStringEncodedValues) {
  // Create a dictionary with all the types that can be stored encoded in a
  // string.
  const PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, "string");
  ASSERT_TRUE(RegisterSchema(ns,
                             R"({
        "type": "object",
        "id": "MainType",
        "properties": {
          "bool": { "type": "boolean" },
          "int": { "type": "integer" },
          "double": { "type": "number" },
          "list": {
            "type": "array",
            "items": { "$ref": "MainType" }
          },
          "dict": { "$ref": "MainType" }
        }
      })"));

  base::Value::Dict policy;
  policy.Set("bool", true);
  policy.Set("int", -123);
  policy.Set("double", 456.78e9);
  base::Value::List list;
  list.Append(policy.Clone());
  list.Append(policy.Clone());
  policy.Set("list", list.Clone());
  // Encode |policy| before adding the "dict" entry.
  std::string encoded_dict;
  base::JSONWriter::Write(policy, &encoded_dict);
  ASSERT_FALSE(encoded_dict.empty());
  policy.Set("dict", policy.Clone());
  std::string encoded_list;
  base::JSONWriter::Write(list, &encoded_list);
  ASSERT_FALSE(encoded_list.empty());
  base::Value::Dict encoded_policy;
  encoded_policy.Set("bool", "1");
  encoded_policy.Set("int", "-123");
  encoded_policy.Set("double", "456.78e9");
  encoded_policy.Set("list", encoded_list);
  encoded_policy.Set("dict", encoded_dict);

  const std::wstring kPathSuffix =
      kTestPolicyKey + std::wstring(L"\\3rdparty\\extensions\\string");
  EXPECT_TRUE(InstallValue(base::Value(encoded_policy.Clone()),
                           HKEY_CURRENT_USER, kPathSuffix, kMandatory));

  PolicyBundle expected;
  expected.Get(ns).LoadFrom(policy.Clone(), POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM);
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, LoadIntegerEncodedValues) {
  const PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, "int");
  ASSERT_TRUE(RegisterSchema(
      ns,
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"bool\": { \"type\": \"boolean\" },"
      "    \"int\": { \"type\": \"integer\" },"
      "    \"double\": { \"type\": \"number\" }"
      "  }"
      "}"));

  base::Value::Dict encoded_policy;
  encoded_policy.Set("bool", 1);
  encoded_policy.Set("int", 123);
  encoded_policy.Set("double", 456);

  const std::wstring kPathSuffix =
      kTestPolicyKey + std::wstring(L"\\3rdparty\\extensions\\int");
  EXPECT_TRUE(InstallValue(base::Value(encoded_policy.Clone()),
                           HKEY_CURRENT_USER, kPathSuffix, kMandatory));

  base::Value::Dict policy;
  policy.Set("bool", true);
  policy.Set("int", 123);
  policy.Set("double", 456.0);
  PolicyBundle expected;
  expected.Get(ns).LoadFrom(policy.Clone(), POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM);
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, DefaultPropertySchemaType) {
  // Build a schema for an "object" with a default schema for its properties.
  // Note that the top-level object can't have "additionalProperties", so
  // a "policy" property is used instead.
  const PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, "test");
  ASSERT_TRUE(RegisterSchema(
      ns,
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"policy\": {"
      "      \"type\": \"object\","
      "      \"properties\": {"
      "        \"special-int1\": { \"type\": \"integer\" },"
      "        \"special-int2\": { \"type\": \"integer\" }"
      "      },"
      "      \"additionalProperties\": { \"type\": \"number\" }"
      "    }"
      "  }"
      "}"));

  // Write some test values.
  base::Value::Dict policy;
  // These special values have a specific schema for them.
  policy.Set("special-int1", 123);
  policy.Set("special-int2", "-456");
  // Other values default to be loaded as doubles.
  policy.Set("double1", 789.0);
  policy.Set("double2", "123.456e7");
  policy.Set("invalid", "omg");
  base::Value::Dict all_policies;
  all_policies.Set("policy", policy.Clone());

  const std::wstring kPathSuffix =
      kTestPolicyKey + std::wstring(L"\\3rdparty\\extensions\\test");
  EXPECT_TRUE(InstallValue(base::Value(all_policies.Clone()), HKEY_CURRENT_USER,
                           kPathSuffix, kMandatory));

  base::Value::Dict expected_policy;
  expected_policy.Set("special-int1", 123);
  expected_policy.Set("special-int2", -456);
  expected_policy.Set("double1", 789.0);
  expected_policy.Set("double2", 123.456e7);
  base::Value::Dict expected_policies;
  expected_policies.Set("policy", expected_policy.Clone());
  PolicyBundle expected;
  expected.Get(ns).LoadFrom(expected_policies.Clone(), POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM);
  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, AlternativePropertySchemaType) {
  const char kTestSchema[] =
      "{"
      "  \"type\": \"object\","
      "  \"properties\": {"
      "    \"policy 1\": { \"type\": \"integer\" },"
      "    \"policy 2\": { \"type\": \"integer\" }"
      "  }"
      "}";
  // Register two namespaces. One will be completely populated with all defined
  // properties and the second will be only partially populated.
  const PolicyNamespace ns_a(
      POLICY_DOMAIN_EXTENSIONS, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  const PolicyNamespace ns_b(
      POLICY_DOMAIN_EXTENSIONS, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  ASSERT_TRUE(RegisterSchema(ns_a, kTestSchema));
  ASSERT_TRUE(RegisterSchema(ns_b, kTestSchema));

  PolicyBundle expected;
  base::Value::Dict expected_a;
  expected_a.Set("policy 1", 3);
  expected_a.Set("policy 2", 3);
  expected.Get(ns_a).LoadFrom(expected_a.Clone(), POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM);
  base::Value::Dict expected_b;
  expected_b.Set("policy 1", 2);
  expected.Get(ns_b).LoadFrom(expected_b.Clone(), POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM);

  const std::wstring kPathSuffix =
      kTestPolicyKey +
      std::wstring(L"\\3rdparty\\extensions\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  EXPECT_TRUE(InstallValue(base::Value(expected_a.Clone()), HKEY_LOCAL_MACHINE,
                           kPathSuffix, kMandatory));
  const std::wstring kPathSuffix2 =
      kTestPolicyKey +
      std::wstring(L"\\3rdparty\\extensions\\bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  EXPECT_TRUE(InstallValue(base::Value(expected_b.Clone()), HKEY_LOCAL_MACHINE,
                           kPathSuffix2, kMandatory));

  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, LoadPrecedencePolicies) {
  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  RegisterChromeSchema(chrome_ns);

  // Merging of precedence policies is handled separately from all remaining
  // policies. This ensures that all precedence policies are correctly loaded
  // from the registry.
  RegKey hklm_key(HKEY_LOCAL_MACHINE, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hklm_key.Valid());
  PolicyBundle expected;

  hklm_key.WriteValue(
      base::UTF8ToWide(key::kCloudPolicyOverridesPlatformPolicy).c_str(),
      /*in_value=*/1);
  hklm_key.WriteValue(
      base::UTF8ToWide(key::kCloudUserPolicyOverridesCloudMachinePolicy)
          .c_str(),
      /*in_value=*/1);

  expected.Get(chrome_ns).Set(
      key::kCloudPolicyOverridesPlatformPolicy, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  expected.Get(chrome_ns).Set(
      key::kCloudUserPolicyOverridesCloudMachinePolicy, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);

  EXPECT_TRUE(Matches(expected));
}

TEST_F(PolicyLoaderWinTest, LoadExpandSzPolicies) {
  constexpr char kTestEnvVar[] = "TEST_ENV_VAR";
  constexpr char kTestEnvVarValue[] = "TEST_VALUE";
  base::ScopedEnvironmentVariableOverride scoped_env(kTestEnvVar,
                                                     kTestEnvVarValue);

  RegKey hklm_key(HKEY_LOCAL_MACHINE, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hklm_key.Valid());
  auto reg_value = base::UTF8ToWide(std::string("%") + kTestEnvVar + "%");
  hklm_key.WriteValue(
      base::UTF8ToWide(test_keys::kKeyString).c_str(), reg_value.c_str(),
      static_cast<DWORD>(sizeof(reg_value[0]) * (reg_value.size() + 1)),
      REG_EXPAND_SZ);

  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
           POLICY_SOURCE_PLATFORM, base::Value(kTestEnvVarValue), nullptr);

  EXPECT_TRUE(Matches(expected));
}

// Make sure environment variables aren't expanded for REG_SZ.
TEST_F(PolicyLoaderWinTest, LoadSzPoliciesWithEnvVar) {
  constexpr char kTestEnvVar[] = "TEST_ENV_VAR";
  constexpr char kTestEnvVarValue[] = "TEST_VALUE";
  base::ScopedEnvironmentVariableOverride scoped_env(kTestEnvVar,
                                                     kTestEnvVarValue);

  RegKey hklm_key(HKEY_LOCAL_MACHINE, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hklm_key.Valid());
  auto reg_value = std::string("%") + kTestEnvVar + "%";
  hklm_key.WriteValue(base::UTF8ToWide(test_keys::kKeyString).c_str(),
                      base::UTF8ToWide(reg_value).c_str());

  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
           POLICY_SOURCE_PLATFORM, base::Value(reg_value), nullptr);

  EXPECT_TRUE(Matches(expected));
}

}  // namespace policy
