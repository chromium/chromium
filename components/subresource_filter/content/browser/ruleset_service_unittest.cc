// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ruleset_service.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/content/browser/ruleset_publisher.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

// Testing constants ----------------------------------------------------------

const char kTestContentVersion1[] = "1.2.3.4";
const char kTestContentVersion2[] = "1.2.3.5";
const char kTestContentVersion3[] = "1.2.3.6";

const char kTestDisallowedSuffix1[] = "foo";
const char kTestDisallowedSuffix2[] = "bar";
const char kTestDisallowedSuffix3[] = "baz";
const char kTestLicenseContents[] = "Lorem ipsum";

// Helpers --------------------------------------------------------------------

template <typename Fun>
class ScopedFunctionOverride {
 public:
  ScopedFunctionOverride(Fun* target, Fun replacement)
      : target_(target), replacement_(replacement) {
    std::swap(*target_, replacement_);
  }

  ~ScopedFunctionOverride() { std::swap(*target_, replacement_); }

 private:
  Fun* target_;
  Fun replacement_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFunctionOverride);
};

template <typename Fun>
std::unique_ptr<ScopedFunctionOverride<Fun>> OverrideFunctionForScope(
    Fun* target,
    Fun replacement) {
  return std::make_unique<ScopedFunctionOverride<Fun>>(target, replacement);
}

std::vector<uint8_t> ReadFileContentsToVector(base::File* file) {
  size_t length = base::checked_cast<size_t>(file->GetLength());
  std::vector<uint8_t> contents(length);
  static_assert(sizeof(uint8_t) == sizeof(char), "Expected char = byte.");
  file->Read(0, reinterpret_cast<char*>(contents.data()),
             base::checked_cast<int>(length));
  return contents;
}

// Mocks ----------------------------------------------------------------------

class MockRulesetPublisherImpl : public RulesetPublisher {
 public:
  explicit MockRulesetPublisherImpl(
      scoped_refptr<base::TestSimpleTaskRunner> blocking_task_runner,
      scoped_refptr<base::TestSimpleTaskRunner> best_effort_task_runner)
      : blocking_task_runner_(std::move(blocking_task_runner)),
        best_effort_task_runner_(std::move(best_effort_task_runner)) {}
  ~MockRulesetPublisherImpl() override = default;

  void TryOpenAndSetRulesetFile(
      const base::FilePath& path,
      int expected_checksum,
      base::OnceCallback<void(base::File)> callback) override {
    // Emulate |VerifiedRulesetDealer::Handle| behaviour:
    //   1. Open file on task runner.
    //   2. Reply with result on current thread runner.
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(), FROM_HERE,
        base::BindOnce(&MockRulesetPublisherImpl::OpenRulesetFile, path),
        std::move(callback));
  }

  void PublishNewRulesetVersion(base::File ruleset_data) override {
    published_rulesets_.push_back(std::move(ruleset_data));
  }

  scoped_refptr<base::SingleThreadTaskRunner> BestEffortTaskRunner() override {
    return best_effort_task_runner_;
  }

  VerifiedRulesetDealer::Handle* GetRulesetDealer() override { return nullptr; }

  void SetRulesetPublishedCallbackForTesting(
      base::OnceClosure callback) override {}

  std::vector<base::File>& published_rulesets() { return published_rulesets_; }

  void RunBestEffortUntilIdle() {
    best_effort_task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  static base::File OpenRulesetFile(base::FilePath file_path) {
    return base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                     base::File::FLAG_SHARE_DELETE);
  }

  std::vector<base::File> published_rulesets_;
  scoped_refptr<base::TestSimpleTaskRunner> blocking_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> best_effort_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockRulesetPublisherImpl);
};

bool MockFailingReplaceFile(const base::FilePath&,
                            const base::FilePath&,
                            base::File::Error* error) {
  *error = base::File::FILE_ERROR_NOT_FOUND;
  return false;
}

#if GTEST_HAS_DEATH_TEST
bool MockCrashingIndexRuleset(base::File, RulesetIndexer*) {
  LOG(FATAL) << "Synthetic crash.";
  return false;
}
#else
bool MockFailingIndexRuleset(base::File, RulesetIndexer*) {
  return false;
}
#endif

}  // namespace

// Test fixtures --------------------------------------------------------------

using testing::TestRulesetPair;
using testing::TestRulesetCreator;

class SubresourceFilteringRulesetServiceTest : public ::testing::Test {
 public:
  SubresourceFilteringRulesetServiceTest()
      : blocking_task_runner_(
            base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        background_task_runner_(
            base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        best_effort_task_runner_(
            base::MakeRefCounted<base::TestSimpleTaskRunner>()) {}

 protected:
  void SetUp() override {
    IndexedRulesetVersion::RegisterPrefs(pref_service_.registry());

    SetUpTempDir();
    ResetRulesetService();
    RunBlockingUntilIdle();

    ASSERT_NO_FATAL_FAILURE(
        ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            kTestDisallowedSuffix1, &test_ruleset_1_));
    ASSERT_NO_FATAL_FAILURE(
        ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            kTestDisallowedSuffix2, &test_ruleset_2_));
    ASSERT_NO_FATAL_FAILURE(
        ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            kTestDisallowedSuffix3, &test_ruleset_3_));
  }

  virtual void SetUpTempDir() {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  void ResetRulesetService() {
    // Note that this takes a dummy task runner as the dealer is not used as the
    // overridden functions use the blocking_task_runner_ explicitly.
    service_ = std::make_unique<RulesetService>(
        &pref_service_, background_task_runner_, base_dir(),
        blocking_task_runner_,
        std::make_unique<MockRulesetPublisherImpl>(blocking_task_runner_,
                                                   best_effort_task_runner_));
  }

  void ClearRulesetService() {
    service_.reset();
  }

  // Creates a new file with the given license |contents| at a unique temporary
  // path, which is returned in |path|.
  void CreateTestLicenseFile(const std::string& contents,
                             base::FilePath* path) {
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator()->GetUniqueTemporaryPath(path));
    ASSERT_EQ(static_cast<int>(contents.size()),
              base::WriteFile(*path, contents.data(),
                              static_cast<int>(contents.size())));
  }

  void IndexAndStoreAndPublishUpdatedRuleset(
      const TestRulesetPair& test_ruleset_pair,
      const std::string& new_content_version,
      const base::FilePath& license_path = base::FilePath()) {
    UnindexedRulesetInfo ruleset_info;
    ruleset_info.ruleset_path = test_ruleset_pair.unindexed.path;
    ruleset_info.license_path = license_path;
    ruleset_info.content_version = new_content_version;
    service()->IndexAndStoreAndPublishRulesetIfNeeded(ruleset_info);
  }

  void WaitForIndexAndStoreAndPublishUpdatedRuleset(
      const TestRulesetPair& test_ruleset_pair,
      const std::string& new_content_version,
      const base::FilePath& license_path = base::FilePath()) {
    IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_pair,
                                          new_content_version, license_path);
    // Wait for indexing on background task runner.
    RunBackgroundUntilIdle();
    // Wait for file to be opened on blocking task runner.
    RunBlockingUntilIdle();
  }

  // Mark the initialization complete and run task queues until all are empty.
  void SimulateStartupCompletedAndWaitForTasks() {
    DCHECK(mock_publisher());
    mock_publisher()->RunBestEffortUntilIdle();
    RunAllUntilIdle();
  }

  bool WriteRuleset(const TestRulesetPair& test_ruleset_pair,
                    const IndexedRulesetVersion& indexed_version,
                    const base::FilePath& license_path = base::FilePath()) {
    return RulesetService::WriteRuleset(
               GetExpectedVersionDirPath(indexed_version), license_path,
               test_ruleset_pair.indexed.contents.data(),
               test_ruleset_pair.indexed.contents.size()) ==
           RulesetService::IndexAndWriteRulesetResult::SUCCESS;
  }

  void DeleteObsoleteRulesets(const base::FilePath& indexed_ruleset_base_dir,
                              const IndexedRulesetVersion& version_to_keep) {
    return IndexedRulesetLocator::DeleteObsoleteRulesets(
        indexed_ruleset_base_dir, version_to_keep);
  }

  base::FilePath GetExpectedVersionDirPath(
      const IndexedRulesetVersion& indexed_version) const {
    return IndexedRulesetLocator::GetSubdirectoryPathForVersion(
        base_dir(), indexed_version);
  }

  base::FilePath GetExpectedRulesetDataFilePath(
      const IndexedRulesetVersion& version) const {
    return IndexedRulesetLocator::GetRulesetDataFilePath(
        GetExpectedVersionDirPath(version));
  }

  base::FilePath GetExpectedLicenseFilePath(
      const IndexedRulesetVersion& version) const {
    return IndexedRulesetLocator::GetLicenseFilePath(
        GetExpectedVersionDirPath(version));
  }

  base::FilePath GetExpectedSentinelFilePath(
      const IndexedRulesetVersion& version) const {
    return IndexedRulesetLocator::GetSentinelFilePath(
        GetExpectedVersionDirPath(version));
  }

  void RunAllUntilIdle() {
    while (best_effort_task_runner_->HasPendingTask() ||
           blocking_task_runner_->HasPendingTask() ||
           background_task_runner_->HasPendingTask()) {
      mock_publisher()->RunBestEffortUntilIdle();
      RunBlockingUntilIdle();
      RunBackgroundUntilIdle();
    }
  }

  void RunBlockingUntilIdle() {
    blocking_task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void RunBackgroundUntilIdle() {
    background_task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void RunBackgroundPendingTasksNTimes(size_t n) {
    while (n--)
      background_task_runner_->RunPendingTasks();
  }

  void AssertValidRulesetFileWithContents(
      base::File* file,
      const std::vector<uint8_t>& expected_contents) {
    ASSERT_TRUE(file->IsValid());
    ASSERT_EQ(expected_contents, ReadFileContentsToVector(file));
  }

  void AssertReadonlyRulesetFile(base::File* file) {
    const char kTest[] = "t";
    ASSERT_TRUE(file->IsValid());
    ASSERT_EQ(-1, file->Write(0, kTest, sizeof(kTest)));
  }

  base::TestSimpleTaskRunner* blocking_task_runner() const {
    return blocking_task_runner_.get();
  }

  base::TestSimpleTaskRunner* background_task_runner() const {
    return background_task_runner_.get();
  }

  PrefService* prefs() { return &pref_service_; }
  RulesetService* service() { return service_.get(); }
  MockRulesetPublisherImpl* mock_publisher() {
    return static_cast<MockRulesetPublisherImpl*>(service_->publisher_.get());
  }

  virtual base::FilePath effective_temp_dir() const {
    return scoped_temp_dir_.GetPath();
  }

  TestRulesetCreator* test_ruleset_creator() { return &ruleset_creator_; }
  const TestRulesetPair& test_ruleset_1() const { return test_ruleset_1_; }
  const TestRulesetPair& test_ruleset_2() const { return test_ruleset_2_; }
  const TestRulesetPair& test_ruleset_3() const { return test_ruleset_3_; }
  base::FilePath base_dir() const {
    return effective_temp_dir().AppendASCII("Rules").AppendASCII("Indexed");
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;

  scoped_refptr<base::TestSimpleTaskRunner> blocking_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> background_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> best_effort_task_runner_;
  TestingPrefServiceSimple pref_service_;

  TestRulesetCreator ruleset_creator_;
  TestRulesetPair test_ruleset_1_;
  TestRulesetPair test_ruleset_2_;
  TestRulesetPair test_ruleset_3_;

  std::unique_ptr<RulesetService> service_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilteringRulesetServiceTest);
};

// Specialized test fixture for death tests. It exposes the temporary directory
// used by the parent process as an environment variable that is consumed by the
// child process, so that both processes use the same temporary directory. This
// would not otherwise be the case on Windows, where children are not `forked`
// and therefore would create their own unique temp directory.
class SubresourceFilteringRulesetServiceDeathTest
    : public SubresourceFilteringRulesetServiceTest {
 public:
  SubresourceFilteringRulesetServiceDeathTest()
      : environment_(base::Environment::Create()) {}

 protected:
  void SetUpTempDir() override {
    if (environment_->HasVar(kInheritedTempDirKey)) {
      std::string value;
      ASSERT_TRUE(environment_->GetVar(kInheritedTempDirKey, &value));
      inherited_temp_dir_ = base::FilePath::FromUTF8Unsafe(value);
    } else {
      SubresourceFilteringRulesetServiceTest::SetUpTempDir();
      environment_->SetVar(kInheritedTempDirKey,
                           effective_temp_dir().AsUTF8Unsafe());
    }
  }

  void TearDown() override {
    SubresourceFilteringRulesetServiceTest::TearDown();
    if (inherited_temp_dir_.empty())
      environment_->UnSetVar(kInheritedTempDirKey);
  }

  base::FilePath effective_temp_dir() const override {
    if (!inherited_temp_dir_.empty())
      return inherited_temp_dir_;
    return SubresourceFilteringRulesetServiceTest::effective_temp_dir();
  }

 private:
  static const char kInheritedTempDirKey[];

  std::unique_ptr<base::Environment> environment_;
  base::FilePath inherited_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilteringRulesetServiceDeathTest);
};

// static
const char SubresourceFilteringRulesetServiceDeathTest::kInheritedTempDirKey[] =
    "SUBRESOURCE_FILTERING_RULESET_SERVICE_DEATH_TEST_TEMP_DIR";


TEST_F(SubresourceFilteringRulesetServiceTest, PathsAreSane) {
  IndexedRulesetVersion indexed_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());

  base::FilePath ruleset_data_path =
      GetExpectedRulesetDataFilePath(indexed_version);
  base::FilePath license_path = GetExpectedLicenseFilePath(indexed_version);
  base::FilePath sentinel_path = GetExpectedSentinelFilePath(indexed_version);

  base::FilePath version_dir = ruleset_data_path.DirName();
  EXPECT_NE(ruleset_data_path, license_path);
  EXPECT_NE(ruleset_data_path, sentinel_path);
  EXPECT_EQ(version_dir, license_path.DirName());
  EXPECT_EQ(version_dir, sentinel_path.DirName());

  EXPECT_TRUE(base_dir().IsParent(version_dir));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring,
                      base::NumberToString(indexed_version.format_version),
                      version_dir.MaybeAsASCII());
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, indexed_version.content_version,
                      version_dir.MaybeAsASCII());
}

TEST_F(SubresourceFilteringRulesetServiceTest, WriteRuleset) {
  base::FilePath original_license_path;
  ASSERT_NO_FATAL_FAILURE(
      CreateTestLicenseFile(kTestLicenseContents, &original_license_path));
  IndexedRulesetVersion indexed_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());

  ASSERT_TRUE(
      WriteRuleset(test_ruleset_1(), indexed_version, original_license_path));

  base::File indexed_ruleset_data;
  indexed_ruleset_data.Initialize(
      GetExpectedRulesetDataFilePath(indexed_version),
      base::File::FLAG_READ | base::File::FLAG_OPEN);
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &indexed_ruleset_data, test_ruleset_1().indexed.contents));
  std::string actual_license_contents;
  ASSERT_TRUE(base::ReadFileToString(
      GetExpectedLicenseFilePath(indexed_version), &actual_license_contents));
  EXPECT_EQ(kTestLicenseContents, actual_license_contents);
}

// If the unindexed ruleset is not accompanied by a LICENSE file, there should
// be no such file created next to the indexed ruleset. The lack of license can
// be indicated by |license_path| being either an empty or non-existent path.
TEST_F(SubresourceFilteringRulesetServiceTest,
       WriteRuleset_NonExistentLicensePath) {
  base::FilePath nonexistent_license_path;
  ASSERT_NO_FATAL_FAILURE(test_ruleset_creator()->GetUniqueTemporaryPath(
      &nonexistent_license_path));
  IndexedRulesetVersion indexed_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  ASSERT_TRUE(WriteRuleset(test_ruleset_1(), indexed_version,
                           nonexistent_license_path));
  EXPECT_TRUE(
      base::PathExists(GetExpectedRulesetDataFilePath(indexed_version)));
  EXPECT_FALSE(base::PathExists(GetExpectedLicenseFilePath(indexed_version)));
}

TEST_F(SubresourceFilteringRulesetServiceTest, WriteRuleset_EmptyLicensePath) {
  IndexedRulesetVersion indexed_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  ASSERT_TRUE(
      WriteRuleset(test_ruleset_1(), indexed_version, base::FilePath()));
  EXPECT_TRUE(
      base::PathExists(GetExpectedRulesetDataFilePath(indexed_version)));
  EXPECT_FALSE(base::PathExists(GetExpectedLicenseFilePath(indexed_version)));
}

TEST_F(SubresourceFilteringRulesetServiceTest, DeleteObsoleteRulesets_Noop) {
  ASSERT_FALSE(base::DirectoryExists(base_dir()));
  DeleteObsoleteRulesets(base_dir(), IndexedRulesetVersion());
  EXPECT_TRUE(base::IsDirectoryEmpty(base_dir()));
}

TEST_F(SubresourceFilteringRulesetServiceTest, DeleteObsoleteRulesets) {
  IndexedRulesetVersion legacy_format_content_version_1(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion() - 1);
  IndexedRulesetVersion legacy_format_content_version_2(
      kTestContentVersion2, IndexedRulesetVersion::CurrentFormatVersion() - 1);
  IndexedRulesetVersion current_format_content_version_1(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  IndexedRulesetVersion current_format_content_version_2(
      kTestContentVersion2, IndexedRulesetVersion::CurrentFormatVersion());
  IndexedRulesetVersion current_format_content_version_3(
      kTestContentVersion3, IndexedRulesetVersion::CurrentFormatVersion());

  WriteRuleset(test_ruleset_1(), legacy_format_content_version_1);
  WriteRuleset(test_ruleset_2(), legacy_format_content_version_2);
  base::WriteFile(GetExpectedSentinelFilePath(legacy_format_content_version_2),
                  nullptr, 0);

  WriteRuleset(test_ruleset_1(), current_format_content_version_1);
  WriteRuleset(test_ruleset_2(), current_format_content_version_2);
  WriteRuleset(test_ruleset_3(), current_format_content_version_3);
  base::WriteFile(GetExpectedSentinelFilePath(current_format_content_version_3),
                  nullptr, 0);

  DeleteObsoleteRulesets(base_dir(), current_format_content_version_2);

  EXPECT_FALSE(base::PathExists(
      GetExpectedVersionDirPath(legacy_format_content_version_1)));
  EXPECT_FALSE(base::PathExists(
      GetExpectedVersionDirPath(legacy_format_content_version_2)));
  EXPECT_FALSE(base::PathExists(
      GetExpectedVersionDirPath(current_format_content_version_1)));
  EXPECT_TRUE(base::PathExists(
      GetExpectedRulesetDataFilePath(current_format_content_version_2)));
  EXPECT_TRUE(base::PathExists(
      GetExpectedSentinelFilePath(current_format_content_version_3)));
}

TEST_F(SubresourceFilteringRulesetServiceTest, Startup_NoRulesetNotPublished) {
  RunBlockingUntilIdle();
  EXPECT_EQ(0u, mock_publisher()->published_rulesets().size());
}

// It should not normally happen that Local State indicates that a usable
// version of the ruleset had been stored, yet the file is nowhere to be found,
// but ensure some sane behavior just in case.
TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_MissingRulesetNotPublished) {
  IndexedRulesetVersion current_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  // "Forget" to write ruleset data.
  current_version.SaveToPrefs(prefs());

  ResetRulesetService();
  RunBlockingUntilIdle();
  EXPECT_EQ(0u, mock_publisher()->published_rulesets().size());
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_LegacyFormatRulesetNotPublishedButDeleted) {
  int legacy_format_version = IndexedRulesetVersion::CurrentFormatVersion() - 1;
  IndexedRulesetVersion legacy_version(kTestContentVersion1,
                                       legacy_format_version);
  ASSERT_TRUE(legacy_version.IsValid());
  legacy_version.SaveToPrefs(prefs());
  WriteRuleset(test_ruleset_1(), legacy_version);

  ASSERT_FALSE(base::IsDirectoryEmpty(base_dir()));

  ResetRulesetService();
  RunBackgroundUntilIdle();
  EXPECT_EQ(0u, mock_publisher()->published_rulesets().size());

  SimulateStartupCompletedAndWaitForTasks();

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_FALSE(stored_version.IsValid());
  EXPECT_TRUE(base::IsDirectoryEmpty(base_dir()));
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_ExistingRulesetPublishedAndNotDeleted) {
  IndexedRulesetVersion current_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  current_version.SaveToPrefs(prefs());
  WriteRuleset(test_ruleset_1(), current_version);

  ResetRulesetService();
  RunBlockingUntilIdle();

  ASSERT_EQ(1u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));

  SimulateStartupCompletedAndWaitForTasks();

  EXPECT_EQ(1u, mock_publisher()->published_rulesets().size());
  EXPECT_TRUE(
      base::PathExists(GetExpectedRulesetDataFilePath(current_version)));
}

TEST_F(SubresourceFilteringRulesetServiceTest, NewRuleset_Published) {
  SimulateStartupCompletedAndWaitForTasks();
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);

  ASSERT_EQ(1u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetWithEmptyVersion_NotPublished) {
  SimulateStartupCompletedAndWaitForTasks();
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), std::string());

  ASSERT_EQ(0u, mock_publisher()->published_rulesets().size());
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetEarly_PublishedAfterStartupCompleted) {
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);

  ASSERT_EQ(0u, mock_publisher()->published_rulesets().size());

  SimulateStartupCompletedAndWaitForTasks();

  ASSERT_EQ(1u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
}

TEST_F(SubresourceFilteringRulesetServiceTest, NewRuleset_Persisted) {
  base::HistogramTester histogram_tester;

  base::FilePath original_license_path;
  CreateTestLicenseFile(kTestLicenseContents, &original_license_path);
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1,
                                        original_license_path);

  // Indexing should be queued, and will be performed in a deferred manner,
  // after start-up, at the same time with cleaning up obsoleted rulesets. Make
  // sure it does not get immediately deleted.
  SimulateStartupCompletedAndWaitForTasks();

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion1, stored_version.content_version);
  EXPECT_EQ(IndexedRulesetVersion::CurrentFormatVersion(),
            stored_version.format_version);

  EXPECT_TRUE(base::PathExists(GetExpectedRulesetDataFilePath(stored_version)));
  EXPECT_FALSE(base::PathExists(GetExpectedSentinelFilePath(stored_version)));

  // The unindexed ruleset was accompanied by a LICENSE file, ensure it is
  // copied next to the indexed ruleset.
  std::string actual_license_contents;
  ASSERT_TRUE(base::ReadFileToString(GetExpectedLicenseFilePath(stored_version),
                                     &actual_license_contents));
  EXPECT_EQ(kTestLicenseContents, actual_license_contents);

  ResetRulesetService();
  RunBlockingUntilIdle();

  ASSERT_EQ(1u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));

  SimulateStartupCompletedAndWaitForTasks();

  EXPECT_TRUE(base::PathExists(GetExpectedRulesetDataFilePath(stored_version)));

  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.IndexRuleset.WallDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.IndexRuleset.NumUnsupportedRules", 0, 1);
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.WriteRuleset.ReplaceFileError", 0);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.WriteRuleset.Result",
      static_cast<int>(RulesetService::IndexAndWriteRulesetResult::SUCCESS), 1);
}

// Test the scenario where a faulty copy of the ruleset resides on disk, that
// is, when there is something in the directory corresponding to a ruleset
// version, but preferences do not indicate that a valid copy of that version is
// stored. The expectation is that the directory is overwritten with a correct
// contents when the same version of the ruleset is fed to the service again.
TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRuleset_OverwritesBadCopyOfSameVersionOnDisk) {
  mock_publisher()->RunBestEffortUntilIdle();
  // Emulate a bad ruleset by writing |test_ruleset_2| into the directory
  // corresponding to |test_ruleset_1| and not updating prefs. This must come
  // after SimulateStartupCompleted, otherwise it gets deleted by the clean-up
  // routines, rendering this test pointless.
  IndexedRulesetVersion same_version_but_incomplete(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  WriteRuleset(test_ruleset_2(), same_version_but_incomplete);

  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion1, stored_version.content_version);
  EXPECT_EQ(IndexedRulesetVersion::CurrentFormatVersion(),
            stored_version.format_version);
  EXPECT_FALSE(base::PathExists(GetExpectedSentinelFilePath(stored_version)));

  ASSERT_EQ(1u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRuleset_SuccessWithUnsupportedRules) {
  base::HistogramTester histogram_tester;
  mock_publisher()->RunBestEffortUntilIdle();

  // The default field values are considered unsupported.
  url_pattern_index::proto::UrlRule unfilled_rule;

  TestRulesetPair ruleset_with_unsupported_rule;
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_creator()->CreateUnindexedRulesetWithRules(
          {unfilled_rule}, &ruleset_with_unsupported_rule.unindexed));
  WaitForIndexAndStoreAndPublishUpdatedRuleset(ruleset_with_unsupported_rule,
                                               kTestContentVersion1);

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion1, stored_version.content_version);
  EXPECT_EQ(IndexedRulesetVersion::CurrentFormatVersion(),
            stored_version.format_version);
  EXPECT_FALSE(base::PathExists(GetExpectedSentinelFilePath(stored_version)));

  ASSERT_EQ(1u, mock_publisher()->published_rulesets().size());

  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.IndexRuleset.WallDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.IndexRuleset.NumUnsupportedRules", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.WriteRuleset.Result",
      static_cast<int>(RulesetService::IndexAndWriteRulesetResult::SUCCESS), 1);
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRuleset_CannotOpenUnindexedRulesetFile) {
  base::HistogramTester histogram_tester;
  mock_publisher()->RunBestEffortUntilIdle();

  UnindexedRulesetInfo ruleset_info;
  ruleset_info.ruleset_path = base::FilePath();  // Non-existent.
  ruleset_info.content_version = kTestContentVersion1;
  service()->IndexAndStoreAndPublishRulesetIfNeeded(ruleset_info);
  RunBackgroundUntilIdle();
  RunBlockingUntilIdle();

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_FALSE(stored_version.IsValid());

  // Expect no sentinel file. Although it is unlikely that we will magically
  // find the file on a subsequent attempt, failing this early is cheap.
  IndexedRulesetVersion failed_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  EXPECT_FALSE(base::PathExists(GetExpectedSentinelFilePath(failed_version)));

  ASSERT_EQ(0u, mock_publisher()->published_rulesets().size());

  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.WriteRuleset.Result",
      static_cast<int>(RulesetService::IndexAndWriteRulesetResult::
                           FAILED_OPENING_UNINDEXED_RULESET),
      1);
}

TEST_F(SubresourceFilteringRulesetServiceTest, NewRuleset_ParseFailure) {
  base::HistogramTester histogram_tester;
  mock_publisher()->RunBestEffortUntilIdle();

  const std::string kGarbage(10000, '\xff');
  ASSERT_TRUE(base::AppendToFile(test_ruleset_1().unindexed.path,
                                 kGarbage.data(),
                                 static_cast<int>(kGarbage.size())));
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_FALSE(stored_version.IsValid());

  // Expect that a sentinel file is left behind. The parse failure is most
  // likely permanent, there is no point to retrying indexing on next start-up.
  // However, as versions with sentinel files present will not be cleaned up
  // until the format version is increased, expect no ruleset file.
  IndexedRulesetVersion failed_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  EXPECT_TRUE(base::PathExists(GetExpectedSentinelFilePath(failed_version)));
  EXPECT_FALSE(
      base::PathExists(GetExpectedRulesetDataFilePath(failed_version)));

  ASSERT_EQ(0u, mock_publisher()->published_rulesets().size());

  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.WriteRuleset.Result",
      static_cast<int>(RulesetService::IndexAndWriteRulesetResult::
                           FAILED_PARSING_UNINDEXED_RULESET),
      1);
}

TEST_F(SubresourceFilteringRulesetServiceDeathTest, NewRuleset_IndexingCrash) {
  mock_publisher()->RunBestEffortUntilIdle();
#if GTEST_HAS_DEATH_TEST
  auto scoped_override(OverrideFunctionForScope(
      &RulesetService::g_index_ruleset_func, &MockCrashingIndexRuleset));
  EXPECT_DEATH(
      {
        WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                                     kTestContentVersion1);
      },
      "Synthetic crash");
#else
  auto scoped_override(OverrideFunctionForScope(
      &RulesetService::g_index_ruleset_func, &MockFailingIndexRuleset));
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);
#endif

  // Expect that a sentinel file is left behind as a warning not to attempt
  // indexing this version again, and thus to prevent crashing again.
  // However, as versions with sentinel files present will not be cleaned up
  // until the format version is increased, expect no ruleset file.
  IndexedRulesetVersion crashed_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  EXPECT_TRUE(base::PathExists(GetExpectedSentinelFilePath(crashed_version)));
  EXPECT_FALSE(
      base::PathExists(GetExpectedRulesetDataFilePath(crashed_version)));

  base::HistogramTester histogram_tester;
  ResetRulesetService();
  SimulateStartupCompletedAndWaitForTasks();

  ASSERT_EQ(0u, mock_publisher()->published_rulesets().size());

  // The subsequent indexing attempt should be aborted.
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_FALSE(stored_version.IsValid());

  ASSERT_EQ(0u, mock_publisher()->published_rulesets().size());

  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.WriteRuleset.Result",
      static_cast<int>(RulesetService::IndexAndWriteRulesetResult::
                           ABORTED_BECAUSE_SENTINEL_FILE_PRESENT),
      1);
}

TEST_F(SubresourceFilteringRulesetServiceTest, NewRuleset_WriteFailure) {
  base::HistogramTester histogram_tester;
  mock_publisher()->RunBestEffortUntilIdle();
  auto scoped_override(OverrideFunctionForScope(
      &RulesetService::g_replace_file_func, &MockFailingReplaceFile));

  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_FALSE(stored_version.IsValid());

  ASSERT_EQ(0u, mock_publisher()->published_rulesets().size());

  // Expect that the sentinel file is already gone. Write failures are quite
  // frequent and are often transient, so it is worth attempting indexing again.
  IndexedRulesetVersion failed_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  EXPECT_FALSE(base::PathExists(GetExpectedSentinelFilePath(failed_version)));

  using IndexAndWriteRulesetResult = RulesetService::IndexAndWriteRulesetResult;
  histogram_tester.ExpectTotalCount(
      "SubresourceFilter.IndexRuleset.WallDuration", 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.IndexRuleset.NumUnsupportedRules", 0, 1);
  base::File::Error expected_error = base::File::FILE_ERROR_NOT_FOUND;
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.WriteRuleset.ReplaceFileError", -expected_error, 1);
  histogram_tester.ExpectUniqueSample(
      "SubresourceFilter.WriteRuleset.Result",
      static_cast<int>(IndexAndWriteRulesetResult::FAILED_REPLACE_FILE), 1);
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwice_SecondRulesetPrevails) {
  mock_publisher()->RunBestEffortUntilIdle();
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_2(),
                                               kTestContentVersion2);

  // This verifies that the contents from the first version of the ruleset file
  // can still be read after it has been deprecated.
  ASSERT_EQ(2u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[1],
      test_ruleset_2().indexed.contents));

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion2, stored_version.content_version);
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwiceWithTheSameVersion_SecondIsIgnored) {
  mock_publisher()->RunBestEffortUntilIdle();
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);

  // For good measure, also violate the requirement that versions should
  // uniquely identify the contents.
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_2(), kTestContentVersion1);
  ASSERT_FALSE(background_task_runner()->HasPendingTask());

  ASSERT_EQ(1u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));

  IndexedRulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion1, stored_version.content_version);
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       MultipleNewRulesetsEarly_MostRecentIsPublishedAfterStartupIsComplete) {
  IndexedRulesetVersion current_version(
      kTestContentVersion1, IndexedRulesetVersion::CurrentFormatVersion());
  current_version.SaveToPrefs(prefs());
  WriteRuleset(test_ruleset_1(), current_version);

  ResetRulesetService();

  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_2(), kTestContentVersion2);
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_3(), kTestContentVersion3);
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);

  SimulateStartupCompletedAndWaitForTasks();

  // Make sure the active ruleset is test_ruleset_3.
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets().back(),
      test_ruleset_3().indexed.contents));
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetSetShortlyBeforeDestruction_NoCrashes) {
  for (size_t num_tasks_inbetween = 0; num_tasks_inbetween < 5u;
       ++num_tasks_inbetween) {
    SCOPED_TRACE(::testing::Message() << "#Tasks: " << num_tasks_inbetween);

    SimulateStartupCompletedAndWaitForTasks();

    IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                          kTestContentVersion1);
    RunBackgroundPendingTasksNTimes(num_tasks_inbetween);
    ClearRulesetService();
    RunBlockingUntilIdle();

    EXPECT_TRUE(base::DeleteFileRecursively(base_dir()));
    ResetRulesetService();
  }

  // Must pump out PostTaskWithReply tasks that are referencing the very same
  // task runner to avoid circular dependencies and leaks on shutdown.
  RunBackgroundUntilIdle();
  RunBlockingUntilIdle();
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwiceInQuickSuccession_SecondRulesetPrevails) {
  for (size_t num_tasks_inbetween = 0; num_tasks_inbetween < 5u;
       ++num_tasks_inbetween) {
    SCOPED_TRACE(::testing::Message() << "#Tasks: " << num_tasks_inbetween);

    SimulateStartupCompletedAndWaitForTasks();

    IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                          kTestContentVersion1);
    RunBackgroundPendingTasksNTimes(num_tasks_inbetween);
    WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_2(),
                                                 kTestContentVersion2);

    // Optionally permit a "hazardous" publication of either the old or new
    // version of the ruleset, but the last ruleset message must be the new one.
    ASSERT_LE(1u, mock_publisher()->published_rulesets().size());
    ASSERT_GE(2u, mock_publisher()->published_rulesets().size());
    if (mock_publisher()->published_rulesets().size() == 2) {
      base::File* file = &mock_publisher()->published_rulesets()[0];
      ASSERT_TRUE(file->IsValid());
      EXPECT_THAT(
          ReadFileContentsToVector(file),
          ::testing::AnyOf(::testing::Eq(test_ruleset_1().indexed.contents),
                           ::testing::Eq(test_ruleset_2().indexed.contents)));
    }
    ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
        &mock_publisher()->published_rulesets().back(),
        test_ruleset_2().indexed.contents));

    IndexedRulesetVersion stored_version;
    stored_version.ReadFromPrefs(prefs());
    EXPECT_EQ(kTestContentVersion2, stored_version.content_version);

    ClearRulesetService();
    RunBlockingUntilIdle();

    EXPECT_TRUE(base::DeleteFileRecursively(base_dir()));
    IndexedRulesetVersion().SaveToPrefs(prefs());
    ResetRulesetService();
  }
}

TEST_F(SubresourceFilteringRulesetServiceTest, RulesetIsReadonly) {
  mock_publisher()->RunBestEffortUntilIdle();
  WaitForIndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(),
                                               kTestContentVersion1);

  ASSERT_EQ(1u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(
      AssertReadonlyRulesetFile(&mock_publisher()->published_rulesets()[0]));
}

TEST_F(SubresourceFilteringRulesetServiceTest, ParallelOpenOfTwoFiles) {
  // Test emulates bail out situation when ruleset file opening is cheduled
  // during another file opening.
  SimulateStartupCompletedAndWaitForTasks();

  // Schedule two indexing tasks on background task runner.
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_1(), kTestContentVersion1);
  IndexAndStoreAndPublishUpdatedRuleset(test_ruleset_2(), kTestContentVersion2);
  ASSERT_EQ(2u, background_task_runner()->NumPendingTasks());

  // Run indexing. Responses are scheduled on the current thread task runner.
  background_task_runner()->RunPendingTasks();

  // Process both responses. Each one should schedule files opening on blocking
  // task runner.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, blocking_task_runner()->NumPendingTasks());

  // Run opening. Responses are scheduled on the current thread task runner.
  blocking_task_runner()->RunPendingTasks();

  // Process both respones.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, mock_publisher()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[0],
      test_ruleset_1().indexed.contents));
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_publisher()->published_rulesets()[1],
      test_ruleset_2().indexed.contents));
}

}  // namespace subresource_filter
