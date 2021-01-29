// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_database.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/feed/core/feed_journal_mutation.h"
#include "components/feed/core/proto/journal_storage.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramTester;
using leveldb_proto::test::FakeDB;
using testing::Mock;
using testing::NotNull;
using testing::_;

namespace feed {

namespace {

const char kJournalKey1[] = "JournalKey1";
const char kJournalKey2[] = "JournalKey2";
const char kJournalKey3[] = "JournalKey3";
const char kJournalData1[] = "Journal Data1";
const char kJournalData2[] = "Journal Data2";
const char kJournalData3[] = "Journal Data3";
const char kJournalData4[] = "Journal Data4";
const char kJournalData5[] = "Journal Data5";
const char kJournalData6[] = "Journal Data6";

const char kUmaCommitMutationSizeHistogramName[] =
    "ContentSuggestions.Feed.JournalStorage.CommitMutationCount";
const char kUmaLoadKeysTimeHistogramName[] =
    "ContentSuggestions.Feed.JournalStorage.LoadKeysTime";
const char kUmaLoadTimeHistogramName[] =
    "ContentSuggestions.Feed.JournalStorage.LoadTime";
const char kUmaOperationCommitTimeHistogramName[] =
    "ContentSuggestions.Feed.JournalStorage.OperationCommitTime";
const char kUmaSizeHistogramName[] =
    "ContentSuggestions.Feed.JournalStorage.Count";

}  // namespace

class FeedJournalDatabaseTest : public testing::Test {
 public:
  FeedJournalDatabaseTest() : journal_db_(nullptr) {}

  void CreateDatabase(bool init_database) {
    // The FakeDBs are owned by |feed_db_|, so clear our pointers before
    // resetting |feed_db_| itself.
    journal_db_ = nullptr;
    // Explicitly destroy any existing database before creating a new one.
    feed_db_.reset();

    auto storage_db =
        std::make_unique<FakeDB<JournalStorageProto>>(&journal_db_storage_);

    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

    journal_db_ = storage_db.get();
    feed_db_ = std::make_unique<FeedJournalDatabase>(std::move(storage_db),
                                                     task_runner_);
    if (init_database) {
      InitStatusCallback(journal_db_, leveldb_proto::Enums::InitStatus::kOK);
      ASSERT_TRUE(db()->IsInitialized());
    }
  }

  void InjectJournalStorageProto(const std::string& key,
                                 const std::vector<std::string>& entries) {
    JournalStorageProto storage_proto;
    storage_proto.set_key(key);
    for (const std::string& entry : entries) {
      storage_proto.add_journal_data(entry);
    }
    journal_db_storage_[key] = storage_proto;
  }

  // Since the FakeDB implementation doesn't run callbacks on the same task
  // runner as the original request was made (like the real ProtoDatabase impl
  // does), we explicitly post all callbacks onto the DB task runner here.
  void InitStatusCallback(FakeDB<JournalStorageProto>* storage_db,
                          leveldb_proto::Enums::InitStatus status) {
    task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(
                                [](FakeDB<JournalStorageProto>* storage_db,
                                   leveldb_proto::Enums::InitStatus status) {
                                  storage_db->InitStatusCallback(status);
                                },
                                storage_db, status));
    RunUntilIdle();
  }
  void GetCallback(FakeDB<JournalStorageProto>* storage_db, bool success) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce([](FakeDB<JournalStorageProto>* storage_db,
                          bool success) { storage_db->GetCallback(success); },
                       storage_db, success));
    RunUntilIdle();
  }
  void LoadKeysCallback(FakeDB<JournalStorageProto>* storage_db, bool success) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FakeDB<JournalStorageProto>* storage_db, bool success) {
              storage_db->LoadKeysCallback(success);
            },
            storage_db, success));
    RunUntilIdle();
  }
  void UpdateCallback(FakeDB<JournalStorageProto>* storage_db, bool success) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FakeDB<JournalStorageProto>* storage_db, bool success) {
              storage_db->UpdateCallback(success);
            },
            storage_db, success));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  FakeDB<JournalStorageProto>* storage_db() { return journal_db_; }

  FeedJournalDatabase* db() { return feed_db_.get(); }

  HistogramTester& histogram() { return histogram_; }

  MOCK_METHOD2(OnJournalEntryReceived, void(bool, std::vector<std::string>));
  MOCK_METHOD1(OnStorageCommitted, void(bool));
  MOCK_METHOD2(OnCheckJournalExistReceived, void(bool, bool));

 private:
  base::test::TaskEnvironment task_environment_;

  std::map<std::string, JournalStorageProto> journal_db_storage_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Owned by |feed_db_|.
  FakeDB<JournalStorageProto>* journal_db_;

  std::unique_ptr<FeedJournalDatabase> feed_db_;

  HistogramTester histogram_;

  DISALLOW_COPY_AND_ASSIGN(FeedJournalDatabaseTest);
};

TEST_F(FeedJournalDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase(/*init_database=*/false);

  EXPECT_FALSE(db()->IsInitialized());
  InitStatusCallback(storage_db(), leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(FeedJournalDatabaseTest, LoadJournalEntry) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Store |kJournalKey1| and |kJournalKey2|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4});

  // Try to Load |kJournalKey1|.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, LoadNonExistingJournalEntry) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Try to Load |kJournalKey1|.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 0U);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, AppendJournal) {
  CreateDatabase(/*init_database=*/true);

  // Save |kJournalKey1|.
  auto journal_mutation = std::make_unique<JournalMutation>(kJournalKey1);
  journal_mutation->AddAppendOperation(kJournalData1);
  journal_mutation->AddAppendOperation(kJournalData2);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitJournalMutation(
      std::move(journal_mutation),
      base::BindOnce(&FeedJournalDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);
  UpdateCallback(storage_db(), true);

  // Make sure they're there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/2, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);

  Mock::VerifyAndClearExpectations(this);

  // Append more for |kJournalKey1|.
  journal_mutation = std::make_unique<JournalMutation>(kJournalKey1);
  journal_mutation->AddAppendOperation(kJournalData3);
  journal_mutation->AddAppendOperation(kJournalData4);
  journal_mutation->AddAppendOperation(kJournalData5);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitJournalMutation(
      std::move(journal_mutation),
      base::BindOnce(&FeedJournalDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);
  UpdateCallback(storage_db(), true);

  // Check new instances are there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 5U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
        EXPECT_EQ(results[3], kJournalData4);
        EXPECT_EQ(results[4], kJournalData5);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/3, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 2);
}

TEST_F(FeedJournalDatabaseTest, CopyJournal) {
  CreateDatabase(/*init_database=*/true);

  // Save |kJournalKey1|.
  InjectJournalStorageProto(kJournalKey1, {kJournalData1, kJournalData2});

  // Copy |kJournalKey1| to |kJournalKey2|.
  auto journal_mutation = std::make_unique<JournalMutation>(kJournalKey1);
  journal_mutation->AddCopyOperation(kJournalKey2);
  journal_mutation->AddAppendOperation(kJournalData3);
  journal_mutation->AddAppendOperation(kJournalData4);
  journal_mutation->AddCopyOperation(kJournalKey3);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitJournalMutation(
      std::move(journal_mutation),
      base::BindOnce(&FeedJournalDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);
  UpdateCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/4, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);

  // Check new journal is there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
      });
  db()->LoadJournal(
      kJournalKey2,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);

  Mock::VerifyAndClearExpectations(this);

  // Check new journal is there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 4U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
        EXPECT_EQ(results[3], kJournalData4);
      });
  db()->LoadJournal(
      kJournalKey3,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 2);

  Mock::VerifyAndClearExpectations(this);

  // Check first journal is still there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 4U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
        EXPECT_EQ(results[3], kJournalData4);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 3);
}

TEST_F(FeedJournalDatabaseTest, DeleteJournal) {
  CreateDatabase(/*init_database=*/true);

  // Store |kJournalKey1|, |kJournalKey2|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});

  // Delete |kJournalKey2|.
  auto journal_mutation = std::make_unique<JournalMutation>(kJournalKey2);
  journal_mutation->AddDeleteOperation();
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitJournalMutation(
      std::move(journal_mutation),
      base::BindOnce(&FeedJournalDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  RunUntilIdle();
  UpdateCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/1, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);

  // Make sure |kJournalKey2| got deleted.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 0U);
      });
  db()->LoadJournal(
      kJournalKey2,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);

  Mock::VerifyAndClearExpectations(this);

  // Make sure |kJournalKey1| is still there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 2);
}

TEST_F(FeedJournalDatabaseTest, ChecExistingJournal) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Store |kJournalKey1| and |kJournalKey2|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4});

  // Check |kJournalKey1|.
  EXPECT_CALL(*this, OnCheckJournalExistReceived(true, true));

  db()->DoesJournalExist(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnCheckJournalExistReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, CheckNonExistingJournal) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Check |kJournalKey1|.
  EXPECT_CALL(*this, OnCheckJournalExistReceived(true, false));
  db()->DoesJournalExist(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnCheckJournalExistReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, LoadAllJournalKeys) {
  CreateDatabase(/*init_database=*/true);

  // Store |kJournalKey1|, |kJournalKey2| and |kJournalKey3|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalKey1);
        EXPECT_EQ(results[1], kJournalKey2);
        EXPECT_EQ(results[2], kJournalKey3);
      });
  db()->LoadAllJournalKeys(
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  LoadKeysCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaSizeHistogramName,
                                /*size=*/3, 1);
  histogram().ExpectTotalCount(kUmaLoadKeysTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, DeleteAllJournals) {
  CreateDatabase(/*init_database=*/true);

  // Store |kJournalKey1|, |kJournalKey2|, |kJournalKey3|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  // Delete all journals, meaning |kJournalKey1|, |kJournalKey2| and
  // |kJournalKey3| are expected to be deleted.
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->DeleteAllJournals(base::BindOnce(
      &FeedJournalDatabaseTest::OnStorageCommitted, base::Unretained(this)));
  UpdateCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);

  // Make sure all journals got deleted.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 0U);
      });
  db()->LoadAllJournalKeys(
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  LoadKeysCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaSizeHistogramName,
                                /*size=*/0, 1);
  histogram().ExpectTotalCount(kUmaLoadKeysTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, LoadJournalEntryFail) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Store |kJournalKey1| and |kJournalKey2|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4});

  // Try to Load |kJournalKey1|.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_FALSE(success);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), false);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, LoadNonExistingJournalEntryFail) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Try to Load |kJournalKey1|.
  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_FALSE(success);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), false);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, LoadAllJournalKeysFail) {
  CreateDatabase(/*init_database=*/true);

  // Store |kJournalKey1|, |kJournalKey2| and |kJournalKey3|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  EXPECT_CALL(*this, OnJournalEntryReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_FALSE(success);
      });
  db()->LoadAllJournalKeys(
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  LoadKeysCallback(storage_db(), false);

  histogram().ExpectTotalCount(kUmaSizeHistogramName, 0);
  histogram().ExpectTotalCount(kUmaLoadKeysTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, ChecExistingJournalFail) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Store |kJournalKey1| and |kJournalKey2|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4});

  // Check |kJournalKey1|.
  EXPECT_CALL(*this, OnCheckJournalExistReceived(false, true));

  db()->DoesJournalExist(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnCheckJournalExistReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), false);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedJournalDatabaseTest, CheckNonExistingJournalFail) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Check |kJournalKey1|.
  EXPECT_CALL(*this, OnCheckJournalExistReceived(false, false));
  db()->DoesJournalExist(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnCheckJournalExistReceived,
                     base::Unretained(this)));
  GetCallback(storage_db(), false);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

}  // namespace feed
