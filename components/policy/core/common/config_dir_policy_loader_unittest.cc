// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/config_dir_policy_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include <sys/stat.h>
#include <unistd.h>
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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
                               const base::ListValue& policy_value) override;
  void InstallDictionaryPolicy(const std::string& policy_name,
                               const base::DictValue& policy_value) override;
  void Install3rdPartyPolicy(const base::DictValue& policies) override;

  const base::FilePath& test_dir() { return test_dir_.GetPath(); }

  // JSON-encode a dictionary and write it to a file.
  void WriteConfigFile(const base::DictValue& dict,
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
  std::unique_ptr<AsyncPolicyLoader> loader(new ConfigDirPolicyLoader(
      task_runner, {ConfigDirPolicyLoader::ConfigDirectory(test_dir())},
      POLICY_SCOPE_MACHINE));
  return new AsyncPolicyProvider(registry, std::move(loader));
}

void TestHarness::InstallEmptyPolicy() {
  base::DictValue dict;
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  base::DictValue dict;
  dict.Set(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  base::DictValue dict;
  dict.Set(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  base::DictValue dict;
  dict.Set(policy_name, policy_value);
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue& policy_value) {
  base::DictValue dict;
  dict.Set(policy_name, policy_value.Clone());
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::InstallDictionaryPolicy(const std::string& policy_name,
                                          const base::DictValue& policy_value) {
  base::DictValue dict;
  dict.Set(policy_name, policy_value.Clone());
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::Install3rdPartyPolicy(const base::DictValue& policies) {
  base::DictValue dict;
  dict.Set("3rdparty", policies.Clone());
  WriteConfigFile(dict, NextConfigFileName());
}

void TestHarness::WriteConfigFile(const base::DictValue& dict,
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
  ConfigDirPolicyLoader loader(
      task_environment_.GetMainThreadTaskRunner(),
      {ConfigDirPolicyLoader::ConfigDirectory(harness_.test_dir())},
      POLICY_SCOPE_MACHINE);
  PolicyBundle bundle = loader.Load();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(bundle.Equals(kEmptyBundle));
}

// Reading from a non-existent directory should result in an empty preferences
// dictionary.
TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsNonExistentDirectory) {
  base::FilePath non_existent_dir(
      harness_.test_dir().Append(FILE_PATH_LITERAL("not_there")));
  ConfigDirPolicyLoader loader(
      task_environment_.GetMainThreadTaskRunner(),
      {ConfigDirPolicyLoader::ConfigDirectory(non_existent_dir)},
      POLICY_SCOPE_MACHINE);
  PolicyBundle bundle = loader.Load();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(bundle.Equals(kEmptyBundle));
}

TEST_F(ConfigDirPolicyLoaderTest, ReadPrefsWithComments) {
  harness_.WriteConfigFile(PolicyWithQuirks, "policies.json");
  ConfigDirPolicyLoader loader(
      task_environment_.GetMainThreadTaskRunner(),
      {ConfigDirPolicyLoader::ConfigDirectory(harness_.test_dir())},
      POLICY_SCOPE_MACHINE);
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
  base::DictValue test_dict_bar;
  const char kHomepageLocation[] = "HomepageLocation";
  test_dict_bar.Set(kHomepageLocation, "http://bar.com");
  for (unsigned int i = 1; i <= 4; ++i)
    harness_.WriteConfigFile(test_dict_bar, base::NumberToString(i));
  base::DictValue test_dict_foo;
  test_dict_foo.Set(kHomepageLocation, "http://foo.com");
  harness_.WriteConfigFile(test_dict_foo, "9");
  for (unsigned int i = 5; i <= 8; ++i)
    harness_.WriteConfigFile(test_dict_bar, base::NumberToString(i));

  ConfigDirPolicyLoader loader(
      task_environment_.GetMainThreadTaskRunner(),
      {ConfigDirPolicyLoader::ConfigDirectory(harness_.test_dir())},
      POLICY_SCOPE_USER);
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
  ConfigDirPolicyLoader loader(
      task_environment_.GetMainThreadTaskRunner(),
      {ConfigDirPolicyLoader::ConfigDirectory(harness_.test_dir())},
      POLICY_SCOPE_MACHINE);
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

// Tests for loading multiple directories in decreasing order of precedence.
class ConfigDirPolicyLoaderMultipleDirsTest : public ConfigDirPolicyLoaderTest {
 protected:
  void SetUp() override {
    ConfigDirPolicyLoaderTest::SetUp();
    ASSERT_TRUE(secondary_temp_dir_.CreateUniqueTempDir());
    secondary_dir_ = secondary_temp_dir_.GetPath().AppendASCII("policies");
    ASSERT_TRUE(base::CreateDirectory(secondary_dir_));
  }

  void WriteSecondaryFile(const std::string& subdir,
                          const std::string& file_name,
                          const std::string& data) {
    const base::FilePath dir = secondary_dir_.AppendASCII(subdir);
    ASSERT_TRUE(base::CreateDirectory(dir));
    ASSERT_TRUE(base::WriteFile(dir.AppendASCII(file_name), data));
  }

  std::unique_ptr<ConfigDirPolicyLoader> CreateLoader(
      const base::FilePath& secondary,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    std::vector<ConfigDirPolicyLoader::ConfigDirectory> dirs = {
        ConfigDirPolicyLoader::ConfigDirectory(harness_.test_dir()),
        ConfigDirPolicyLoader::ConfigDirectory(secondary),
    };
    return std::make_unique<ConfigDirPolicyLoader>(
        task_runner ? std::move(task_runner)
                    : task_environment_.GetMainThreadTaskRunner(),
        std::move(dirs), POLICY_SCOPE_MACHINE);
  }

  static const PolicyMap& ChromePolicies(const PolicyBundle& bundle) {
    return bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  }

  base::ScopedTempDir secondary_temp_dir_;
  base::FilePath secondary_dir_;
};

TEST_F(ConfigDirPolicyLoaderMultipleDirsTest, PrimaryWinsConflicts) {
  harness_.WriteConfigFile(R"({"HomepageLocation": "http://primary.com"})",
                           "policy.json");
  WriteSecondaryFile("managed", "policy.json",
                     R"({"HomepageLocation": "http://secondary.com",
                         "HomepageIsNewTabPage": true})");

  PolicyBundle bundle = CreateLoader(secondary_dir_)->Load();

  const PolicyMap& policies = ChromePolicies(bundle);
  const PolicyMap::Entry* homepage = policies.Get(key::kHomepageLocation);
  ASSERT_TRUE(homepage);
  EXPECT_EQ(base::Value("http://primary.com"), *homepage->value_unsafe());
  ASSERT_EQ(1u, homepage->conflicts.size());
  EXPECT_EQ(base::Value("http://secondary.com"),
            *homepage->conflicts[0].entry().value_unsafe());
  // Policies that the primary directory doesn't set still apply.
  const PolicyMap::Entry* ntp = policies.Get(key::kHomepageIsNewTabPage);
  ASSERT_TRUE(ntp);
  EXPECT_EQ(base::Value(true), *ntp->value_unsafe());
}

TEST_F(ConfigDirPolicyLoaderMultipleDirsTest, LevelTakesPrecedenceOverSource) {
  // As between any two policy sources, a mandatory policy wins over a
  // recommended one regardless of which directory it comes from.
  const base::FilePath recommended_dir =
      harness_.test_dir().AppendASCII("recommended");
  ASSERT_TRUE(base::CreateDirectory(recommended_dir));
  ASSERT_TRUE(base::WriteFile(recommended_dir.AppendASCII("policy.json"),
                              R"({"HomepageLocation": "http://primary.com"})"));
  WriteSecondaryFile("managed", "policy.json",
                     R"({"HomepageLocation": "http://secondary.com"})");

  PolicyBundle bundle = CreateLoader(secondary_dir_)->Load();

  const PolicyMap::Entry* homepage =
      ChromePolicies(bundle).Get(key::kHomepageLocation);
  ASSERT_TRUE(homepage);
  EXPECT_EQ(base::Value("http://secondary.com"), *homepage->value_unsafe());
  EXPECT_EQ(POLICY_LEVEL_MANDATORY, homepage->level);
}

TEST_F(ConfigDirPolicyLoaderMultipleDirsTest, LastModificationTimeAcrossDirs) {
  harness_.WriteConfigFile(R"({"HomepageLocation": "http://primary.com"})",
                           "policy.json");
  WriteSecondaryFile("managed", "policy.json",
                     R"({"HomepageIsNewTabPage": true})");
  const base::FilePath secondary_file =
      secondary_dir_.AppendASCII("managed").AppendASCII("policy.json");
  const base::Time later_time = base::Time::Now() + base::Minutes(5);
  ASSERT_TRUE(base::TouchFile(secondary_file, later_time, later_time));

  std::unique_ptr<ConfigDirPolicyLoader> loader = CreateLoader(secondary_dir_);
  base::File::Info info;
  ASSERT_TRUE(base::GetFileInfo(secondary_file, &info));
  EXPECT_EQ(info.last_modified, loader->LastModificationTime());
}

TEST_F(ConfigDirPolicyLoaderMultipleDirsTest, WatchesMultipleDirs) {
  WriteSecondaryFile("managed", "policy.json",
                     R"({"HomepageIsNewTabPage": true})");
  // `base::FilePathWatcher` needs a `base::FileDescriptorWatcher` on some
  // platforms (kqueue on Mac), which ThreadPool workers provide but the main
  // thread of the default `TaskEnvironment` doesn't. Production also runs
  // `InitOnBackgroundThread()` on a ThreadPool sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  base::test::TestFuture<PolicyBundle> future;
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<ConfigDirPolicyLoader> loader) {
            loader->InitOnBackgroundThread();
            return loader->Load();
          },
          CreateLoader(secondary_dir_, task_runner)),
      future.GetCallback());

  EXPECT_TRUE(ChromePolicies(future.Take()).Get(key::kHomepageIsNewTabPage));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Tests for `ConfigDirectory::ownership_requirement`. Tests can't create
// root-owned files, so most of them treat the current user as the trusted
// owner and the temp dir as the trusted base directory.
class ConfigDirPolicyLoaderOwnershipRequirementTest
    : public ConfigDirPolicyLoaderTest {
 protected:
  void SetUp() override {
    ConfigDirPolicyLoaderTest::SetUp();
    ASSERT_TRUE(secondary_temp_dir_.CreateUniqueTempDir());
    trusted_root_ = base::MakeAbsoluteFilePath(secondary_temp_dir_.GetPath());
    ASSERT_FALSE(trusted_root_.empty());
    ASSERT_TRUE(base::SetPosixFilePermissions(trusted_root_, 0755));
    secondary_dir_ = trusted_root_.AppendASCII("policies");
    ASSERT_TRUE(base::CreateDirectory(secondary_dir_));
    ASSERT_TRUE(base::SetPosixFilePermissions(secondary_dir_, 0755));
  }

  // Writes `data` to `secondary_dir_/<subdir>/<file_name>` with permissions
  // 0644, creating the subdirectory with permissions 0755.
  void WriteSecondaryFile(const std::string& subdir,
                          const std::string& file_name,
                          const std::string& data) {
    const base::FilePath dir = secondary_dir_.AppendASCII(subdir);
    ASSERT_TRUE(base::CreateDirectory(dir));
    ASSERT_TRUE(base::SetPosixFilePermissions(dir, 0755));
    const base::FilePath file = dir.AppendASCII(file_name);
    ASSERT_TRUE(base::WriteFile(file, data));
    ASSERT_TRUE(base::SetPosixFilePermissions(file, 0644));
  }

  // Returns a loader for the primary test dir plus `secondary` owned by
  // `owner_uid` under `trusted_base_dir`.
  std::unique_ptr<ConfigDirPolicyLoader> CreateLoaderWithOwner(
      const base::FilePath& secondary,
      const base::FilePath& trusted_base_dir,
      uid_t owner_uid) {
    std::vector<ConfigDirPolicyLoader::ConfigDirectory> dirs = {
        ConfigDirPolicyLoader::ConfigDirectory(harness_.test_dir()),
        ConfigDirPolicyLoader::ConfigDirectory(
            secondary,
            ConfigDirPolicyLoader::OwnershipRequirement{
                .trusted_base_dir = trusted_base_dir,
                .trusted_uid = owner_uid,
            }),
    };
    return std::make_unique<ConfigDirPolicyLoader>(
        task_environment_.GetMainThreadTaskRunner(), std::move(dirs),
        POLICY_SCOPE_MACHINE);
  }

  std::unique_ptr<ConfigDirPolicyLoader> CreateLoader(
      const base::FilePath& secondary) {
    return CreateLoaderWithOwner(secondary, trusted_root_, geteuid());
  }

  static const PolicyMap& ChromePolicies(const PolicyBundle& bundle) {
    return bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  }

  base::ScopedTempDir secondary_temp_dir_;
  base::FilePath trusted_root_;
  base::FilePath secondary_dir_;
};

TEST_F(ConfigDirPolicyLoaderOwnershipRequirementTest,
       LoadsTrustedDirThroughSymlink) {
  WriteSecondaryFile("managed", "policy.json",
                     R"({"HomepageLocation": "http://secondary.com"})");
  WriteSecondaryFile("recommended", "policy.json",
                     R"({"HomepageIsNewTabPage": true})");
  // `$SNAP_DATA` is usually `/var/snap/<name>/current`, a symlink to the
  // current revision.
  const base::FilePath link = trusted_root_.AppendASCII("current");
  ASSERT_TRUE(base::CreateSymbolicLink(secondary_dir_, link));

  PolicyBundle bundle = CreateLoader(link)->Load();

  const PolicyMap& policies = ChromePolicies(bundle);
  const PolicyMap::Entry* homepage = policies.Get(key::kHomepageLocation);
  ASSERT_TRUE(homepage);
  EXPECT_EQ(base::Value("http://secondary.com"), *homepage->value_unsafe());
  EXPECT_EQ(POLICY_LEVEL_MANDATORY, homepage->level);
  EXPECT_EQ(POLICY_SCOPE_MACHINE, homepage->scope);
  EXPECT_EQ(POLICY_SOURCE_PLATFORM, homepage->source);
  const PolicyMap::Entry* ntp = policies.Get(key::kHomepageIsNewTabPage);
  ASSERT_TRUE(ntp);
  EXPECT_EQ(POLICY_LEVEL_RECOMMENDED, ntp->level);
}

TEST_F(ConfigDirPolicyLoaderOwnershipRequirementTest, RejectsUntrustedDir) {
  WriteSecondaryFile("managed", "policy.json",
                     R"({"HomepageIsNewTabPage": true})");

  // Wrong owner.
  EXPECT_TRUE(
      CreateLoaderWithOwner(secondary_dir_, trusted_root_, geteuid() + 1)
          ->Load()
          .Equals(PolicyBundle()));
  // Requiring root ownership up to `/` rejects a temp dir created by an
  // unprivileged user.
  EXPECT_TRUE(CreateLoaderWithOwner(secondary_dir_,
                                    base::FilePath(FILE_PATH_LITERAL("/")), 0)
                  ->Load()
                  .Equals(PolicyBundle()));
  // Group-writable policy dir.
  ASSERT_TRUE(base::SetPosixFilePermissions(secondary_dir_, 0775));
  EXPECT_TRUE(CreateLoader(secondary_dir_)->Load().Equals(PolicyBundle()));
  ASSERT_TRUE(base::SetPosixFilePermissions(secondary_dir_, 0755));
  // World-writable parent dir.
  ASSERT_TRUE(base::SetPosixFilePermissions(trusted_root_, 0777));
  EXPECT_TRUE(CreateLoader(secondary_dir_)->Load().Equals(PolicyBundle()));
}

TEST_F(ConfigDirPolicyLoaderOwnershipRequirementTest,
       SkipsUntrustedSubdirsAndFiles) {
  // A world-writable subdirectory is skipped as a whole.
  WriteSecondaryFile("managed", "policy.json",
                     R"({"HomepageIsNewTabPage": true})");
  ASSERT_TRUE(base::SetPosixFilePermissions(
      secondary_dir_.AppendASCII("managed"), 0777));
  // Trusted files in a trusted subdirectory load.
  WriteSecondaryFile("recommended", "a.json",
                     R"({"HomepageLocation": "http://secondary.com"})");
  // World-writable files are skipped.
  WriteSecondaryFile("recommended", "b.json", R"({"ShowHomeButton": true})");
  const base::FilePath recommended = secondary_dir_.AppendASCII("recommended");
  ASSERT_TRUE(
      base::SetPosixFilePermissions(recommended.AppendASCII("b.json"), 0666));
  // Symlinked files are skipped, since their target may be writable by anyone.
  const base::FilePath target = trusted_root_.AppendASCII("c.json");
  ASSERT_TRUE(base::WriteFile(target, R"({"BookmarkBarEnabled": true})"));
  ASSERT_TRUE(base::SetPosixFilePermissions(target, 0644));
  ASSERT_TRUE(
      base::CreateSymbolicLink(target, recommended.AppendASCII("c.json")));

  PolicyBundle bundle = CreateLoader(secondary_dir_)->Load();

  const PolicyMap& policies = ChromePolicies(bundle);
  EXPECT_TRUE(policies.Get(key::kHomepageLocation));
  EXPECT_FALSE(policies.Get(key::kHomepageIsNewTabPage));
  EXPECT_FALSE(policies.Get(key::kShowHomeButton));
  EXPECT_FALSE(policies.Get(key::kBookmarkBarEnabled));
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace policy
