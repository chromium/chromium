// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/config_dir_policy_loader.h"
#include <memory>

#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

// Subdirectory of the config dir that contains mandatory policies.
const base::FilePath::CharType kMandatoryPath[] = FILE_PATH_LITERAL("managed");
// The policy input supports trailing comma and c++ styled comments.
const char PolicyWithQuirks[] = R"({
  // Some comments here.
  "HomepageIsNewTabPage": true,
  /* Some more comments here */
})";

#if !BUILDFLAG(IS_CHROMEOS)
const char PrecedencePolicies[] = R"({
  "CloudPolicyOverridesPlatformPolicy": true,
  "CloudUserPolicyOverridesCloudMachinePolicy": false,
})";
#endif  // BUILDFLAG(IS_CHROMEOS)

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  TestHarness(const TestHarness&) = delete;
  TestHarness& operator=(const TestHarness&) = delete;
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
                               const base::Value::List& policy_value) override;
  void InstallDictionaryPolicy(const std::string& policy_name,
                               const base::Value::Dict& policy_value) override;
  void Install3rdPartyPolicy(const base::Value::Dict& policies) override;

  const base::FilePath& test_dir() { return test_dir_.GetPath(); }

  // JSON-encode a dictionary and write it to a file.
  void WriteConfigFile(const base::Value::Dict& dict,
                       const std::string& file_name);
  void WriteConfigFile(const std::string& data, const std::string& file_name);

  // Returns a unique name for a policy file. Each subsequent call returns a
  // new name that comes lexicographically after the previous one.
  std::string NextConfigFileName();

  static PolicyProviderTestHarness* Create();

 private:
  base::ScopedTempDir test_dir_;
  int next_policy_file_index_;
};

TestHarness::TestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY,
                                POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_PLATFORM),
      next_policy_file_index_(100) {}

TestHarness::~TestHarness() = default;

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
  base::Value::Dict dict;
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  base::Value::Dict dict;
  dict.Set(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  base::Value::Dict dict;
  dict.Set(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  base::Value::Dict dict;
  dict.Set(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallStringListPolicy(
    const std::string& policy_name,
    const base::Value::List& policy_value) {
  base::Value::Dict dict;
  dict.Set(policy_name, policy_value.Clone());
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::Value::Dict& policy_value) {
  base::Value::Dict dict;
  dict.Set(policy_name, policy_value.Clone());
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::Install3rdPartyPolicy(const base::Value::Dict& policies) {
  base::Value::Dict dict;
  dict.Set("3rdparty", policies.Clone());
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::WriteConfigFile(const base::Value::Dict& dict,
                                  const std::string& file_name) {
  std::string data;
  JSONStringValueSerializer serializer(&data);
  serializer.Serialize(dict);
  WriteConfigFile(data, file_name);
}

void TestHarness::WriteConfigFile(const std::string& data,
                                  const std::string& file_name) {
  const base::FilePath mandatory_dir(test_dir().Append(kMandatoryPath));
  ASSERT_TRUE(base::CreateDirectory(mandatory_dir));
  const base::FilePath file_path(mandatory_dir.AppendASCII(file_name));
  ASSERT_TRUE(base::WriteFile(file_path, data));
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
  PolicyBundle bundle = loader.Load();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(bundle.Equals(kEmptyBundle));
}

// Reading from a non-existent directory should result in an empty preferences
// dictionary.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsNonExistentDirectory) {
  base::FilePath non_existent_dir(
      harness_.test_dir().Append(FILE_PATH_LITERAL("not_there")));
  ConfigDirPolicyLoader loader(task_environment_.GetMainThreadTaskRunner(),
                               non_existent_dir, POLICY_SCOPE_MACHINE);
  PolicyBundle bundle = loader.Load();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(bundle.Equals(kEmptyBundle));
}

TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsWithComments) {
  harness_.WriteConfigFile(PolicyWithQuirks, "policies.json");
  ConfigDirPolicyLoader loader(task_environment_.GetMainThreadTaskRunner(),
                               harness_.test_dir(), POLICY_SCOPE_MACHINE);
  PolicyBundle bundle = loader.Load();
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(key::kHomepageIsNewTabPage, POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM, base::Value(true),
           /*external_data_fetcher=*/nullptr);

  EXPECT_TRUE(bundle.Equals(expected_bundle));
}

// Test merging values from different files.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsMergePrefs) {
  // Write a bunch of data files in order to increase the chance to detect the
  // provider not respecting lexicographic ordering when reading them. Since the
  // filesystem may return files in arbitrary order, there is no way to be sure,
  // but this is better than nothing.
  base::Value::Dict test_dict_bar;
  const char kHomepageLocation[] = "HomepageLocation";
  test_dict_bar.Set(kHomepageLocation, "http://bar.com");
  for (unsigned int i = 1; i <= 4; ++i)
    harness_.WriteConfigFile(test_dict_bar, base::NumberToString(i));
  base::Value::Dict test_dict_foo;
  test_dict_foo.Set(kHomepageLocation, "http://foo.com");
  harness_.WriteConfigFile(test_dict_foo, "9");
  for (unsigned int i = 5; i <= 8; ++i)
    harness_.WriteConfigFile(test_dict_bar, base::NumberToString(i));

  ConfigDirPolicyLoader loader(task_environment_.GetMainThreadTaskRunner(),
                               harness_.test_dir(), POLICY_SCOPE_USER);
  PolicyBundle bundle = loader.Load();
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .LoadFrom(test_dict_foo, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_PLATFORM);
  for (unsigned int i = 1; i <= 8; ++i) {
    auto conflict_policy =
        expected_bundle
            .Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
            .Get(kHomepageLocation)
            ->DeepCopy();
    conflict_policy.conflicts.clear();
    conflict_policy.set_value(base::Value("http://bar.com"));
    expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .GetMutable(kHomepageLocation)
        ->AddConflictingPolicy(std::move(conflict_policy));
    expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .GetMutable(kHomepageLocation)
        ->AddMessage(PolicyMap::MessageType::kWarning,
                     IDS_POLICY_CONFLICT_DIFF_VALUE);
  }
  EXPECT_TRUE(bundle.Equals(expected_bundle));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ConfigDirPolicyLoaderTest, LoadPrecedencePolicies) {
  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, std::string());
  RegisterChromeSchema(chrome_ns);

  harness_.WriteConfigFile(PrecedencePolicies, "policies.json");
  ConfigDirPolicyLoader loader(task_environment_.GetMainThreadTaskRunner(),
                               harness_.test_dir(), POLICY_SCOPE_MACHINE);
  PolicyBundle bundle = loader.Load();
  PolicyBundle expected_bundle;
  expected_bundle.Get(chrome_ns).Set(
      key::kCloudPolicyOverridesPlatformPolicy, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM, base::Value(true),
      /*external_data_fetcher=*/nullptr);
  expected_bundle.Get(chrome_ns).Set(
      key::kCloudUserPolicyOverridesCloudMachinePolicy, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM, base::Value(false),
      /*external_data_fetcher=*/nullptr);

  EXPECT_TRUE(bundle.Equals(expected_bundle));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace policy
