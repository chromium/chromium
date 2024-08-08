// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <atomic>
#include <memory>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

namespace {

Embedding RandomEmbedding() {
  constexpr size_t kSize = 768u;
  std::vector<float> random_vector(kSize, 0.0f);
  for (float& v : random_vector) {
    v = base::RandFloat();
  }
  Embedding embedding(std::move(random_vector));
  embedding.Normalize();
  return embedding;
}

Embedding DeterministicEmbedding(float value) {
  constexpr size_t kSize = 768u;
  std::vector<float> vector(kSize, 0.0f);
  vector[0] = 1;
  vector[1] = value;
  Embedding embedding(std::move(vector));
  embedding.Normalize();
  embedding.SetPassageWordCount(10);
  return embedding;
}

}  // namespace

TEST(HistoryEmbeddingsVectorDatabaseTest, Constructs) {
  std::make_unique<VectorDatabaseInMemory>();
}

TEST(HistoryEmbeddingsVectorDatabaseTest, EmbeddingOperations) {
  Embedding a({1, 1, 1});
  EXPECT_FLOAT_EQ(a.Magnitude(), std::sqrt(3));

  a.Normalize();
  EXPECT_FLOAT_EQ(a.Magnitude(), 1.0f);

  Embedding b({2, 2, 2});
  b.Normalize();
  SearchInfo search_info;
  EXPECT_FLOAT_EQ(a.ScoreWith(search_info, "", b), 1.0f);
  EXPECT_FLOAT_EQ(a.ScoreWith(search_info, "this is an ASCII string", b), 1.0f);
  EXPECT_EQ(search_info.skipped_nonascii_passage_count, 0u);
  EXPECT_FLOAT_EQ(
      a.ScoreWith(search_info, "this is non-ASCII string and scores ∅", b),
      0.0f);
  EXPECT_EQ(search_info.skipped_nonascii_passage_count, 1u);

  // Verify more similar embeddings have higher scores.
  EXPECT_GT(DeterministicEmbedding(5).ScoreWith(search_info, "",
                                                DeterministicEmbedding(4)),
            DeterministicEmbedding(5).ScoreWith(search_info, "",
                                                DeterministicEmbedding(3)));

  EXPECT_GT(DeterministicEmbedding(5).ScoreWith(search_info, "",
                                                DeterministicEmbedding(6)),
            DeterministicEmbedding(5).ScoreWith(search_info, "",
                                                DeterministicEmbedding(7)));
}

TEST(HistoryEmbeddingsVectorDatabaseTest, FindNearest) {
  VectorDatabaseInMemory database;
  for (size_t i = 0; i < 10; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, base::Time::Now());
    url_data.url_passages.passages.add_passages("some deterministic passage");
    url_data.url_embeddings.embeddings.push_back(DeterministicEmbedding(i));
    database.AddUrlData(url_data);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, DeterministicEmbedding(0),
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_THAT(scored_urls,
                testing::ElementsAre(testing::Field(&ScoredUrl::url_id, 1),
                                     testing::Field(&ScoredUrl::url_id, 2),
                                     testing::Field(&ScoredUrl::url_id, 3)));
  }

  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, DeterministicEmbedding(20),
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_THAT(scored_urls,
                testing::ElementsAre(testing::Field(&ScoredUrl::url_id, 10),
                                     testing::Field(&ScoredUrl::url_id, 9),
                                     testing::Field(&ScoredUrl::url_id, 8)));
  }
}

TEST(HistoryEmbeddingsVectorDatabaseTest, SearchCanBeHaltedEarly) {
  VectorDatabaseInMemory database;
  for (size_t i = 0; i < 3; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, base::Time::Now());
    for (size_t j = 0; j < 3; j++) {
      url_data.url_passages.passages.add_passages("a random passage");
      url_data.url_embeddings.embeddings.push_back(RandomEmbedding());
    }
    database.AddUrlData(url_data);
  }
  Embedding query = RandomEmbedding();

  // An ordinary search with full results:
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 3u);
  }

  // A halted search with fewer results:
  {
    std::atomic<size_t> counter(0u);
    base::WeakPtrFactory<std::atomic<size_t>> weak_factory(&counter);
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, query,
                         base::BindRepeating(
                             [](auto weak_counter) {
                               (*weak_counter)++;
                               return *weak_counter > 2u;
                             },
                             weak_factory.GetWeakPtr()))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 2u);
  }
}

TEST(HistoryEmbeddingsVectorDatabaseTest, TimeRangeNarrowsSearchResult) {
  const base::Time now = base::Time::Now();
  VectorDatabaseInMemory database;
  for (size_t i = 0; i < 3; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, now + base::Minutes(i));
    for (size_t j = 0; j < 3; j++) {
      url_data.url_passages.passages.add_passages("some random passage");
      url_data.url_embeddings.embeddings.push_back(RandomEmbedding());
    }
    database.AddUrlData(url_data);
  }
  Embedding query = RandomEmbedding();

  // An ordinary search with full results:
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 3u);
  }

  // Narrowed searches with time range.
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now, 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 3u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(30), 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 2u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(90), 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 1u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Minutes(2), 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 1u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(121), 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 0u);
  }
}

// Note: Disabled by default so as to not burden the bots. Enable when needed.
TEST(HistoryEmbeddingsVectorDatabaseTest, DISABLED_ManyVectorsAreFastEnough) {
  VectorDatabaseInMemory database;
  size_t count = 0;
  // Estimate for expected URL count...
  for (size_t i = 0; i < 15000; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, base::Time::Now());
    // Times 3 embeddings each, on average.
    for (size_t j = 0; j < 3; j++) {
      url_data.url_passages.passages.add_passages("one of many passages");
      url_data.url_embeddings.embeddings.push_back(RandomEmbedding());
      count++;
    }
    database.AddUrlData(url_data);
  }
  Embedding query = RandomEmbedding();
  base::ElapsedTimer timer;

  // Since inner loop atomic checks can impact performance, simulate that here.
  std::atomic<size_t> id(0u);
  base::WeakPtrFactory<std::atomic<size_t>> weak_factory(&id);
  database.FindNearest(
      {}, 3, query,
      base::BindRepeating(
          [](auto weak_id) { return !weak_id || *weak_id != 0u; },
          weak_factory.GetWeakPtr()));

  // This could be an assertion with an extraordinarily high threshold, but for
  // now we avoid any possibility of blowing up trybots and just need the info.
  LOG(INFO) << "Searched " << count << " embeddings in " << timer.Elapsed();
}

}  // namespace history_embeddings
