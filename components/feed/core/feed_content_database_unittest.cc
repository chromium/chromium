// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_content_database.h"

#include <map>

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/feed/core/feed_content_mutation.h"
#include "components/feed/core/proto/content_storage.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramTester;
using leveldb_proto::test::FakeDB;
using testing::_;

namespace feed {

namespace {

const char kContentKeyPrefix[] = "ContentKey";
const char kContentKey1[] = "ContentKey1";
const char kContentKey2[] = "ContentKey2";
const char kContentKey3[] = "ContentKey3";
const char kContentData1[] = "Content Data1";
const char kContentData2[] = "Content Data2";

const char kUmaCommitMutationSizeHistogramName[] =
    "ContentSuggestions.Feed.ContentStorage.CommitMutationCount";
const char kUmaLoadKeysTimeHistogramName[] =
    "ContentSuggestions.Feed.ContentStorage.LoadKeysTime";
const char kUmaLoadTimeHistogramName[] =
    "ContentSuggestions.Feed.ContentStorage.LoadTime";
const char kUmaOperationCommitTimeHistogramName[] =
    "ContentSuggestions.Feed.ContentStorage.OperationCommitTime";
const char kUmaSizeHistogramName[] =
    "ContentSuggestions.Feed.ContentStorage.Count";

}  // namespace

class FeedContentDatabaseTest : public testing::Test {
 public:
  FeedContentDatabaseTest() : content_db_(nullptr) {}

  void CreateDatabase(bool init_database) {
    // The FakeDBs are owned by |feed_db_|, so clear our pointers before
    // resetting |feed_db_| itself.
    content_db_ = nullptr;
    // Explicitly destroy any existing database before creating a new one.
    feed_db_.reset();

    auto storage_db =
        std::make_unique<FakeDB<ContentStorageProto>>(&content_db_storage_);

    task_runner_ =
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::USER_VISIBLE});

    content_db_ = storage_db.get();
    feed_db_ = std::make_unique<FeedContentDatabase>(std::move(storage_db),
                                                     task_runner_);
    if (init_database) {
      InitStatusCallback(content_db_, leveldb_proto::Enums::InitStatus::kOK);
      ASSERT_TRUE(db()->IsInitialized());
    }
  }

  void InjectContentStorageProto(const std::string& key,
                                 const std::string& data) {
    ContentStorageProto storage_proto;
    storage_proto.set_key(key);
    storage_proto.set_content_data(data);
    content_db_storage_[key] = storage_proto;
  }

  // Since the FakeDB implementation doesn't run callbacks on the same task
  // runner as the original request was made (like the real ProtoDatabase impl
  // does), we explicitly post all callbacks onto the DB task runner here.
  void InitStatusCallback(FakeDB<ContentStorageProto>* storage_db,
                          leveldb_proto::Enums::InitStatus status) {
    task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(
                                [](FakeDB<ContentStorageProto>* storage_db,
                                   leveldb_proto::Enums::InitStatus status) {
                                  storage_db->InitStatusCallback(status);
                                },
                                storage_db, status));
    RunUntilIdle();
  }
  void LoadCallback(FakeDB<ContentStorageProto>* storage_db, bool success) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce([](FakeDB<ContentStorageProto>* storage_db,
                          bool success) { storage_db->LoadCallback(success); },
                       storage_db, success));
    RunUntilIdle();
  }
  void LoadKeysCallback(FakeDB<ContentStorageProto>* storage_db, bool success) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FakeDB<ContentStorageProto>* storage_db, bool success) {
              storage_db->LoadKeysCallback(success);
            },
            storage_db, success));
    RunUntilIdle();
  }
  void UpdateCallback(FakeDB<ContentStorageProto>* storage_db, bool success) {
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](FakeDB<ContentStorageProto>* storage_db, bool success) {
              storage_db->UpdateCallback(success);
            },
            storage_db, success));
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  FakeDB<ContentStorageProto>* storage_db() { return content_db_; }

  FeedContentDatabase* db() { return feed_db_.get(); }

  HistogramTester& histogram() { return histogram_; }

  MOCK_METHOD2(OnContentEntriesReceived,
               void(bool, std::vector<std::pair<std::string, std::string>>));
  MOCK_METHOD2(OnContentKeyReceived, void(bool, std::vector<std::string>));
  MOCK_METHOD1(OnStorageCommitted, void(bool));

 private:
  base::test::TaskEnvironment task_environment_;

  std::map<std::string, ContentStorageProto> content_db_storage_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Owned by |feed_db_|.
  FakeDB<ContentStorageProto>* content_db_;

  std::unique_ptr<FeedContentDatabase> feed_db_;

  HistogramTester histogram_;

  DISALLOW_COPY_AND_ASSIGN(FeedContentDatabaseTest);
};

TEST_F(FeedContentDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase(/*init_database=*/false);

  InitStatusCallback(storage_db(), leveldb_proto::Enums::InitStatus::kOK);

  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(FeedContentDatabaseTest, LoadContentAfterInitSuccess) {
  CreateDatabase(/*init_database=*/true);

  EXPECT_CALL(*this, OnContentEntriesReceived(_, _));
  db()->LoadContent(
      {kContentKey1},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, LoadContentsEntries) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Try to Load |kContentKey2| and |kContentKey3|, only |kContentKey2| should
  // return.
  EXPECT_CALL(*this, OnContentEntriesReceived(_, _))
      .WillOnce([](bool success,
                   std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].first, kContentKey2);
        EXPECT_EQ(results[0].second, kContentData2);
      });
  db()->LoadContent(
      {kContentKey2, kContentKey3},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, LoadContentsEntriesByPrefix) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Try to Load "ContentKey", both |kContentKey1| and |kContentKey2| should
  // return.
  EXPECT_CALL(*this, OnContentEntriesReceived(_, _))
      .WillOnce([](bool success,
                   std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
        EXPECT_EQ(results[1].first, kContentKey2);
        EXPECT_EQ(results[1].second, kContentData2);
      });
  db()->LoadContentByPrefix(
      kContentKeyPrefix,
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), true);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, LoadAllContentKeys) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  EXPECT_CALL(*this, OnContentKeyReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0], kContentKey1);
        EXPECT_EQ(results[1], kContentKey2);
      });
  db()->LoadAllContentKeys(base::BindOnce(
      &FeedContentDatabaseTest::OnContentKeyReceived, base::Unretained(this)));
  LoadKeysCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaSizeHistogramName,
                                /*size=*/2, 1);
  histogram().ExpectTotalCount(kUmaLoadKeysTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, SaveContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendUpsertOperation(kContentKey1, kContentData1);
  content_mutation->AppendUpsertOperation(kContentKey2, kContentData2);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  UpdateCallback(storage_db(), true);
  UpdateCallback(storage_db(), true);

  // Make sure they're there.
  EXPECT_CALL(*this, OnContentEntriesReceived(_, _))
      .WillOnce([](bool success,
                   std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_TRUE(success);
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
        EXPECT_EQ(results[1].first, kContentKey2);
        EXPECT_EQ(results[1].second, kContentData2);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/2, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, DeleteContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Delete |kContentKey2| and |kContentKey3|
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendDeleteOperation(kContentKey2);
  content_mutation->AppendDeleteOperation(kContentKey3);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  UpdateCallback(storage_db(), true);
  UpdateCallback(storage_db(), true);

  // Make sure only |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_, _))
      .WillOnce([](bool success,
                   std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_TRUE(success);
        EXPECT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/2, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, DeleteContentByPrefix) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Delete |kContentKey1| and |kContentKey2|
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendDeleteByPrefixOperation(kContentKeyPrefix);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  UpdateCallback(storage_db(), true);

  // Make sure |kContentKey1| and |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_, _))
      .WillOnce([](bool success,
                   std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_TRUE(success);
        EXPECT_EQ(results.size(), 0U);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/1, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, DeleteAllContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Delete all content, meaning |kContentKey1| and |kContentKey2| are expected
  // to be deleted.
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendDeleteAllOperation();
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  UpdateCallback(storage_db(), true);

  // Make sure |kContentKey1| and |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_, _))
      .WillOnce([](bool success,
                   std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_TRUE(success);
        EXPECT_EQ(results.size(), 0U);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/1, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, SaveAndDeleteContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  std::unique_ptr<ContentMutation> content_mutation =
      std::make_unique<ContentMutation>();
  content_mutation->AppendUpsertOperation(kContentKey1, kContentData1);
  content_mutation->AppendUpsertOperation(kContentKey2, kContentData2);
  content_mutation->AppendDeleteOperation(kContentKey2);
  content_mutation->AppendDeleteOperation(kContentKey3);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CommitContentMutation(
      std::move(content_mutation),
      base::BindOnce(&FeedContentDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  UpdateCallback(storage_db(), true);
  UpdateCallback(storage_db(), true);
  UpdateCallback(storage_db(), true);
  UpdateCallback(storage_db(), true);

  // Make sure only |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_, _))
      .WillOnce([](bool success,
                   std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_TRUE(success);
        EXPECT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), true);

  histogram().ExpectBucketCount(kUmaCommitMutationSizeHistogramName,
                                /*operations=*/4, 1);
  histogram().ExpectTotalCount(kUmaOperationCommitTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, LoadContentsFail) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Try to Load |kContentKey2| and |kContentKey3|,.
  EXPECT_CALL(*this, OnContentEntriesReceived(_, _))
      .WillOnce([](bool success,
                   std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_FALSE(success);
      });
  db()->LoadContent(
      {kContentKey2, kContentKey3},
      base::BindOnce(&FeedContentDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  LoadCallback(storage_db(), false);

  histogram().ExpectTotalCount(kUmaLoadTimeHistogramName, 1);
}

TEST_F(FeedContentDatabaseTest, LoadAllContentKeysFail) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  EXPECT_CALL(*this, OnContentKeyReceived(_, _))
      .WillOnce([](bool success, std::vector<std::string> results) {
        EXPECT_FALSE(success);
      });
  db()->LoadAllContentKeys(base::BindOnce(
      &FeedContentDatabaseTest::OnContentKeyReceived, base::Unretained(this)));
  LoadKeysCallback(storage_db(), false);

  histogram().ExpectTotalCount(kUmaSizeHistogramName, 0);
  histogram().ExpectTotalCount(kUmaLoadKeysTimeHistogramName, 1);
}

}  // namespace feed
