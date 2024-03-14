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

  UrlEmbeddings url_embeddings(1, 1, base::Time::Now());
  url_embeddings.embeddings.push_back(Embedding({
      1.0f,
      2.0f,
      3.0f,
      4.0f,
  }));
  EXPECT_TRUE(sql_database->AddUrlEmbeddings(url_embeddings));

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

}  // namespace history_embeddings
