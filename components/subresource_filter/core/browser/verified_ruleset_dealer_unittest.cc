// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

// TODO(pkalinnikov): Consider putting this to a test_support for this test file
// and SubresourceFilterRulesetDealerTest.
class TestRulesets {
 public:
  TestRulesets() = default;

  TestRulesets(const TestRulesets&) = delete;
  TestRulesets& operator=(const TestRulesets&) = delete;

  void CreateRulesets(bool many_rules = false) {
    if (many_rules) {
      ASSERT_NO_FATAL_FAILURE(
          test_ruleset_creator_.CreateRulesetToDisallowURLsWithManySuffixes(
              kTestRulesetSuffix1, kNumberOfRulesInBigRuleset,
              &test_ruleset_pair_1_));
      ASSERT_NO_FATAL_FAILURE(
          test_ruleset_creator_.CreateRulesetToDisallowURLsWithManySuffixes(
              kTestRulesetSuffix2, kNumberOfRulesInBigRuleset,
              &test_ruleset_pair_2_));
    } else {
      ASSERT_NO_FATAL_FAILURE(
          test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
              kTestRulesetSuffix1, &test_ruleset_pair_1_));
      ASSERT_NO_FATAL_FAILURE(
          test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
              kTestRulesetSuffix2, &test_ruleset_pair_2_));
    }
  }

  const testing::TestRuleset& indexed_1() const {
    return test_ruleset_pair_1_.indexed;
  }

  const testing::TestRuleset& indexed_2() const {
    return test_ruleset_pair_2_.indexed;
  }

 private:
  static constexpr const char kTestRulesetSuffix1[] = "foo";
  static constexpr const char kTestRulesetSuffix2[] = "bar";
  static constexpr int kNumberOfRulesInBigRuleset = 500;

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_1_;
  testing::TestRulesetPair test_ruleset_pair_2_;
};

constexpr const char TestRulesets::kTestRulesetSuffix1[];
constexpr const char TestRulesets::kTestRulesetSuffix2[];
constexpr int TestRulesets::kNumberOfRulesInBigRuleset;

std::vector<uint8_t> ReadRulesetContents(const MemoryMappedRuleset* ruleset) {
  return std::vector<uint8_t>(ruleset->data().begin(), ruleset->data().end());
}

std::vector<uint8_t> ReadFileContent(base::File* file) {
  CHECK(file);
  CHECK(file->IsValid());

  const int64_t file_length = file->GetLength();
  CHECK_LE(0, file_length);

  std::vector<uint8_t> file_content(static_cast<size_t>(file_length));
  CHECK(file->ReadAndCheck(0, file_content));
  return file_content;
}

}  // namespace

// Tests for VerifiedRulesetDealer. --------------------------------------------
//
// Note that VerifiedRulesetDealer uses RulesetDealer very directly to provide
// MemoryMappedRulesets. Many aspects of its work, e.g., lifetime of a
// MemoryMappedRuleset, its lazy creation, etc., are covered with tests to
// RulesetDealer, therefore these aspects are not tested here.

class SubresourceFilterVerifiedRulesetDealerTest : public ::testing::Test {
 public:
  SubresourceFilterVerifiedRulesetDealerTest() = default;

  SubresourceFilterVerifiedRulesetDealerTest(
      const SubresourceFilterVerifiedRulesetDealerTest&) = delete;
  SubresourceFilterVerifiedRulesetDealerTest& operator=(
      const SubresourceFilterVerifiedRulesetDealerTest&) = delete;

 protected:
  void SetUp() override {
    rulesets_.CreateRulesets(true /* many_rules */);
    ruleset_dealer_ =
        std::make_unique<VerifiedRulesetDealer>(kSafeBrowsingRulesetConfig);
  }

  void TearDown() override {
    // Ensure ruleset file deletion tasks have run.
    task_environment_.RunUntilIdle();
  }

  const TestRulesets& rulesets() const { return rulesets_; }
  VerifiedRulesetDealer* ruleset_dealer() { return ruleset_dealer_.get(); }

  bool has_cached_ruleset() const {
    return ruleset_dealer_->has_cached_ruleset();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestRulesets rulesets_;
  std::unique_ptr<VerifiedRulesetDealer> ruleset_dealer_;
};

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       RulesetIsMemoryMappedAndVerifiedLazily) {
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified,
            ruleset_dealer()->status());

  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_TRUE(ref_to_ruleset);
  EXPECT_TRUE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kIntact, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       CorruptedRulesetIsNeitherProvidedNorCached) {
  testing::TestRuleset::CorruptByTruncating(rulesets().indexed_1(), 123);

  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified,
            ruleset_dealer()->status());

  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest, MmapFailureInitial) {
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));

  MemoryMappedRuleset::SetMemoryMapFailuresForTesting(true);
  EXPECT_FALSE(ruleset_dealer()->GetRuleset());
  MemoryMappedRuleset::SetMemoryMapFailuresForTesting(false);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kInvalidFile,
            ruleset_dealer()->status());
}

// This is a duplicated test from RulesetDealer, to ensure that verification
// doesn't introduce any bad assumptions about mmap failures.
TEST_F(SubresourceFilterVerifiedRulesetDealerTest, MmapFailureSubsequent) {
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));
  {
    scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
        ruleset_dealer()->GetRuleset();
    EXPECT_TRUE(ref_to_ruleset);

    // Simulate subsequent mmap failures
    MemoryMappedRuleset::SetMemoryMapFailuresForTesting(true);

    // Calls to GetRuleset should succeed as long as the strong ref
    // is still around.
    EXPECT_TRUE(ruleset_dealer()->has_cached_ruleset());
    EXPECT_TRUE(ruleset_dealer()->GetRuleset());
    EXPECT_EQ(RulesetVerificationStatus::kIntact, ruleset_dealer()->status());
  }
  EXPECT_FALSE(ruleset_dealer()->has_cached_ruleset());
  EXPECT_FALSE(ruleset_dealer()->GetRuleset());
  EXPECT_EQ(RulesetVerificationStatus::kIntact, ruleset_dealer()->status());
  MemoryMappedRuleset::SetMemoryMapFailuresForTesting(false);
  EXPECT_TRUE(ruleset_dealer()->GetRuleset());
  EXPECT_EQ(RulesetVerificationStatus::kIntact, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       TruncatingFileMakesRulesetInvalid) {
  testing::TestRuleset::CorruptByTruncating(rulesets().indexed_1(), 4096);
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));
  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       FillingRangeMakesRulesetInvalid) {
  testing::TestRuleset::CorruptByFilling(rulesets().indexed_1(),
                                         2501 /* from */, 4000 /* to */,
                                         255 /* fill_with */);
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));
  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       RulesetIsVerifiedAfterUpdate) {
  testing::TestRuleset::CorruptByTruncating(rulesets().indexed_1(), 123);

  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_1()));

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified,
            ruleset_dealer()->status());

  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, ruleset_dealer()->status());
  ruleset_dealer()->SetRulesetFile(
      testing::TestRuleset::Open(rulesets().indexed_2()));
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified,
            ruleset_dealer()->status());
  ref_to_ruleset = ruleset_dealer()->GetRuleset();

  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_TRUE(ref_to_ruleset);
  EXPECT_TRUE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kIntact, ruleset_dealer()->status());
}

// Check that without the checksum parameter to OpenAndSetRulesetFile,
// the corrupted file is seen as valid.
TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       OpenAndSetRulesetFileValidNoChecksum) {
  // See also SubresourceFilterBrowserTest.InvalidRuleset_Checksum, corrupting
  // in this manner doesn't invalidate the Flatbuffer Verifier check.
  testing::TestRuleset::CorruptByFilling(rulesets().indexed_1(), 28246, 28247,
                                         32);
  RulesetFilePtr file = ruleset_dealer()->OpenAndSetRulesetFile(
      /*expected_checksum=*/0, rulesets().indexed_1().path);

  // Check the required file is opened.
  ASSERT_TRUE(file->IsValid());

  // Check |OpenAndSetRulesetFile| forwards call to |SetRulesetFile| on success.
  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified,
            ruleset_dealer()->status());

  // Check that after getting the ruleset the expected values are present.
  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();
  EXPECT_TRUE(ref_to_ruleset);
  EXPECT_EQ(RulesetVerificationStatus::kIntact, ruleset_dealer()->status());
}

// Check that when adding the checksum parameter to OpenAndSetRulesetFile,
// the corrupted file is detected as invalid.
TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       OpenAndSetRulesetFileInvalidChecksum) {
  int expected_checksum = base::PersistentHash(rulesets().indexed_1().contents);
  // See also SubresourceFilterBrowserTest.InvalidRuleset_Checksum, corrupting
  // in this manner doesn't invalidate the Flatbuffer Verifier check.
  testing::TestRuleset::CorruptByFilling(rulesets().indexed_1(), 28246, 28247,
                                         32);
  RulesetFilePtr file = ruleset_dealer()->OpenAndSetRulesetFile(
      expected_checksum, rulesets().indexed_1().path);

  // Check the required file is opened.
  ASSERT_TRUE(file->IsValid());

  // Check |OpenAndSetRulesetFile| forwards call to |SetRulesetFile| on success.
  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified,
            ruleset_dealer()->status());

  // Check that after getting the ruleset the expected values are present.
  scoped_refptr<const MemoryMappedRuleset> ref_to_ruleset =
      ruleset_dealer()->GetRuleset();
  EXPECT_FALSE(ref_to_ruleset);
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       OpenAndSetRulesetFileReturnsCorrectFileOnSuccess) {
  RulesetFilePtr file = ruleset_dealer()->OpenAndSetRulesetFile(
      /*expected_checksum=*/0, rulesets().indexed_1().path);

  // Check the required file is opened.
  ASSERT_TRUE(file->IsValid());
  EXPECT_EQ(rulesets().indexed_1().contents, ReadFileContent(file.get()));

  // Check |OpenAndSetRulesetFile| forwards call to |SetRulesetFile| on success.
  EXPECT_TRUE(ruleset_dealer()->IsRulesetFileAvailable());
  EXPECT_FALSE(has_cached_ruleset());
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified,
            ruleset_dealer()->status());
}

TEST_F(SubresourceFilterVerifiedRulesetDealerTest,
       OpenAndSetRulesetFileReturnsNullFileOnFailure) {
  RulesetFilePtr file = ruleset_dealer()->OpenAndSetRulesetFile(
      /*expected_checksum=*/0,
      base::FilePath::FromUTF8Unsafe("non_existent_file"));

  EXPECT_FALSE(file->IsValid());
  EXPECT_FALSE(ruleset_dealer()->IsRulesetFileAvailable());
}

// Tests for VerifiedRulesetDealer::Handle. ------------------------------------

namespace {

class TestVerifiedRulesetDealerClient {
 public:
  TestVerifiedRulesetDealerClient() = default;

  TestVerifiedRulesetDealerClient(const TestVerifiedRulesetDealerClient&) =
      delete;
  TestVerifiedRulesetDealerClient& operator=(
      const TestVerifiedRulesetDealerClient&) = delete;

  base::OnceCallback<void(VerifiedRulesetDealer*)> GetCallback() {
    return base::BindOnce(&TestVerifiedRulesetDealerClient::Callback,
                          base::Unretained(this));
  }

  void ExpectRulesetState(bool expected_availability,
                          RulesetVerificationStatus expected_status =
                              RulesetVerificationStatus::kNotVerified,
                          bool expected_cached = false) const {
    ASSERT_EQ(1, invocation_counter_);
    EXPECT_EQ(expected_availability, is_ruleset_file_available_);
    EXPECT_EQ(expected_cached, has_cached_ruleset_);
    EXPECT_EQ(expected_status, status_);
  }

  void ExpectRulesetContents(const std::vector<uint8_t>& expected_contents,
                             bool expected_cached = false) const {
    ExpectRulesetState(true, RulesetVerificationStatus::kIntact,
                       expected_cached);
    EXPECT_TRUE(ruleset_is_created_);
    EXPECT_EQ(expected_contents, contents_);
  }

 private:
  void Callback(VerifiedRulesetDealer* dealer) {
    ++invocation_counter_;
    ASSERT_TRUE(dealer);

    is_ruleset_file_available_ = dealer->IsRulesetFileAvailable();
    has_cached_ruleset_ = dealer->has_cached_ruleset();
    status_ = dealer->status();

    auto ruleset = dealer->GetRuleset();
    ruleset_is_created_ = !!ruleset;
    if (ruleset_is_created_) {
      contents_ = ReadRulesetContents(ruleset.get());
    }
  }

  bool is_ruleset_file_available_ = false;
  bool has_cached_ruleset_ = false;
  RulesetVerificationStatus status_ = RulesetVerificationStatus::kNotVerified;

  bool ruleset_is_created_ = false;
  std::vector<uint8_t> contents_;

  int invocation_counter_ = 0;
};

}  // namespace

class SubresourceFilterVerifiedRulesetDealerHandleTest
    : public ::testing::Test {
 public:
  SubresourceFilterVerifiedRulesetDealerHandleTest() = default;

  SubresourceFilterVerifiedRulesetDealerHandleTest(
      const SubresourceFilterVerifiedRulesetDealerHandleTest&) = delete;
  SubresourceFilterVerifiedRulesetDealerHandleTest& operator=(
      const SubresourceFilterVerifiedRulesetDealerHandleTest&) = delete;

 protected:
  void SetUp() override {
    rulesets_.CreateRulesets(false /* many_rules */);
    task_runner_ = new base::TestSimpleTaskRunner;
  }
  void TearDown() override {
    // This is needed to ensure the File created by VerifiedRulesetDealer is
    // correctly destroyed (it's destroyed on `task_runner_` through a
    // PostTask).
    task_environment_.RunUntilIdle();
    task_runner_->RunUntilIdle();
    ::testing::Test::TearDown();
  }

  const TestRulesets& rulesets() const { return rulesets_; }
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  TestRulesets rulesets_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SubresourceFilterVerifiedRulesetDealerHandleTest,
       RulesetIsMappedLazily) {
  TestVerifiedRulesetDealerClient before_set_ruleset;
  TestVerifiedRulesetDealerClient after_set_ruleset;
  TestVerifiedRulesetDealerClient after_warm_up;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle(
      new VerifiedRulesetDealer::Handle(task_runner(),
                                        kSafeBrowsingRulesetConfig));
  dealer_handle->GetDealerAsync(before_set_ruleset.GetCallback());
  dealer_handle->TryOpenAndSetRulesetFile(rulesets().indexed_1().path,
                                          /*expected_checksum=*/0,
                                          base::DoNothing());
  dealer_handle->GetDealerAsync(after_set_ruleset.GetCallback());
  dealer_handle->GetDealerAsync(after_warm_up.GetCallback());
  dealer_handle.reset(nullptr);
  task_runner()->RunUntilIdle();

  before_set_ruleset.ExpectRulesetState(false);
  after_set_ruleset.ExpectRulesetState(true);
  after_warm_up.ExpectRulesetContents(rulesets().indexed_1().contents);
}

TEST_F(SubresourceFilterVerifiedRulesetDealerHandleTest, RulesetFileIsUpdated) {
  TestVerifiedRulesetDealerClient after_set_ruleset_1;
  TestVerifiedRulesetDealerClient read_ruleset_1;
  TestVerifiedRulesetDealerClient after_set_ruleset_2;
  TestVerifiedRulesetDealerClient read_ruleset_2;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle(
      new VerifiedRulesetDealer::Handle(task_runner(),
                                        kSafeBrowsingRulesetConfig));

  dealer_handle->TryOpenAndSetRulesetFile(
      rulesets().indexed_1().path, /*expected_checksum=*/0, base::DoNothing());
  dealer_handle->GetDealerAsync(after_set_ruleset_1.GetCallback());
  dealer_handle->GetDealerAsync(read_ruleset_1.GetCallback());

  dealer_handle->TryOpenAndSetRulesetFile(
      rulesets().indexed_2().path, /*expected_checksum=*/0, base::DoNothing());
  dealer_handle->GetDealerAsync(after_set_ruleset_2.GetCallback());
  dealer_handle->GetDealerAsync(read_ruleset_2.GetCallback());

  dealer_handle.reset(nullptr);
  task_runner()->RunUntilIdle();

  after_set_ruleset_1.ExpectRulesetState(true);
  read_ruleset_1.ExpectRulesetContents(rulesets().indexed_1().contents);
  after_set_ruleset_2.ExpectRulesetState(true);
  read_ruleset_2.ExpectRulesetContents(rulesets().indexed_2().contents);
}

TEST_F(SubresourceFilterVerifiedRulesetDealerHandleTest,
       InvalidFileDoesNotReplaceTheValidOne) {
  TestVerifiedRulesetDealerClient after_set_ruleset_1;
  TestVerifiedRulesetDealerClient read_ruleset_1;
  TestVerifiedRulesetDealerClient after_set_ruleset_2;
  TestVerifiedRulesetDealerClient read_ruleset_2;

  auto dealer_handle = std::make_unique<VerifiedRulesetDealer::Handle>(
      task_runner(), kSafeBrowsingRulesetConfig);

  dealer_handle->TryOpenAndSetRulesetFile(
      rulesets().indexed_1().path, /*expected_checksum=*/0, base::DoNothing());
  dealer_handle->GetDealerAsync(after_set_ruleset_1.GetCallback());
  dealer_handle->GetDealerAsync(read_ruleset_1.GetCallback());

  dealer_handle->TryOpenAndSetRulesetFile(
      base::FilePath::FromUTF8Unsafe("non_existent_file"),
      /*expected_checksum=*/0, base::BindOnce([](RulesetFilePtr file) {
        EXPECT_FALSE(file->IsValid());
      }));
  dealer_handle->GetDealerAsync(after_set_ruleset_2.GetCallback());
  dealer_handle->GetDealerAsync(read_ruleset_2.GetCallback());
  dealer_handle.reset(nullptr);
  task_runner()->RunUntilIdle();

  after_set_ruleset_1.ExpectRulesetState(true);
  read_ruleset_1.ExpectRulesetContents(rulesets().indexed_1().contents);
  after_set_ruleset_2.ExpectRulesetState(true,
                                         RulesetVerificationStatus::kIntact);
  read_ruleset_2.ExpectRulesetContents(rulesets().indexed_1().contents);
}

// Tests for VerifiedRuleset::Handle. ------------------------------------------

namespace {

class TestVerifiedRulesetClient {
 public:
  TestVerifiedRulesetClient() = default;

  TestVerifiedRulesetClient(const TestVerifiedRulesetClient&) = delete;
  TestVerifiedRulesetClient& operator=(const TestVerifiedRulesetClient&) =
      delete;

  base::OnceCallback<void(VerifiedRuleset*)> GetCallback() {
    return base::BindOnce(&TestVerifiedRulesetClient::Callback,
                          base::Unretained(this));
  }

  void ExpectNoRuleset() const {
    ASSERT_EQ(1, invocation_counter_);
    EXPECT_FALSE(has_ruleset_);
  }

  void ExpectRulesetContents(
      const std::vector<uint8_t> expected_contents) const {
    ASSERT_EQ(1, invocation_counter_);
    EXPECT_EQ(expected_contents, contents_);
  }

 private:
  void Callback(VerifiedRuleset* ruleset) {
    ++invocation_counter_;
    ASSERT_TRUE(ruleset);
    has_ruleset_ = !!ruleset->Get();
    if (has_ruleset_) {
      contents_ = ReadRulesetContents(ruleset->Get());
    }
  }

  bool has_ruleset_ = false;
  std::vector<uint8_t> contents_;

  int invocation_counter_ = 0;
};

}  // namespace

class SubresourceFilterVerifiedRulesetHandleTest : public ::testing::Test {
 public:
  SubresourceFilterVerifiedRulesetHandleTest() = default;

  SubresourceFilterVerifiedRulesetHandleTest(
      const SubresourceFilterVerifiedRulesetHandleTest&) = delete;
  SubresourceFilterVerifiedRulesetHandleTest& operator=(
      const SubresourceFilterVerifiedRulesetHandleTest&) = delete;

 protected:
  void SetUp() override {
    rulesets_.CreateRulesets(true /* many_rules */);
    task_runner_ = new base::TestSimpleTaskRunner;
    dealer_handle_ = std::make_unique<VerifiedRulesetDealer::Handle>(
        task_runner_, kSafeBrowsingRulesetConfig);
  }

  void TearDown() override {
    dealer_handle_.reset(nullptr);
    // This is needed to ensure the File created by VerifiedRulesetDealer is
    // correctly destroyed (it's destroyed on `task_runner_` through a
    // PostTask).
    task_environment_.RunUntilIdle();
    task_runner_->RunUntilIdle();
    ::testing::Test::TearDown();
  }

  const TestRulesets& rulesets() const { return rulesets_; }
  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }

  VerifiedRulesetDealer::Handle* dealer_handle() {
    return dealer_handle_.get();
  }

  std::unique_ptr<VerifiedRuleset::Handle> CreateRulesetHandle() {
    return std::make_unique<VerifiedRuleset::Handle>(dealer_handle());
  }

 private:
  TestRulesets rulesets_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SubresourceFilterVerifiedRulesetHandleTest,
       RulesetHandleKeepsRulesetMemoryMappedAndVerified) {
  TestVerifiedRulesetDealerClient created_handle;
  TestVerifiedRulesetClient read_ruleset;
  TestVerifiedRulesetDealerClient deleted_handle;

  dealer_handle()->TryOpenAndSetRulesetFile(
      rulesets().indexed_1().path, /*expected_checksum=*/0,
      base::BindOnce(
          [](RulesetFilePtr file) { EXPECT_TRUE(file->IsValid()); }));

  auto ruleset_handle = CreateRulesetHandle();
  dealer_handle()->GetDealerAsync(created_handle.GetCallback());
  ruleset_handle->GetRulesetAsync(read_ruleset.GetCallback());
  ruleset_handle.reset(nullptr);
  dealer_handle()->GetDealerAsync(deleted_handle.GetCallback());
  task_runner()->RunUntilIdle();

  created_handle.ExpectRulesetContents(rulesets().indexed_1().contents, true);
  read_ruleset.ExpectRulesetContents(rulesets().indexed_1().contents);
  deleted_handle.ExpectRulesetState(true, RulesetVerificationStatus::kIntact);
}

TEST_F(SubresourceFilterVerifiedRulesetHandleTest,
       RulesetUnmappedOnlyAfterLastHandleIsDeleted) {
  TestVerifiedRulesetDealerClient created_handles;
  TestVerifiedRulesetClient read_ruleset_from_handle_1;
  TestVerifiedRulesetClient read_ruleset_from_handle_2;
  TestVerifiedRulesetDealerClient deleted_handle_1;
  TestVerifiedRulesetClient read_ruleset_again_from_handle_2;
  TestVerifiedRulesetDealerClient deleted_both_handles;

  dealer_handle()->TryOpenAndSetRulesetFile(
      rulesets().indexed_1().path, /*expected_checksum=*/0,
      base::BindOnce(
          [](RulesetFilePtr file) { EXPECT_TRUE(file->IsValid()); }));

  auto ruleset_handle_1 = CreateRulesetHandle();
  auto ruleset_handle_2 = CreateRulesetHandle();
  dealer_handle()->GetDealerAsync(created_handles.GetCallback());
  ruleset_handle_1->GetRulesetAsync(read_ruleset_from_handle_1.GetCallback());
  ruleset_handle_2->GetRulesetAsync(read_ruleset_from_handle_2.GetCallback());

  ruleset_handle_1.reset(nullptr);
  dealer_handle()->GetDealerAsync(deleted_handle_1.GetCallback());
  ruleset_handle_2->GetRulesetAsync(
      read_ruleset_again_from_handle_2.GetCallback());

  ruleset_handle_2.reset(nullptr);
  dealer_handle()->GetDealerAsync(deleted_both_handles.GetCallback());

  task_runner()->RunUntilIdle();

  created_handles.ExpectRulesetContents(rulesets().indexed_1().contents, true);
  read_ruleset_from_handle_1.ExpectRulesetContents(
      rulesets().indexed_1().contents);
  read_ruleset_from_handle_2.ExpectRulesetContents(
      rulesets().indexed_1().contents);
  deleted_handle_1.ExpectRulesetContents(rulesets().indexed_1().contents, true);
  read_ruleset_again_from_handle_2.ExpectRulesetContents(
      rulesets().indexed_1().contents);
  deleted_both_handles.ExpectRulesetState(true,
                                          RulesetVerificationStatus::kIntact);
}

TEST_F(SubresourceFilterVerifiedRulesetHandleTest,
       OldRulesetRemainsMappedAfterUpdateUntilHandleIsDeleted) {
  TestVerifiedRulesetDealerClient created_handle_1;
  TestVerifiedRulesetClient read_from_handle_1;
  TestVerifiedRulesetDealerClient created_handle_2_after_update;
  TestVerifiedRulesetClient read_from_handle_2;
  TestVerifiedRulesetClient read_again_from_handle_1;
  TestVerifiedRulesetClient read_from_handle_1_after_update;
  TestVerifiedRulesetClient read_from_handle_2_after_update;
  TestVerifiedRulesetDealerClient deleted_all_handles;

  dealer_handle()->TryOpenAndSetRulesetFile(
      rulesets().indexed_1().path, /*expected_checksum=*/0,
      base::BindOnce(
          [](RulesetFilePtr file) { EXPECT_TRUE(file->IsValid()); }));

  auto ruleset_handle_1 = CreateRulesetHandle();
  dealer_handle()->GetDealerAsync(created_handle_1.GetCallback());
  ruleset_handle_1->GetRulesetAsync(read_from_handle_1.GetCallback());

  dealer_handle()->TryOpenAndSetRulesetFile(
      rulesets().indexed_2().path, /*expected_checksum=*/0, base::DoNothing());
  auto ruleset_handle_2 = CreateRulesetHandle();
  dealer_handle()->GetDealerAsync(created_handle_2_after_update.GetCallback());
  ruleset_handle_2->GetRulesetAsync(read_from_handle_2.GetCallback());
  ruleset_handle_1->GetRulesetAsync(read_again_from_handle_1.GetCallback());

  ruleset_handle_1 = CreateRulesetHandle();
  ruleset_handle_1->GetRulesetAsync(
      read_from_handle_1_after_update.GetCallback());
  ruleset_handle_2->GetRulesetAsync(
      read_from_handle_2_after_update.GetCallback());

  ruleset_handle_1.reset(nullptr);
  ruleset_handle_2.reset(nullptr);
  dealer_handle()->GetDealerAsync(deleted_all_handles.GetCallback());

  task_runner()->RunUntilIdle();

  created_handle_1.ExpectRulesetContents(rulesets().indexed_1().contents, true);
  read_from_handle_1.ExpectRulesetContents(rulesets().indexed_1().contents);
  created_handle_2_after_update.ExpectRulesetContents(
      rulesets().indexed_2().contents, true);
  read_from_handle_2.ExpectRulesetContents(rulesets().indexed_2().contents);
  read_again_from_handle_1.ExpectRulesetContents(
      rulesets().indexed_1().contents);
  read_from_handle_1_after_update.ExpectRulesetContents(
      rulesets().indexed_2().contents);
  read_from_handle_2_after_update.ExpectRulesetContents(
      rulesets().indexed_2().contents);
  deleted_all_handles.ExpectRulesetState(true,
                                         RulesetVerificationStatus::kIntact);
}

TEST_F(SubresourceFilterVerifiedRulesetHandleTest,
       CorruptRulesetIsNotHandedOut) {
  TestVerifiedRulesetDealerClient created_handle;
  TestVerifiedRulesetClient read_ruleset;
  TestVerifiedRulesetDealerClient deleted_handle;

  testing::TestRuleset::CorruptByTruncating(rulesets().indexed_1(), 4096);
  dealer_handle()->TryOpenAndSetRulesetFile(
      rulesets().indexed_1().path, /*expected_checksum=*/0,
      base::BindOnce(
          [](RulesetFilePtr file) { EXPECT_TRUE(file->IsValid()); }));

  auto ruleset_handle = CreateRulesetHandle();
  dealer_handle()->GetDealerAsync(created_handle.GetCallback());
  ruleset_handle->GetRulesetAsync(read_ruleset.GetCallback());
  ruleset_handle.reset(nullptr);
  dealer_handle()->GetDealerAsync(deleted_handle.GetCallback());
  task_runner()->RunUntilIdle();

  created_handle.ExpectRulesetState(true, RulesetVerificationStatus::kCorrupt);
  read_ruleset.ExpectNoRuleset();
  deleted_handle.ExpectRulesetState(true, RulesetVerificationStatus::kCorrupt);
}

}  // namespace subresource_filter
