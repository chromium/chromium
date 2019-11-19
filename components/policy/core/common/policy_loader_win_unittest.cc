// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include <iterator>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_map.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;
using base::UTF16ToUTF8;
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
                  const base::string16& path,
                  const base::string16& name) {
  // KEY_ALL_ACCESS causes the ctor to create the key if it does not exist yet.
  RegKey key(hive, path.c_str(), KEY_ALL_ACCESS);
  EXPECT_TRUE(key.Valid());
  switch (value.type()) {
    case base::Value::Type::NONE:
      return key.WriteValue(name.c_str(), L"") == ERROR_SUCCESS;

    case base::Value::Type::BOOLEAN: {
      bool bool_value;
      if (!value.GetAsBoolean(&bool_value))
        return false;
      return key.WriteValue(name.c_str(), bool_value ? 1 : 0) == ERROR_SUCCESS;
    }

    case base::Value::Type::INTEGER: {
      int int_value;
      if (!value.GetAsInteger(&int_value))
        return false;
      return key.WriteValue(name.c_str(), int_value) == ERROR_SUCCESS;
    }

    case base::Value::Type::DOUBLE: {
      double double_value;
      if (!value.GetAsDouble(&double_value))
        return false;
      base::string16 str_value = base::NumberToString16(double_value);
      return key.WriteValue(name.c_str(), str_value.c_str()) == ERROR_SUCCESS;
    }

    case base::Value::Type::STRING: {
      base::string16 str_value;
      if (!value.GetAsString(&str_value))
        return false;
      return key.WriteValue(name.c_str(), str_value.c_str()) == ERROR_SUCCESS;
    }

    case base::Value::Type::DICTIONARY: {
      const base::DictionaryValue* sub_dict = nullptr;
      if (!value.GetAsDictionary(&sub_dict))
        return false;
      for (base::DictionaryValue::Iterator it(*sub_dict);
           !it.IsAtEnd(); it.Advance()) {
        if (!InstallValue(it.value(), hive, path + kPathSep + name,
                          UTF8ToUTF16(it.key()))) {
          return false;
        }
      }
      return true;
    }

    case base::Value::Type::LIST: {
      const base::ListValue* list = nullptr;
      if (!value.GetAsList(&list))
        return false;
      for (size_t i = 0; i < list->GetSize(); ++i) {
        const base::Value* item;
        if (!list->Get(i, &item))
          return false;
        if (!InstallValue(*item, hive, path + kPathSep + name,
                          base::NumberToString16(i + 1))) {
          return false;
        }
      }
      return true;
    }

    case base::Value::Type::BINARY:
      return false;

    // TODO(crbug.com/859477): Remove after root cause is found.
    case base::Value::Type::DEAD:
      CHECK(false);
      return false;
  }
  // TODO(crbug.com/859477): Revert to NOTREACHED() after root cause is found.
  CHECK(false);
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
  ~ScopedGroupPolicyRegistrySandbox();

  // Activates the registry keys overrides. This must be called before doing any
  // writes to registry and the call should be wrapped in
  // ASSERT_NO_FATAL_FAILURE.
  void ActivateOverrides();

 private:
  void RemoveOverrides();

  // Deletes the sandbox keys.
  void DeleteKeys();

  base::string16 key_name_;

  // Keys are created for the lifetime of a test to contain
  // the sandboxed HKCU and HKLM hives, respectively.
  RegKey temp_hkcu_hive_key_;
  RegKey temp_hklm_hive_key_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGroupPolicyRegistrySandbox);
};

// A test harness that feeds policy via the Chrome GPO registry subtree.
class RegistryTestHarness : public PolicyProviderTestHarness {
 public:
  RegistryTestHarness(HKEY hive, PolicyScope scope);
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
                               const base::ListValue* policy_value) override;
  void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) override;
  void Install3rdPartyPolicy(const base::DictionaryValue* policies) override;

  // Creates a harness instance that will install policy in HKCU or HKLM,
  // respectively.
  static PolicyProviderTestHarness* CreateHKCU();
  static PolicyProviderTestHarness* CreateHKLM();

 private:
  HKEY hive_;

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;

  DISALLOW_COPY_AND_ASSIGN(RegistryTestHarness);
};

ScopedGroupPolicyRegistrySandbox::ScopedGroupPolicyRegistrySandbox() {}

ScopedGroupPolicyRegistrySandbox::~ScopedGroupPolicyRegistrySandbox() {
  RemoveOverrides();
  DeleteKeys();
}

void ScopedGroupPolicyRegistrySandbox::ActivateOverrides() {
  // Generate a unique registry key for the override for each test. This
  // makes sure that tests executing in parallel won't delete each other's
  // key, at DeleteKeys().
  key_name_ = base::ASCIIToUTF16(base::StringPrintf(
      "SOFTWARE\\chromium unittest %" CrPRIdPid, base::GetCurrentProcId()));
  std::wstring hklm_key_name = key_name_ + L"\\HKLM";
  std::wstring hkcu_key_name = key_name_ + L"\\HKCU";

  // Delete the registry test keys if they already exist (this could happen if
  // the process id got recycled and the last test running under the same
  // process id crashed ).
  DeleteKeys();

  // Create the subkeys to hold the overridden HKLM and HKCU
  // policy settings.
  temp_hklm_hive_key_.Create(HKEY_CURRENT_USER,
                             hklm_key_name.c_str(),
                             KEY_ALL_ACCESS);
  temp_hkcu_hive_key_.Create(HKEY_CURRENT_USER,
                             hkcu_key_name.c_str(),
                             KEY_ALL_ACCESS);

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

RegistryTestHarness::~RegistryTestHarness() {}

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
  std::unique_ptr<AsyncPolicyLoader> loader(
      new PolicyLoaderWin(task_runner, kTestPolicyKey));
  return new AsyncPolicyProvider(registry, std::move(loader));
}

void RegistryTestHarness::InstallEmptyPolicy() {}

void RegistryTestHarness::InstallStringPolicy(
    const std::string& policy_name,
    const std::string& policy_value) {
  RegKey key(hive_, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  ASSERT_HRESULT_SUCCEEDED(key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                                          UTF8ToUTF16(policy_value).c_str()));
}

void RegistryTestHarness::InstallIntegerPolicy(
    const std::string& policy_name,
    int policy_value) {
  RegKey key(hive_, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void RegistryTestHarness::InstallBooleanPolicy(
    const std::string& policy_name,
    bool policy_value) {
  RegKey key(hive_, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void RegistryTestHarness::InstallStringListPolicy(
    const std::string& policy_name,
    const base::ListValue* policy_value) {
  RegKey key(hive_,
             (base::string16(kTestPolicyKey) + base::ASCIIToUTF16("\\") +
              UTF8ToUTF16(policy_name)).c_str(),
             KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  int index = 1;
  for (base::ListValue::const_iterator element(policy_value->begin());
       element != policy_value->end();
       ++element) {
    std::string element_value;
    if (!element->GetAsString(&element_value))
      continue;
    std::string name(base::NumberToString(index++));
    key.WriteValue(UTF8ToUTF16(name).c_str(),
                   UTF8ToUTF16(element_value).c_str());
  }
}

void RegistryTestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  std::string json;
  base::JSONWriter::Write(*policy_value, &json);
  RegKey key(hive_, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(key.Valid());
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 UTF8ToUTF16(json).c_str());
}

void RegistryTestHarness::Install3rdPartyPolicy(
    const base::DictionaryValue* policies) {
  // The first level entries are domains, and the second level entries map
  // components to their policy.
  const base::string16 kPathPrefix =
      base::string16(kTestPolicyKey) + kPathSep + kThirdParty + kPathSep;
  for (base::DictionaryValue::Iterator domain(*policies);
       !domain.IsAtEnd(); domain.Advance()) {
    const base::DictionaryValue* components = nullptr;
    if (!domain.value().GetAsDictionary(&components)) {
      ADD_FAILURE();
      continue;
    }
    for (base::DictionaryValue::Iterator component(*components);
         !component.IsAtEnd(); component.Advance()) {
      const base::string16 path = kPathPrefix +
          UTF8ToUTF16(domain.key()) + kPathSep + UTF8ToUTF16(component.key());
      InstallValue(component.value(), hive_, path, kMandatory);
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
  static const base::char16 kTestPolicyKey[];

  PolicyLoaderWinTest() : scoped_domain_(false) {}
  ~PolicyLoaderWinTest() override {}

  void SetUp() override {
    PolicyTestBase::SetUp();

    // Activate overrides of registry keys. gtest documentation guarantees
    // that the test will not be executed if SetUp has a fatal failure. This is
    // important, see crbug.com/721691.
    ASSERT_NO_FATAL_FAILURE(registry_sandbox_.ActivateOverrides());
  }

  bool Matches(const PolicyBundle& expected) {
    PolicyLoaderWin loader(task_environment_.GetMainThreadTaskRunner(),
                           kTestPolicyKey);
    std::unique_ptr<PolicyBundle> loaded(
        loader.InitialLoad(schema_registry_.schema_map()));
    return loaded->Equals(expected);
  }

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;
  base::win::ScopedDomainStateForTesting scoped_domain_;
};

const base::char16 PolicyLoaderWinTest::kTestPolicyKey[] =
    L"SOFTWARE\\Policies\\Chromium";

TEST_F(PolicyLoaderWinTest, HKLMOverHKCU) {
  RegKey hklm_key(HKEY_LOCAL_MACHINE, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hklm_key.Valid());
  hklm_key.WriteValue(UTF8ToUTF16(test_keys::kKeyString).c_str(),
                      UTF8ToUTF16("hklm").c_str());
  RegKey hkcu_key(HKEY_CURRENT_USER, kTestPolicyKey, KEY_ALL_ACCESS);
  ASSERT_TRUE(hkcu_key.Valid());
  hkcu_key.WriteValue(UTF8ToUTF16(test_keys::kKeyString).c_str(),
                      UTF8ToUTF16("hkcu").c_str());

  PolicyBundle expected;
  expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
           POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>("hklm"),
           nullptr);
  expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .GetMutable(test_keys::kKeyString)
      ->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);

  PolicyMap::Entry conflict(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                            POLICY_SOURCE_PLATFORM,
                            std::make_unique<base::Value>("hkcu"), nullptr);
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

  const base::string16 kPathSuffix =
      kTestPolicyKey + base::ASCIIToUTF16("\\3rdparty\\extensions\\merge");

  const char kUserMandatory[] = "user-mandatory";
  const char kUserRecommended[] = "user-recommended";
  const char kMachineMandatory[] = "machine-mandatory";
  const char kMachineRecommended[] = "machine-recommended";

  base::DictionaryValue policy;
  policy.SetString("a", kMachineMandatory);
  EXPECT_TRUE(InstallValue(policy, HKEY_LOCAL_MACHINE,
                           kPathSuffix, kMandatory));
  policy.SetString("a", kUserMandatory);
  policy.SetString("b", kUserMandatory);
  EXPECT_TRUE(InstallValue(policy, HKEY_CURRENT_USER,
                           kPathSuffix, kMandatory));
  policy.SetString("a", kMachineRecommended);
  policy.SetString("b", kMachineRecommended);
  policy.SetString("c", kMachineRecommended);
  EXPECT_TRUE(InstallValue(policy, HKEY_LOCAL_MACHINE,
                           kPathSuffix, kRecommended));
  policy.SetString("a", kUserRecommended);
  policy.SetString("b", kUserRecommended);
  policy.SetString("c", kUserRecommended);
  policy.SetString("d", kUserRecommended);
  EXPECT_TRUE(InstallValue(policy, HKEY_CURRENT_USER,
                           kPathSuffix, kRecommended));

  PolicyBundle expected;
  PolicyMap& expected_policy = expected.Get(ns);
  expected_policy.Set(
      "a", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(kMachineMandatory), nullptr);
  expected_policy.GetMutable("a")->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected_policy.GetMutable("a")->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected_policy.GetMutable("a")->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);

  PolicyMap::Entry a_conflict_1(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(kMachineRecommended), nullptr);
  PolicyMap::Entry a_conflict_2(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(kUserMandatory), nullptr);
  PolicyMap::Entry a_conflict_3(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(kUserRecommended), nullptr);
  expected_policy.GetMutable("a")->AddConflictingPolicy(
      std::move(a_conflict_1));
  expected_policy.GetMutable("a")->AddConflictingPolicy(
      std::move(a_conflict_2));
  expected_policy.GetMutable("a")->AddConflictingPolicy(
      std::move(a_conflict_3));

  expected_policy.Set("b", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM,
                      std::make_unique<base::Value>(kUserMandatory), nullptr);
  expected_policy.GetMutable("b")->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  expected_policy.GetMutable("b")->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);

  PolicyMap::Entry b_conflict_1(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(kMachineRecommended), nullptr);
  PolicyMap::Entry b_conflict_2(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(kUserRecommended), nullptr);
  expected_policy.GetMutable("b")->AddConflictingPolicy(
      std::move(b_conflict_1));
  expected_policy.GetMutable("b")->AddConflictingPolicy(
      std::move(b_conflict_2));

  expected_policy.Set("c", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                      POLICY_SOURCE_PLATFORM,
                      std::make_unique<base::Value>(kMachineRecommended),
                      nullptr);
  expected_policy.GetMutable("c")->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);

  PolicyMap::Entry c_conflict_1(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(kUserRecommended), nullptr);
  expected_policy.GetMutable("c")->AddConflictingPolicy(
      std::move(c_conflict_1));

  expected_policy.Set("d", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                      POLICY_SOURCE_PLATFORM,
                      std::make_unique<base::Value>(kUserRecommended), nullptr);
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

  base::DictionaryValue policy;
  policy.SetBoolean("bool", true);
  policy.SetInteger("int", -123);
  policy.SetDouble("double", 456.78e9);
  base::ListValue list;
  list.Append(std::make_unique<base::Value>(policy.Clone()));
  list.Append(std::make_unique<base::Value>(policy.Clone()));
  policy.SetKey("list", list.Clone());
  // Encode |policy| before adding the "dict" entry.
  std::string encoded_dict;
  base::JSONWriter::Write(policy, &encoded_dict);
  ASSERT_FALSE(encoded_dict.empty());
  policy.SetKey("dict", policy.Clone());
  std::string encoded_list;
  base::JSONWriter::Write(list, &encoded_list);
  ASSERT_FALSE(encoded_list.empty());
  base::DictionaryValue encoded_policy;
  encoded_policy.SetString("bool", "1");
  encoded_policy.SetString("int", "-123");
  encoded_policy.SetString("double", "456.78e9");
  encoded_policy.SetString("list", encoded_list);
  encoded_policy.SetString("dict", encoded_dict);

  const base::string16 kPathSuffix =
      kTestPolicyKey + base::ASCIIToUTF16("\\3rdparty\\extensions\\string");
  EXPECT_TRUE(
      InstallValue(encoded_policy, HKEY_CURRENT_USER, kPathSuffix, kMandatory));

  PolicyBundle expected;
  expected.Get(ns).LoadFrom(&policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                            POLICY_SOURCE_PLATFORM);
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

  base::DictionaryValue encoded_policy;
  encoded_policy.SetInteger("bool", 1);
  encoded_policy.SetInteger("int", 123);
  encoded_policy.SetInteger("double", 456);

  const base::string16 kPathSuffix =
      kTestPolicyKey + base::ASCIIToUTF16("\\3rdparty\\extensions\\int");
  EXPECT_TRUE(
      InstallValue(encoded_policy, HKEY_CURRENT_USER, kPathSuffix, kMandatory));

  base::DictionaryValue policy;
  policy.SetBoolean("bool", true);
  policy.SetInteger("int", 123);
  policy.SetDouble("double", 456.0);
  PolicyBundle expected;
  expected.Get(ns).LoadFrom(&policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                            POLICY_SOURCE_PLATFORM);
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
  base::DictionaryValue policy;
  // These special values have a specific schema for them.
  policy.SetInteger("special-int1", 123);
  policy.SetString("special-int2", "-456");
  // Other values default to be loaded as doubles.
  policy.SetInteger("double1", 789.0);
  policy.SetString("double2", "123.456e7");
  policy.SetString("invalid", "omg");
  base::DictionaryValue all_policies;
  all_policies.SetKey("policy", policy.Clone());

  const base::string16 kPathSuffix =
      kTestPolicyKey + base::ASCIIToUTF16("\\3rdparty\\extensions\\test");
  EXPECT_TRUE(
      InstallValue(all_policies, HKEY_CURRENT_USER, kPathSuffix, kMandatory));

  base::DictionaryValue expected_policy;
  expected_policy.SetInteger("special-int1", 123);
  expected_policy.SetInteger("special-int2", -456);
  expected_policy.SetDouble("double1", 789.0);
  expected_policy.SetDouble("double2", 123.456e7);
  base::DictionaryValue expected_policies;
  expected_policies.SetKey("policy", expected_policy.Clone());
  PolicyBundle expected;
  expected.Get(ns).LoadFrom(&expected_policies, POLICY_LEVEL_MANDATORY,
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
  base::DictionaryValue expected_a;
  expected_a.SetInteger("policy 1", 3);
  expected_a.SetInteger("policy 2", 3);
  expected.Get(ns_a).LoadFrom(&expected_a, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM);
  base::DictionaryValue expected_b;
  expected_b.SetInteger("policy 1", 2);
  expected.Get(ns_b).LoadFrom(&expected_b, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM);

  const base::string16 kPathSuffix =
      kTestPolicyKey +
      base::ASCIIToUTF16(
          "\\3rdparty\\extensions\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  EXPECT_TRUE(
      InstallValue(expected_a, HKEY_LOCAL_MACHINE, kPathSuffix, kMandatory));
  const base::string16 kPathSuffix2 =
      kTestPolicyKey +
      base::ASCIIToUTF16(
          "\\3rdparty\\extensions\\bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  EXPECT_TRUE(
      InstallValue(expected_b, HKEY_LOCAL_MACHINE, kPathSuffix2, kMandatory));

  EXPECT_TRUE(Matches(expected));
}

}  // namespace policy
