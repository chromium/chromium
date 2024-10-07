// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/sql_database.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

namespace {

constexpr int64_t kEmbeddingsVersion = 1;
constexpr size_t kEmbeddingsSize = 768ul;

Embedding FakeEmbedding() {
  Embedding embedding(std::vector<float>(kEmbeddingsSize, 1.0f));
  embedding.Normalize();
  return embedding;
}

}  // namespace

class HistoryEmbeddingsSqlDatabaseTest : public testing::Test {
 public:
  HistoryEmbeddingsSqlDatabaseTest()
      : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting()) {}

  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    CHECK(history_dir_.Delete());
  }

  // Adds mock data for url_id = 1 tied to visit_id = 10, and url_id = 2 tied to
  // visit_id = 11.
  void AddBasicMockData(SqlDatabase* sql_database) {
    {
      UrlPassagesEmbeddings url_data_1(1, 10, base::Time::Now());
      url_data_1.url_passages.passages.add_passages("fake passage 1");
      url_data_1.url_passages.passages.add_passages("fake passage 2");
      url_data_1.url_embeddings.embeddings.emplace_back(
          std::vector<float>(kEmbeddingsSize, 1.0f));
      url_data_1.url_embeddings.embeddings.emplace_back(
          std::vector<float>(kEmbeddingsSize, 1.0f));
      ASSERT_TRUE(sql_database->AddUrlData(url_data_1));
    }

    {
      UrlPassagesEmbeddings url_data_2(2, 11, base::Time::Now());
      url_data_2.url_passages.passages.add_passages("fake passage 3");
      url_data_2.url_passages.passages.add_passages("fake passage 4");
      url_data_2.url_embeddings.embeddings.emplace_back(
          std::vector<float>(kEmbeddingsSize, 1.0f));
      url_data_2.url_embeddings.embeddings.emplace_back(
          std::vector<float>(kEmbeddingsSize, 1.0f));
      ASSERT_TRUE(sql_database->AddUrlData(url_data_2));
    }

    ASSERT_TRUE(sql_database->GetPassages(1));
    ASSERT_TRUE(sql_database->GetPassages(2));
    ASSERT_EQ(GetEmbeddingCount(sql_database), 2U);
  }

  size_t GetEmbeddingCount(SqlDatabase* sql_database) {
    auto iterator = sql_database->MakeUrlDataIterator({});
    EXPECT_TRUE(iterator);
    size_t count = 0;
    while (iterator->Next()) {
      count++;
    }
    return count;
  }

 protected:
  os_crypt_async::Encryptor GetEncryptorInstance() {
    base::test::TestFuture<os_crypt_async::Encryptor, bool> future;
    std::ignore = os_crypt_->GetInstance(future.GetCallback());
    auto [encryptor, result] = future.Take();
    EXPECT_TRUE(result);
    return std::move(encryptor);
  }

  base::test::TaskEnvironment env_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  base::ScopedTempDir history_dir_;
};

TEST_F(HistoryEmbeddingsSqlDatabaseTest, WriteCloseAndThenReadPassages) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

  // Write passages
  UrlPassages url_passages(1, 1, base::Time::Now());
  proto::PassagesValue& original_proto = url_passages.passages;
  original_proto.add_passages("fake passage 1");
  original_proto.add_passages("fake passage 2");
  EXPECT_TRUE(sql_database->InsertOrReplacePassages(url_passages));

  // Reset and reload.
  sql_database.reset();
  sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

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

TEST_F(HistoryEmbeddingsSqlDatabaseTest, WriteCloseAndThenReadUrlData) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

  // Write embeddings.
  UrlPassagesEmbeddings url_datas[] = {
      UrlPassagesEmbeddings(1, 1, base::Time::Now()),
      UrlPassagesEmbeddings(2, 2, base::Time::Now()),
  };
  url_datas[0].url_passages.passages.add_passages("data 0 passage 0");
  url_datas[0].url_embeddings.embeddings.push_back(FakeEmbedding());
  url_datas[1].url_passages.passages.add_passages("data 1 passage 0");
  url_datas[1].url_passages.passages.add_passages("data 1 passage 1");
  url_datas[1].url_embeddings.embeddings.push_back(FakeEmbedding());
  url_datas[1].url_embeddings.embeddings.push_back(FakeEmbedding());
  EXPECT_TRUE(sql_database->AddUrlData(url_datas[0]));
  EXPECT_TRUE(sql_database->AddUrlData(url_datas[1]));

  // Reset and reload.
  sql_database.reset();
  sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

  // Read embeddings.
  {
    // Block scope destructs iterator before database is closed.
    std::unique_ptr<VectorDatabase::UrlDataIterator> iterator =
        sql_database->MakeUrlDataIterator({});
    EXPECT_TRUE(iterator);
    for (const UrlPassagesEmbeddings& url_data : url_datas) {
      const UrlPassagesEmbeddings* read_url_data = iterator->Next();
      EXPECT_TRUE(read_url_data);
      EXPECT_EQ(*read_url_data, url_data);
    }
    EXPECT_FALSE(iterator->Next());
  }

  sql_database.reset();
  EXPECT_TRUE(
      base::PathExists(history_dir_.GetPath().Append(kHistoryEmbeddingsName)))
      << "DB file is still there after destruction.";
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, TimeRangeNarrowsSearchResult) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

  // Write embeddings.
  const base::Time now = base::Time::Now();
  for (size_t i = 0; i < 3; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, now + base::Minutes(i));
    for (size_t j = 0; j < 3; j++) {
      url_data.url_passages.passages.add_passages("fake passage");
      url_data.url_embeddings.embeddings.push_back(FakeEmbedding());
    }
    sql_database->AddUrlData(url_data);
  }
  Embedding query = FakeEmbedding();
  SearchParams search_params;

  // An ordinary search with full results:
  {
    std::vector<ScoredUrl> scored_urls =
        sql_database
            ->FindNearest({}, 3, search_params, query,
                          base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 3u);
  }

  // Narrowed searches with time range.
  {
    std::vector<ScoredUrl> scored_urls =
        sql_database
            ->FindNearest(now, 3, search_params, query,
                          base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 3u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        sql_database
            ->FindNearest(now + base::Seconds(30), 3, search_params, query,
                          base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 2u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        sql_database
            ->FindNearest(now + base::Seconds(90), 3, search_params, query,
                          base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 1u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        sql_database
            ->FindNearest(now + base::Minutes(2), 3, search_params, query,
                          base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 1u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        sql_database
            ->FindNearest(now + base::Seconds(121), 3, search_params, query,
                          base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 0u);
  }
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, InsertOrReplacePassages) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

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
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());
  AddBasicMockData(sql_database.get());

  // Without database reset, iteration reads data.
  {
    std::unique_ptr<VectorDatabase::UrlDataIterator> iterator =
        sql_database->MakeUrlDataIterator({});
    EXPECT_TRUE(iterator);
    EXPECT_TRUE(iterator->Next());
  }

  // With database reset, iteration gracefully ends.
  {
    std::unique_ptr<VectorDatabase::UrlDataIterator> iterator =
        sql_database->MakeUrlDataIterator({});
    EXPECT_TRUE(iterator);

    // Reset database while iterator is still in scope.
    sql_database.reset();

    // Iterator access with dead database doesn't crash, just ends iteration.
    EXPECT_FALSE(iterator->Next());
  }
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, DeleteDataForUrlId) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());
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
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());
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
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());
  AddBasicMockData(sql_database.get());

  EXPECT_TRUE(sql_database->DeleteAllData(true, true));
  EXPECT_FALSE(sql_database->GetPassages(1));
  EXPECT_FALSE(sql_database->GetPassages(2));
  EXPECT_EQ(GetEmbeddingCount(sql_database.get()), 0U);
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, DeleteDataWithoutEmbedderMetadata) {
  UrlPassagesEmbeddings url_data(1, 10, base::Time::Now());
  url_data.url_passages.passages.add_passages("fake passage 1");
  url_data.url_embeddings.embeddings.emplace_back(
      std::vector<float>(kEmbeddingsSize, 1.0f));

  {
    auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());

    // Adding data is expected to fail because the database can't initialize
    // fully without embedder metadata.
    ASSERT_FALSE(sql_database->AddUrlData(url_data));

    // With metadata set, now adding the data succeeds.
    sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                      GetEncryptorInstance());
    ASSERT_TRUE(sql_database->AddUrlData(url_data));

    // Don't delete yet. That would succeed as normal. Close with data resident.
    EXPECT_TRUE(sql_database->GetPassages(1));
  }
  {
    // Initialize database again, to see that we can still get it only when
    // metadata is provided.
    auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
    EXPECT_FALSE(sql_database->GetPassages(1));
    sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                      GetEncryptorInstance());
    EXPECT_TRUE(sql_database->GetPassages(1));
    EXPECT_EQ(GetEmbeddingCount(sql_database.get()), 1U);
    // Again deletion would work as normal here.
  }
  {
    // Initialize database again, with no embedder metadata.
    auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
    EXPECT_FALSE(sql_database->GetPassages(1));

    // Deletion succeeds even with no metadata provided.
    EXPECT_TRUE(sql_database->DeleteAllData(true, true));

    // Now there's no data to retrieve, even after metadata is provided.
    sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                      GetEncryptorInstance());
    EXPECT_FALSE(sql_database->GetPassages(1));
    EXPECT_EQ(GetEmbeddingCount(sql_database.get()), 0U);
  }
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, GetUrlData) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

  // Store passages.
  UrlPassages url_passages(1, 1, base::Time::Now());
  url_passages.passages.add_passages("fake passage 1");
  url_passages.passages.add_passages("fake passage 2");
  url_passages.passages.add_passages("fake passage 3");
  EXPECT_TRUE(sql_database->InsertOrReplacePassages(url_passages));

  UrlPassagesEmbeddings data = sql_database->GetUrlData(1).value();

  // There are passages, but no embeddings stored yet.
  CHECK_EQ(data.url_passages.url_id, 1);
  CHECK_EQ(data.url_passages.visit_id, 1);
  CHECK_EQ(data.url_passages.passages.passages_size(), 3);
  CHECK_EQ(data.url_embeddings.embeddings.size(), 0u);

  // Store embeddings.
  UrlEmbeddings url_embeddings(url_passages);
  url_embeddings.embeddings.push_back(FakeEmbedding());
  url_embeddings.embeddings.push_back(FakeEmbedding());
  url_embeddings.embeddings.push_back(FakeEmbedding());
  sql_database->InsertOrReplaceEmbeddings(url_embeddings);

  // There are passages and embeddings now.
  data = sql_database->GetUrlData(1).value();
  CHECK_EQ(data.url_passages.url_id, 1);
  CHECK_EQ(data.url_passages.visit_id, 1);
  CHECK_EQ(data.url_passages.passages.passages_size(), 3);
  EXPECT_EQ(data.url_passages.passages.passages(0), "fake passage 1");
  EXPECT_EQ(data.url_passages.passages.passages(1), "fake passage 2");
  EXPECT_EQ(data.url_passages.passages.passages(2), "fake passage 3");
  CHECK_EQ(data.url_embeddings.url_id, 1);
  CHECK_EQ(data.url_embeddings.visit_id, 1);
  CHECK_EQ(data.url_embeddings.embeddings.size(), 3u);

  // Absent `url_id` returns std::nullopt.
  EXPECT_FALSE(sql_database->GetUrlData(2).has_value());
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, IterationSkipsAndReportsMismatches) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

  // Write embeddings.
  UrlPassagesEmbeddings url_datas[] = {
      UrlPassagesEmbeddings(1, 1, base::Time::Now()),
      UrlPassagesEmbeddings(2, 2, base::Time::Now()),
  };
  url_datas[0].url_passages.passages.add_passages("data 0 passage 0");
  url_datas[0].url_embeddings.embeddings.push_back(FakeEmbedding());
  url_datas[1].url_passages.passages.add_passages("data 1 passage 0");
  url_datas[1].url_passages.passages.add_passages("data 1 passage 1");
  url_datas[1].url_embeddings.embeddings.push_back(FakeEmbedding());
  url_datas[1].url_embeddings.embeddings.push_back(FakeEmbedding());
  // Add one too many embeddings to trigger a mismatch.
  url_datas[1].url_embeddings.embeddings.push_back(FakeEmbedding());
  EXPECT_TRUE(sql_database->AddUrlData(url_datas[0]));
  EXPECT_TRUE(sql_database->AddUrlData(url_datas[1]));

  base::HistogramTester histogram_tester;
  int observed = 0;
  {
    // Iterate through stored data once.
    std::unique_ptr<VectorDatabase::UrlDataIterator> iterator =
        sql_database->MakeUrlDataIterator({});
    EXPECT_TRUE(iterator);
    while (iterator->Next()) {
      observed++;
    }
  }
  EXPECT_EQ(observed, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.DatabaseIterationSkippedMismatches", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "History.Embeddings.DatabaseIterationYielded", 1, 1);
}

TEST_F(HistoryEmbeddingsSqlDatabaseTest, OldVisitsAreExpired) {
  auto sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

  // Write embeddings; one for an old visit, one for a new visit.
  UrlPassagesEmbeddings url_datas[] = {
      UrlPassagesEmbeddings(/*url_id=*/1, /*visit_id=*/1,
                            base::Time::Now() - base::Days(100)),
      UrlPassagesEmbeddings(/*url_id=*/2, /*visit_id=*/2, base::Time::Now()),
  };
  url_datas[0].url_passages.passages.add_passages("data 0 passage 0");
  url_datas[0].url_embeddings.embeddings.push_back(FakeEmbedding());
  url_datas[1].url_passages.passages.add_passages("data 1 passage 0");
  url_datas[1].url_embeddings.embeddings.push_back(FakeEmbedding());
  EXPECT_TRUE(sql_database->AddUrlData(url_datas[0]));
  EXPECT_TRUE(sql_database->AddUrlData(url_datas[1]));

  // Reset and reload.
  sql_database.reset();
  sql_database = std::make_unique<SqlDatabase>(history_dir_.GetPath());
  sql_database->SetEmbedderMetadata({kEmbeddingsVersion, kEmbeddingsSize},
                                    GetEncryptorInstance());

  // Read embeddings; only last visit will be found because first expired.
  EXPECT_FALSE(sql_database->GetUrlData(/*url_id=*/1).has_value());
  EXPECT_TRUE(sql_database->GetUrlData(/*url_id=*/2).has_value());

  sql_database.reset();
  EXPECT_TRUE(
      base::PathExists(history_dir_.GetPath().Append(kHistoryEmbeddingsName)));
}

}  // namespace history_embeddings
