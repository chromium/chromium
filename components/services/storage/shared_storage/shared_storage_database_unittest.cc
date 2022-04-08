// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_options.h"
#include "components/services/storage/shared_storage/shared_storage_test_utils.h"
#include "sql/database.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

using ::testing::ElementsAre;
using OriginMatcherFunction = SharedStorageDatabase::OriginMatcherFunction;
using InitStatus = SharedStorageDatabase::InitStatus;
using SetBehavior = SharedStorageDatabase::SetBehavior;
using OperationResult = SharedStorageDatabase::OperationResult;
using GetResult = SharedStorageDatabase::GetResult;

const int kMaxEntriesPerOrigin = 5;
const int kMaxEntriesPerOriginForIteratorTest = 1000;
const int kMaxStringLength = 100;
const int kMaxBatchSizeForIteratorTest = 25;

}  // namespace

class SharedStorageDatabaseTest : public testing::Test {
 public:
  SharedStorageDatabaseTest() {
    special_storage_policy_ = base::MakeRefCounted<MockSpecialStoragePolicy>();
  }

  ~SharedStorageDatabaseTest() override = default;

  void SetUp() override {
    InitSharedStorageFeature();

    // Get a temporary directory for the test DB files.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_name_ = temp_dir_.GetPath().AppendASCII("TestSharedStorage.db");
  }

  void TearDown() override {
    db_.reset();
    EXPECT_TRUE(temp_dir_.Delete());
  }

  // Initialize a shared storage database instance from the SQL file at
  // `relative_file_path` in the "storage/" subdirectory of test data.
  std::unique_ptr<SharedStorageDatabase> LoadFromFile(
      const char* relative_file_path) {
    if (!CreateDatabaseFromSQL(file_name_, relative_file_path)) {
      ADD_FAILURE() << "Failed loading " << relative_file_path;
      return nullptr;
    }

    return std::make_unique<SharedStorageDatabase>(
        file_name_, special_storage_policy_,
        SharedStorageOptions::Create()->GetDatabaseOptions());
  }

  sql::Database* SqlDB() { return db_ ? db_->db() : nullptr; }

  virtual void InitSharedStorageFeature() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageInitTries", "1"}});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<SharedStorageDatabase> db_;
  base::SimpleTestClock clock_;
};

// Test loading version 1 database.
TEST_F(SharedStorageDatabaseTest, Version1_LoadFromFile) {
  db_ = LoadFromFile("shared_storage.v1.sql");
  ASSERT_TRUE(db_);

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  EXPECT_EQ(db_->Get(google_com, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(google_com, u"key2").data, u"value2");

  // Because the SQL database is lazy-initialized, wait to verify tables and
  // columns until after the first call to `Get()`.
  ASSERT_TRUE(SqlDB());
  VerifySharedStorageTablesAndColumns(*SqlDB());

  url::Origin youtube_com = url::Origin::Create(GURL("http://youtube.com/"));
  EXPECT_EQ(1L, db_->Length(youtube_com));

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  EXPECT_EQ(db_->Get(chromium_org, u"a").data, u"");

  TestSharedStorageEntriesListener listener(
      task_environment_.GetMainThreadTaskRunner());
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(chromium_org, listener.BindNewPipeAndPassRemote()));
  listener.Flush();
  EXPECT_THAT(listener.TakeKeys(), ElementsAre(u"a", u"b", u"c"));
  EXPECT_EQ("", listener.error_message());
  EXPECT_EQ(1U, listener.BatchCount());
  listener.VerifyNoError();

  url::Origin google_org = url::Origin::Create(GURL("http://google.org/"));
  EXPECT_EQ(
      db_->Get(google_org, u"1").data,
      u"fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "fffffffffffffffff");
  EXPECT_EQ(db_->Get(google_org,
                     u"ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "fffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                     "ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
                .data,
            u"k");

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(
      origins,
      ElementsAre(
          url::Origin::Create(GURL("http://abc.xyz")), chromium_org, google_com,
          google_org, url::Origin::Create(GURL("http://growwithgoogle.com")),
          url::Origin::Create(GURL("http://gv.com")),
          url::Origin::Create(GURL("http://waymo.com")),
          url::Origin::Create(GURL("http://withgoogle.com")), youtube_com));

  EXPECT_TRUE(db_->Destroy());
}

TEST_F(SharedStorageDatabaseTest, Version1_DestroyTooNew) {
  // Initialization should fail, since the last compatible version number
  // is too high.
  db_ = LoadFromFile("shared_storage.v1.init_too_new.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(SqlDB());

  // Call an operation so that the database will attempt to be lazy-initialized.
  const url::Origin kOrigin = url::Origin::Create(GURL("http://www.a.com"));
  EXPECT_EQ(OperationResult::kInitFailure, db_->Set(kOrigin, u"key", u"value"));
  ASSERT_FALSE(db_->IsOpenForTesting());
  EXPECT_EQ(InitStatus::kTooNew, db_->DBStatusForTesting());

  // Test that other operations likewise fail, in order to exercise these code
  // paths.
  EXPECT_EQ(OperationResult::kInitFailure, db_->Get(kOrigin, u"key").result);
  EXPECT_EQ(OperationResult::kInitFailure,
            db_->Append(kOrigin, u"key", u"value"));
  EXPECT_EQ(OperationResult::kInitFailure, db_->Delete(kOrigin, u"key"));
  EXPECT_EQ(OperationResult::kInitFailure, db_->Clear(kOrigin));
  EXPECT_EQ(-1, db_->Length(kOrigin));
  EXPECT_EQ(OperationResult::kInitFailure,
            db_->PurgeMatchingOrigins(OriginMatcherFunction(),
                                      base::Time::Min(), base::Time::Max(),
                                      /*perform_storage_cleanup=*/false));
  EXPECT_EQ(OperationResult::kInitFailure,
            db_->PurgeStaleOrigins(base::Seconds(1)));

  // Test that it is still OK to Destroy() the database.
  EXPECT_TRUE(db_->Destroy());
}

TEST_F(SharedStorageDatabaseTest, Version0_DestroyTooOld) {
  // Initialization should fail, since the current version number
  // is too low and we're forcing there not to be a retry attempt.
  db_ = LoadFromFile("shared_storage.v0.init_too_old.sql");
  ASSERT_TRUE(db_);
  ASSERT_TRUE(SqlDB());

  // Call an operation so that the database will attempt to be lazy-initialized.
  EXPECT_EQ(OperationResult::kInitFailure,
            db_->Set(url::Origin::Create(GURL("http://www.a.com")), u"key",
                     u"value"));
  ASSERT_FALSE(db_->IsOpenForTesting());
  EXPECT_EQ(InitStatus::kTooOld, db_->DBStatusForTesting());

  // Test that it is still OK to Destroy() the database.
  EXPECT_TRUE(db_->Destroy());
}

class SharedStorageDatabaseParamTest
    : public SharedStorageDatabaseTest,
      public testing::WithParamInterface<SharedStorageWrappedBool> {
 public:
  void SetUp() override {
    SharedStorageDatabaseTest::SetUp();

    auto options = SharedStorageOptions::Create()->GetDatabaseOptions();
    base::FilePath db_path =
        (GetParam().in_memory_only) ? base::FilePath() : file_name_;
    db_ = std::make_unique<SharedStorageDatabase>(
        db_path, special_storage_policy_, std::move(options));
    db_->OverrideClockForTesting(&clock_);
  }

  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageEntriesPerOrigin",
          base::NumberToString(kMaxEntriesPerOrigin)},
         {"MaxSharedStorageStringLength",
          base::NumberToString(kMaxStringLength)}});
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageDatabaseParamTest,
                         testing::ValuesIn(GetSharedStorageWrappedBools()),
                         testing::PrintToStringParamName());

TEST_P(SharedStorageDatabaseParamTest, BasicOperations) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value2"));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value2");

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key1"));
  EXPECT_FALSE(db_->Get(kOrigin1, u"key1").data);

  // Check that trying to retrieve the empty key doesn't give an error, even
  // though the input is invalid and no value is found.
  GetResult result = db_->Get(kOrigin1, u"");
  EXPECT_EQ(OperationResult::kSuccess, result.result);
  EXPECT_FALSE(result.data);

  // Check that trying to delete the empty key doesn't give an error, even
  // though the input is invalid and no value is found to delete.
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u""));
}

TEST_P(SharedStorageDatabaseParamTest, IgnoreIfPresent) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  // The database does not set a new value for "key1", but retains the
  // previously set value "value1" because `behavior` is `kIgnoreIfPresent`.
  EXPECT_EQ(OperationResult::kIgnored,
            db_->Set(kOrigin1, u"key1", u"value2",
                     /*behavior=*/SetBehavior::kIgnoreIfPresent));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value1"));
  EXPECT_EQ(db_->Get(kOrigin1, u"key2").data, u"value1");

  // Having `behavior` set to `kDefault` makes `Set()` override any previous
  // value.
  EXPECT_EQ(OperationResult::kSet,
            db_->Set(kOrigin1, u"key2", u"value2",
                     /*behavior=*/SetBehavior::kDefault));
  EXPECT_EQ(db_->Get(kOrigin1, u"key2").data, u"value2");

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));

  // If no previous value exists, it makes no difference whether
  // `behavior` is set to `kDefault` or `kIgnoreIfPresent`.
  EXPECT_EQ(OperationResult::kSet,
            db_->Set(kOrigin2, u"key1", u"value1",
                     /*behavior=*/SetBehavior::kIgnoreIfPresent));
  EXPECT_EQ(db_->Get(kOrigin2, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet,
            db_->Set(kOrigin2, u"key2", u"value2",
                     /*behavior=*/SetBehavior::kDefault));
  EXPECT_EQ(db_->Get(kOrigin2, u"key2").data, u"value2");
}

TEST_P(SharedStorageDatabaseParamTest, Append) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1value1");

  EXPECT_EQ(OperationResult::kSet, db_->Append(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1value1value1");
}

TEST_P(SharedStorageDatabaseParamTest, Length) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(0L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value3"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key1"));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
}

TEST_P(SharedStorageDatabaseParamTest, Keys) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  TestSharedStorageEntriesListenerUtility utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin1, utility.BindNewPipeAndPassRemoteForId(id1)));
  utility.FlushForId(id1);
  EXPECT_TRUE(utility.TakeKeysForId(id1).empty());
  EXPECT_EQ(1U, utility.BatchCountForId(id1));
  utility.VerifyNoErrorForId(id1);

  EXPECT_EQ(InitStatus::kUnattempted, db_->DBStatusForTesting());

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));

  size_t id2 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin1, utility.BindNewPipeAndPassRemoteForId(id2)));
  utility.FlushForId(id2);
  EXPECT_THAT(utility.TakeKeysForId(id2), ElementsAre(u"key1", u"key2"));
  EXPECT_EQ(1U, utility.BatchCountForId(id2));
  utility.VerifyNoErrorForId(id2);

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  size_t id3 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id3)));
  utility.FlushForId(id3);
  EXPECT_TRUE(utility.TakeKeysForId(id3).empty());
  EXPECT_EQ(1U, utility.BatchCountForId(id3));
  utility.VerifyNoErrorForId(id3);

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));

  size_t id4 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id4)));
  utility.FlushForId(id4);
  EXPECT_THAT(utility.TakeKeysForId(id4),
              ElementsAre(u"key1", u"key2", u"key3"));
  EXPECT_EQ(1U, utility.BatchCountForId(id4));
  utility.VerifyNoErrorForId(id4);

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key2"));

  size_t id5 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id5)));
  utility.FlushForId(id5);
  EXPECT_THAT(utility.TakeKeysForId(id5), ElementsAre(u"key1", u"key3"));
  EXPECT_EQ(1U, utility.BatchCountForId(id5));
  utility.VerifyNoErrorForId(id5);
}

TEST_P(SharedStorageDatabaseParamTest, Entries) {
  url::Origin kOrigin1 = url::Origin::Create(GURL("http://www.example1.test"));
  TestSharedStorageEntriesListenerUtility utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin1, utility.BindNewPipeAndPassRemoteForId(id1)));
  utility.FlushForId(id1);
  EXPECT_TRUE(utility.TakeEntriesForId(id1).empty());
  EXPECT_EQ(1U, utility.BatchCountForId(id1));
  utility.VerifyNoErrorForId(id1);

  EXPECT_EQ(InitStatus::kUnattempted, db_->DBStatusForTesting());

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));

  size_t id2 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin1, utility.BindNewPipeAndPassRemoteForId(id2)));
  utility.FlushForId(id2);
  EXPECT_THAT(utility.TakeEntriesForId(id2),
              ElementsAre(std::make_pair(u"key1", u"value1"),
                          std::make_pair(u"key2", u"value2")));
  EXPECT_EQ(1U, utility.BatchCountForId(id2));
  utility.VerifyNoErrorForId(id2);

  url::Origin kOrigin2 = url::Origin::Create(GURL("http://www.example2.test"));
  size_t id3 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id3)));
  utility.FlushForId(id3);
  EXPECT_TRUE(utility.TakeEntriesForId(id3).empty());
  EXPECT_EQ(1U, utility.BatchCountForId(id3));
  utility.VerifyNoErrorForId(id3);

  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));

  size_t id4 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id4)));
  utility.FlushForId(id4);
  EXPECT_THAT(utility.TakeEntriesForId(id4),
              ElementsAre(std::make_pair(u"key1", u"value1"),
                          std::make_pair(u"key2", u"value2"),
                          std::make_pair(u"key3", u"value3")));
  EXPECT_EQ(1U, utility.BatchCountForId(id4));
  utility.VerifyNoErrorForId(id4);

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key2"));

  size_t id5 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Entries(kOrigin2, utility.BindNewPipeAndPassRemoteForId(id5)));
  utility.FlushForId(id5);
  EXPECT_THAT(utility.TakeEntriesForId(id5),
              ElementsAre(std::make_pair(u"key1", u"value1"),
                          std::make_pair(u"key3", u"value3")));
  EXPECT_EQ(1U, utility.BatchCountForId(id5));
  utility.VerifyNoErrorForId(id5);
}

TEST_P(SharedStorageDatabaseParamTest, Clear) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin2));

  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(2L, db_->Length(kOrigin2));

  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin2));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
}

TEST_P(SharedStorageDatabaseParamTest, FetchOrigins) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin3));

  const url::Origin kOrigin4 =
      url::Origin::Create(GURL("http://www.example4.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin4));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key1"));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  origins.clear();
  EXPECT_TRUE(origins.empty());
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin3, kOrigin4));
}

class SharedStorageDatabasePurgeMatchingOriginsParamTest
    : public SharedStorageDatabaseTest,
      public testing::WithParamInterface<PurgeMatchingOriginsParams> {
 public:
  void SetUp() override {
    SharedStorageDatabaseTest::SetUp();

    auto options = SharedStorageOptions::Create()->GetDatabaseOptions();
    base::FilePath db_path =
        (GetParam().in_memory_only) ? base::FilePath() : file_name_;
    db_ = std::make_unique<SharedStorageDatabase>(
        db_path, special_storage_policy_, std::move(options));
    db_->OverrideClockForTesting(&clock_);
  }

  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageEntriesPerOrigin",
          base::NumberToString(kMaxEntriesPerOrigin)},
         {"MaxSharedStorageStringLength",
          base::NumberToString(kMaxStringLength)}});
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageDatabasePurgeMatchingOriginsParamTest,
                         testing::ValuesIn(GetPurgeMatchingOriginsParams()),
                         testing::PrintToStringParamName());

TEST_P(SharedStorageDatabasePurgeMatchingOriginsParamTest, AllTime) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin3));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3));

  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          OriginMatcherFunctionUtility::MakeMatcherFunction({kOrigin1}),
          base::Time(), base::Time::Max(), GetParam().perform_storage_cleanup));

  // `kOrigin1` is cleared. The other origins are not.
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(3L, db_->Length(kOrigin3));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin3));

  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          OriginMatcherFunctionUtility::MakeMatcherFunction(
              {kOrigin2, kOrigin3}),
          base::Time(), base::Time::Max(), GetParam().perform_storage_cleanup));

  // All three origins should be cleared.
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin2));
  EXPECT_EQ(0L, db_->Length(kOrigin3));

  EXPECT_TRUE(db_->FetchOrigins().empty());

  // There is no error from trying to clear an origin that isn't in the
  // database.
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          OriginMatcherFunctionUtility::MakeMatcherFunction(
              {"http://www.example4.test"}),
          base::Time(), base::Time::Max(), GetParam().perform_storage_cleanup));
}

TEST_P(SharedStorageDatabasePurgeMatchingOriginsParamTest, SinceThreshold) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin3));

  const url::Origin kOrigin4 =
      url::Origin::Create(GURL("http://www.example4.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin4));

  clock_.SetNow(base::Time::Now());
  clock_.Advance(base::Milliseconds(50));

  // Time threshold that will be used as a starting point for deletion.
  base::Time threshold = clock_.Now();

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  // Read from `kOrigin1`.
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          OriginMatcherFunctionUtility::MakeMatcherFunction(
              {kOrigin1, kOrigin2}),
          threshold, base::Time::Max(), GetParam().perform_storage_cleanup));

  // `kOrigin1` is cleared. The other origins are not.
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(3L, db_->Length(kOrigin3));
  EXPECT_EQ(4L, db_->Length(kOrigin4));

  clock_.Advance(base::Milliseconds(50));

  // Time threshold that will be used as a starting point for deletion.
  threshold = clock_.Now();

  // Write to `kOrigin3`.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin3));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin3, kOrigin4));

  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          OriginMatcherFunctionUtility::MakeMatcherFunction(
              {kOrigin2, kOrigin3, kOrigin4}),
          threshold, base::Time::Max(), GetParam().perform_storage_cleanup));

  // `kOrigin3` is cleared. The others weren't modified within the given time
  // period.
  EXPECT_EQ(0L, db_->Length(kOrigin1));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(0L, db_->Length(kOrigin3));
  EXPECT_EQ(4L, db_->Length(kOrigin4));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin2, kOrigin4));

  // There is no error from trying to clear an origin that isn't in the
  // database.
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->PurgeMatchingOrigins(
          OriginMatcherFunctionUtility::MakeMatcherFunction(
              {"http://www.example5.test"}),
          threshold, base::Time::Max(), GetParam().perform_storage_cleanup));
}

TEST_P(SharedStorageDatabaseParamTest, PurgeStaleOrigins) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(kOrigin1, u"key2").data, u"value2");

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));
  EXPECT_EQ(db_->Get(kOrigin2, u"key1").data, u"value1");

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin3));

  const url::Origin kOrigin4 =
      url::Origin::Create(GURL("http://www.example4.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key2", u"value2"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key3", u"value3"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin4));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  clock_.SetNow(base::Time::Now());
  clock_.Advance(base::Milliseconds(50));

  // Time threshold after which an origin must be read from or written to in
  // order to be considered active.
  base::Time threshold = clock_.Now();
  clock_.Advance(base::Milliseconds(50));

  // Read from `kOrigin1`.
  EXPECT_EQ(db_->Get(kOrigin1, u"key1").data, u"value1");

  // Write to `kOrigin3`.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin3));

  EXPECT_EQ(OperationResult::kSuccess,
            db_->PurgeStaleOrigins(clock_.Now() - threshold));

  // `kOrigin1` was active.
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  // `kOrigin2` was inactive.
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  // `kOrigin3` was active.
  EXPECT_EQ(4L, db_->Length(kOrigin3));

  // `kOrigin4` was inactive.
  EXPECT_EQ(0L, db_->Length(kOrigin4));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin3));
}

TEST_P(SharedStorageDatabaseParamTest, TrimMemory) {
  EXPECT_TRUE(db_->FetchOrigins().empty());

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));

  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://www.example2.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin2, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin2));

  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://www.example3.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin3, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin3));

  const url::Origin kOrigin4 =
      url::Origin::Create(GURL("http://www.example4.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key1", u"value1"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin4, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin4));

  std::vector<url::Origin> origins;
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin1, kOrigin2, kOrigin3, kOrigin4));

  EXPECT_EQ(OperationResult::kSuccess, db_->Clear(kOrigin1));
  EXPECT_EQ(0L, db_->Length(kOrigin1));

  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin2, u"key1"));
  EXPECT_EQ(0L, db_->Length(kOrigin2));

  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin3, kOrigin4));

  // Release nonessential memory.
  db_->TrimMemory();

  // Check that the database is still intact.
  origins.clear();
  for (const auto& info : db_->FetchOrigins())
    origins.push_back(info->origin);
  EXPECT_THAT(origins, ElementsAre(kOrigin3, kOrigin4));

  EXPECT_EQ(1L, db_->Length(kOrigin3));
  EXPECT_EQ(2L, db_->Length(kOrigin4));

  EXPECT_EQ(db_->Get(kOrigin3, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(kOrigin4, u"key1").data, u"value1");
  EXPECT_EQ(db_->Get(kOrigin4, u"key2").data, u"value2");
}

TEST_P(SharedStorageDatabaseParamTest, MaxEntriesPerOrigin) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", u"value1"));
  EXPECT_EQ(1L, db_->Length(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key2", u"value2"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key3", u"value3"));
  EXPECT_EQ(3L, db_->Length(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key4", u"value4"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key5", u"value5"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));

  // `kOrigin1` should have hit capacity, and hence this value will not be set.
  EXPECT_EQ(OperationResult::kNoCapacity,
            db_->Set(kOrigin1, u"key6", u"value6"));

  EXPECT_EQ(5L, db_->Length(kOrigin1));
  EXPECT_EQ(OperationResult::kSuccess, db_->Delete(kOrigin1, u"key5"));
  EXPECT_EQ(4L, db_->Length(kOrigin1));

  // There should now be capacity and the value will be set.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key6", u"value6"));
  EXPECT_EQ(5L, db_->Length(kOrigin1));
}

TEST_P(SharedStorageDatabaseParamTest, MaxStringLength) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("http://www.example1.test"));
  const std::u16string kLongString(kMaxStringLength, u'g');

  // This value has the maximum allowed length.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, u"key1", kLongString));
  EXPECT_EQ(1L, db_->Length(kOrigin1));

  // Appending to the value would exceed the allowed length and so won't
  // succeed.
  EXPECT_EQ(OperationResult::kInvalidAppend,
            db_->Append(kOrigin1, u"key1", u"h"));

  EXPECT_EQ(1L, db_->Length(kOrigin1));

  // This key has the maximum allowed length.
  EXPECT_EQ(OperationResult::kSet, db_->Set(kOrigin1, kLongString, u"value1"));
  EXPECT_EQ(2L, db_->Length(kOrigin1));
}

class SharedStorageDatabaseIteratorTest : public SharedStorageDatabaseTest {
 public:
  void SetUp() override {
    SharedStorageDatabaseTest::SetUp();

    auto options = SharedStorageOptions::Create()->GetDatabaseOptions();
    db_ = std::make_unique<SharedStorageDatabase>(
        file_name_, special_storage_policy_, std::move(options));
  }

  void InitSharedStorageFeature() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        {blink::features::kSharedStorageAPI},
        {{"MaxSharedStorageEntriesPerOrigin",
          base::NumberToString(kMaxEntriesPerOriginForIteratorTest)},
         {"MaxSharedStorageStringLength",
          base::NumberToString(kMaxStringLength)},
         {"MaxSharedStorageIteratorBatchSize",
          base::NumberToString(kMaxBatchSizeForIteratorTest)}});
  }
};

TEST_F(SharedStorageDatabaseIteratorTest, Keys) {
  db_ = LoadFromFile("shared_storage.v1.iterator.sql");
  ASSERT_TRUE(db_);

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  TestSharedStorageEntriesListenerUtility utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = utility.RegisterListener();
  EXPECT_EQ(OperationResult::kSuccess,
            db_->Keys(google_com, utility.BindNewPipeAndPassRemoteForId(id1)));
  utility.FlushForId(id1);
  EXPECT_EQ(201U, utility.TakeKeysForId(id1).size());

  // Batch size is 25 for this test.
  EXPECT_EQ(9U, utility.BatchCountForId(id1));
  utility.VerifyNoErrorForId(id1);

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  size_t id2 = utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->Keys(chromium_org, utility.BindNewPipeAndPassRemoteForId(id2)));
  utility.FlushForId(id2);
  EXPECT_EQ(26U, utility.TakeKeysForId(id2).size());

  // Batch size is 25 for this test.
  EXPECT_EQ(2U, utility.BatchCountForId(id2));
  utility.VerifyNoErrorForId(id2);
}

TEST_F(SharedStorageDatabaseIteratorTest, Entries) {
  db_ = LoadFromFile("shared_storage.v1.iterator.sql");
  ASSERT_TRUE(db_);

  url::Origin google_com = url::Origin::Create(GURL("http://google.com/"));
  TestSharedStorageEntriesListenerUtility utility(
      task_environment_.GetMainThreadTaskRunner());
  size_t id1 = utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->Entries(google_com, utility.BindNewPipeAndPassRemoteForId(id1)));
  utility.FlushForId(id1);
  EXPECT_EQ(201U, utility.TakeEntriesForId(id1).size());

  // Batch size is 25 for this test.
  EXPECT_EQ(9U, utility.BatchCountForId(id1));
  utility.VerifyNoErrorForId(id1);

  url::Origin chromium_org = url::Origin::Create(GURL("http://chromium.org/"));
  size_t id2 = utility.RegisterListener();
  EXPECT_EQ(
      OperationResult::kSuccess,
      db_->Entries(chromium_org, utility.BindNewPipeAndPassRemoteForId(id2)));
  utility.FlushForId(id2);
  EXPECT_EQ(26U, utility.TakeEntriesForId(id2).size());

  // Batch size is 25 for this test.
  EXPECT_EQ(2U, utility.BatchCountForId(id2));
  utility.VerifyNoErrorForId(id2);
}

}  // namespace storage
