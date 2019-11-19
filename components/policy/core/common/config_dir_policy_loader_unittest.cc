// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/config_dir_policy_loader.h"
#include <memory>

#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

// Subdirectory of the config dir that contains mandatory policies.
const base::FilePath::CharType kMandatoryPath[] = FILE_PATH_LITERAL("managed");

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  ~TestHarness() override;

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

  const base::FilePath& test_dir() { return test_dir_.GetPath(); }

  // JSON-encode a dictionary and write it to a file.
  void WriteConfigFile(const base::DictionaryValue& dict,
                       const std::string& file_name);

  // Returns a unique name for a policy file. Each subsequent call returns a new
  // name that comes lexicographically after the previous one.
  std::string NextConfigFileName();

  static PolicyProviderTestHarness* Create();

 private:
  base::ScopedTempDir test_dir_;
  int next_policy_file_index_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY,
                                POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_PLATFORM),
      next_policy_file_index_(100) {}

TestHarness::~TestHarness() {}

void TestHarness::SetUp() {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  std::unique_ptr<AsyncPolicyLoader> loader(
      new ConfigDirPolicyLoader(task_runner, test_dir(), POLICY_SCOPE_MACHINE));
  return new AsyncPolicyProvider(registry, std::move(loader));
}

void TestHarness::InstallEmptyPolicy() {
  base::DictionaryValue dict;
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  base::DictionaryValue dict;
  dict.SetString(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  base::DictionaryValue dict;
  dict.SetInteger(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  base::DictionaryValue dict;
  dict.SetBoolean(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue* policy_value) {
  base::DictionaryValue dict;
  dict.Set(policy_name, std::make_unique<base::Value>(policy_value->Clone()));
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  base::DictionaryValue dict;
  dict.Set(policy_name, std::make_unique<base::Value>(policy_value->Clone()));
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::Install3rdPartyPolicy(const base::DictionaryValue* policies) {
  base::DictionaryValue dict;
  dict.SetKey("3rdparty", policies->Clone());
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::WriteConfigFile(const base::DictionaryValue& dict,
                                  const std::string& file_name) {
  std::string data;
  JSONStringValueSerializer serializer(&data);
  serializer.Serialize(dict);
  const base::FilePath mandatory_dir(test_dir().Append(kMandatoryPath));
  ASSERT_TRUE(base::CreateDirectory(mandatory_dir));
  const base::FilePath file_path(mandatory_dir.AppendASCII(file_name));
  ASSERT_EQ((int) data.size(),
            base::WriteFile(file_path, data.c_str(), data.size()));
}

std::string TestHarness::NextConfigFileName() {
  EXPECT_LE(next_policy_file_index_, 999);
  return std::string("policy") +
         base::NumberToString(next_policy_file_index_++);
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_SUITE_P(ConfigDirPolicyLoaderTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(TestHarness::Create));

// Instantiate abstract test case for 3rd party policy reading tests.
INSTANTIATE_TEST_SUITE_P(ConfigDir3rdPartyPolicyLoaderTest,
                         Configuration3rdPartyPolicyProviderTest,
                         testing::Values(TestHarness::Create));

// Some tests that exercise special functionality in ConfigDirPolicyLoader.
class ConfigDirPolicyLoaderTest : public PolicyTestBase {
 protected:
  void SetUp() override {
    PolicyTestBase::SetUp();
    harness_.SetUp();
  }

  TestHarness harness_;
};

// The preferences dictionary is expected to be empty when there are no files to
// load.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsEmpty) {
  ConfigDirPolicyLoader loader(task_environment_.GetMainThreadTaskRunner(),
                               harness_.test_dir(), POLICY_SCOPE_MACHINE);
  std::unique_ptr<PolicyBundle> bundle(loader.Load());
  ASSERT_TRUE(bundle.get());
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(bundle->Equals(kEmptyBundle));
}

// Reading from a non-existent directory should result in an empty preferences
// dictionary.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsNonExistentDirectory) {
  base::FilePath non_existent_dir(
      harness_.test_dir().Append(FILE_PATH_LITERAL("not_there")));
  ConfigDirPolicyLoader loader(task_environment_.GetMainThreadTaskRunner(),
                               non_existent_dir, POLICY_SCOPE_MACHINE);
  std::unique_ptr<PolicyBundle> bundle(loader.Load());
  ASSERT_TRUE(bundle.get());
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(bundle->Equals(kEmptyBundle));
}

// Test merging values from different files.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsMergePrefs) {
  // Write a bunch of data files in order to increase the chance to detect the
  // provider not respecting lexicographic ordering when reading them. Since the
  // filesystem may return files in arbitrary order, there is no way to be sure,
  // but this is better than nothing.
  base::DictionaryValue test_dict_bar;
  const char kHomepageLocation[] = "HomepageLocation";
  test_dict_bar.SetString(kHomepageLocation, "http://bar.com");
  for (unsigned int i = 1; i <= 4; ++i)
    harness_.WriteConfigFile(test_dict_bar, base::NumberToString(i));
  base::DictionaryValue test_dict_foo;
  test_dict_foo.SetString(kHomepageLocation, "http://foo.com");
  harness_.WriteConfigFile(test_dict_foo, "9");
  for (unsigned int i = 5; i <= 8; ++i)
    harness_.WriteConfigFile(test_dict_bar, base::NumberToString(i));

  ConfigDirPolicyLoader loader(task_environment_.GetMainThreadTaskRunner(),
                               harness_.test_dir(), POLICY_SCOPE_USER);
  std::unique_ptr<PolicyBundle> bundle(loader.Load());
  ASSERT_TRUE(bundle.get());
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .LoadFrom(&test_dict_foo, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_PLATFORM);
  for (unsigned int i = 1; i <= 8; ++i) {
    auto conflict_policy =
        expected_bundle
            .Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
            .Get(kHomepageLocation)
            ->DeepCopy();
    conflict_policy.conflicts.clear();
    conflict_policy.value = std::make_unique<base::Value>("http://bar.com");
    expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .GetMutable(kHomepageLocation)
        ->AddConflictingPolicy(std::move(conflict_policy));
    expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .GetMutable(kHomepageLocation)
        ->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  }
  EXPECT_TRUE(bundle->Equals(expected_bundle));
}

}  // namespace policy
