// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/sql_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

class HistoryEmbeddingsSqlDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    OSCryptMocker::SetUp();
    CHECK(history_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    CHECK(history_dir_.Delete());
    OSCryptMocker::TearDown();
  }

  // Adds mock data for url_id = 1 tied to visit_id = 10, and url_id = 2 tied to
  // visit_id = 11.
  void AddBasicMockData(SqlDatabase* sql_database) {
    {
      UrlPassages url_passages_1(1, 10, base::Time::Now());
      url_passages_1.passages.add_passages("fake passage 1");
      url_passages_1.passages.add_passages("fake passage 2");
      ASSERT_TRUE(sql_database->InsertOrReplacePassages(url_passages_1));

      UrlEmbeddings embeddings_1(1, 10, base::Time::Now());
      embeddings_1.embeddings.push_back(Embedding({1.0f, 2.0f, 3.0f, 4.0f}));
      ASSERT_TRUE(sql_database->AddUrlEmbeddings(embeddings_1));
    }

    {
      UrlPassages url_passages_2(2, 11, base::Time::Now());
      url_passages_2.passages.add_passages("fake passage 3");
      url_passages_2.passages.add_passages("fake passage 4");
      ASSERT_TRUE(sql_database->InsertOrReplacePassages(url_passages_2));

      UrlEmbeddings embeddings_2(2, 11, base::Time::Now());
      embeddings_2.embeddings.push_back(Embedding({1.0f, 2.0f, 3.0f, 4.0f}));
      ASSERT_TRUE(sql_database->AddUrlEmbeddings(embeddings_2));
    }

    ASSERT_TRUE(sql_database->GetPassages(1));
    ASSERT_TRUE(sql_database->GetPassages(2));
    ASSERT_EQ(GetEmbeddingCount(sql_database), 2U);
  }

  size_t GetEmbeddingCount(SqlDatabase* sql_database) {
    auto iterator = sql_database->MakeEmbeddingsIterator();
    EXPECT_TRUE(iterator);
    size_t count = 0;
    while (iterator->Next()) {
      count++;
    }
    return count;
  }

 protected:
  base::ScopedTempDir history_dir_;
};

TEST_F(HistoryEmbeddingsSqlDatabaseTest, WriteCloseAndThenReadPassages) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

  // Write passages
  UrlPassages url_passages(1, 1, base::Time::Now());
  proto::PassagesValue& original_proto = url_passages.passages;
  original_proto.add_passages("fake passage 1");
  original_proto.add_passages("fake passage 2");
  EXPECT_TRUE(sql_database->InsertOrReplacePassages(url_passages));

  // Reset and reload.
  sql_database.reset();
  sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

  // Read passages
  auto read_proto = sql_database->GetPassages(1);
  ASSERT_TRUE(read_proto);
  ASSERT_EQ(read_proto->passages_size(), 2);
  EXPECT_EQ(read_proto->passages()[0], "fake passage 1");
  EXPECT_EQ(read_proto->passages()[1], "fake passage 2");

  sql_database.reset();
  EXPECT_TRUE(
      base::PathExists(history_dir_.GetPath().Append(kHistoryEmbeddingsName)))
      << "DB file is still there after destruction.";
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, WriteCloseAndThenReadEmbeddings) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

  // Write embeddings
  constexpr size_t kCount = 2;
  UrlEmbeddings url_embeddings[kCount] = {
      UrlEmbeddings(1, 1, base::Time::Now()),
      UrlEmbeddings(2, 2, base::Time::Now()),
  };
  url_embeddings[0].embeddings.push_back(Embedding({
      1.0f,
      2.0f,
      3.0f,
      4.0f,
  }));
  url_embeddings[1].embeddings.push_back(Embedding({
      1.0f,
      2.0f,
      3.0f,
      4.0f,
  }));
  url_embeddings[1].embeddings.push_back(Embedding({
      5.0f,
      6.0f,
      7.0f,
      8.0f,
  }));
  EXPECT_TRUE(sql_database->AddUrlEmbeddings(url_embeddings[0]));
  EXPECT_TRUE(sql_database->AddUrlEmbeddings(url_embeddings[1]));

  // Reset and reload.
  sql_database.reset();
  sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

  // Read embeddings
  {
    // Block scope destructs iterator before database is closed.
    std::unique_ptr<VectorDatabase::EmbeddingsIterator> iterator =
        sql_database->MakeEmbeddingsIterator();
    EXPECT_TRUE(iterator);
    for (const UrlEmbeddings& url_embedding : url_embeddings) {
      const UrlEmbeddings* read_url_embeddings = iterator->Next();
      EXPECT_TRUE(read_url_embeddings);
      // Check specific members; easier to debug than full object check.
      EXPECT_EQ(read_url_embeddings->url_id, url_embedding.url_id);
      EXPECT_EQ(read_url_embeddings->visit_id, url_embedding.visit_id);
      EXPECT_EQ(read_url_embeddings->visit_time, url_embedding.visit_time);
      // Finally, check full equality including vectors.
      EXPECT_EQ(*read_url_embeddings, url_embedding);
    }
    EXPECT_FALSE(iterator->Next());
  }

  sql_database.reset();
  EXPECT_TRUE(
      base::PathExists(history_dir_.GetPath().Append(kHistoryEmbeddingsName)))
      << "DB file is still there after destruction.";
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, InsertOrReplacePassages) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

  UrlPassages url_passages(1, 1, base::Time::Now());
  url_passages.passages.add_passages("fake passage 1");
  url_passages.passages.add_passages("fake passage 2");
  EXPECT_TRUE(sql_database->InsertOrReplacePassages(url_passages));
  url_passages.visit_id = 2;
  url_passages.passages.add_passages("fake passage 3");
  EXPECT_TRUE(sql_database->InsertOrReplacePassages(url_passages));

  // Verify that the new one has replaced the old one.
  auto read_proto = sql_database->GetPassages(1);
  ASSERT_TRUE(read_proto);
  ASSERT_EQ(read_proto->passages_size(), 3);
  EXPECT_EQ(read_proto->passages()[0], "fake passage 1");
  EXPECT_EQ(read_proto->passages()[1], "fake passage 2");
  EXPECT_EQ(read_proto->passages()[2], "fake passage 3");

  EXPECT_FALSE(sql_database->GetPassages(2));
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, IteratorMaySafelyOutliveDatabase) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  AddBasicMockData(sql_database.get());

  // Without database reset, iteration reads data.
  {
    std::unique_ptr<VectorDatabase::EmbeddingsIterator> iterator =
        sql_database->MakeEmbeddingsIterator();
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(iterator->Next());
  }

  // With database reset, iteration gracefully ends.
  {
    std::unique_ptr<VectorDatabase::EmbeddingsIterator> iterator =
        sql_database->MakeEmbeddingsIterator();
    EXPECT_TRUE(iterator);

    // Reset database while iterator is still in scope.
    sql_database.reset();

    // Iterator access with dead database doesn't crash, just ends iteration.
    EXPECT_FALSE(iterator->Next());
  }
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, DeleteDataForUrlId) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  AddBasicMockData(sql_database.get());

  EXPECT_TRUE(sql_database->DeleteDataForUrlId(3))
      << "Deleting a non-existing url_id = 3 should return true but do "
         "nothing.";
  EXPECT_TRUE(sql_database->GetPassages(1));
  EXPECT_TRUE(sql_database->GetPassages(2));
  EXPECT_EQ(GetEmbeddingCount(sql_database.get()), 2U);

  EXPECT_TRUE(sql_database->DeleteDataForUrlId(2))
      << "Succeeds. url_id = 2 does exist.";
  EXPECT_TRUE(sql_database->GetPassages(1));
  EXPECT_FALSE(sql_database->GetPassages(2));
  EXPECT_EQ(GetEmbeddingCount(sql_database.get()), 1U);
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, DeleteDataForVisitId) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  AddBasicMockData(sql_database.get());

  EXPECT_TRUE(sql_database->DeleteDataForVisitId(40))
      << "Deleting a non-existing visit_id = 40 should return true but do "
         "nothing.";
  EXPECT_TRUE(sql_database->GetPassages(1));
  EXPECT_TRUE(sql_database->GetPassages(2));
  EXPECT_EQ(GetEmbeddingCount(sql_database.get()), 2U);

  EXPECT_TRUE(sql_database->DeleteDataForVisitId(11))
      << "Succeeds. visit_id = 11 does exist.";
  EXPECT_TRUE(sql_database->GetPassages(1));
  EXPECT_FALSE(sql_database->GetPassages(2));
  EXPECT_EQ(GetEmbeddingCount(sql_database.get()), 1U);
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, DeleteAllData) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  AddBasicMockData(sql_database.get());

  EXPECT_TRUE(sql_database->DeleteAllData());
  EXPECT_FALSE(sql_database->GetPassages(1));
  EXPECT_FALSE(sql_database->GetPassages(2));
  EXPECT_EQ(GetEmbeddingCount(sql_database.get()), 0U);
}

}  // namespace history_embeddings
