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
  EXPECT_FLOAT_EQ(a.ScoreWith(b), 1.0f);
}

TEST(HistoryEmbeddingsVectorDatabaseTest, SearchCanBeHaltedEarly) {
  VectorDatabaseInMemory database;
  for (size_t i = 0; i < 3; i++) {
    UrlEmbeddings url_embeddings(i + 1, i + 1, base::Time::Now());
    for (size_t j = 0; j < 3; j++) {
      url_embeddings.embeddings.push_back(RandomEmbedding());
    }
    database.AddUrlEmbeddings(std::move(url_embeddings));
  }
  Embedding query = RandomEmbedding();

  // An ordinary search with full results:
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 3u);
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
    CHECK_EQ(scored_urls.size(), 2u);
  }
}

TEST(HistoryEmbeddingsVectorDatabaseTest, TimeRangeNarrowsSearchResult) {
  const base::Time now = base::Time::Now();
  VectorDatabaseInMemory database;
  for (size_t i = 0; i < 3; i++) {
    UrlEmbeddings url_embeddings(i + 1, i + 1, now + base::Minutes(i));
    for (size_t j = 0; j < 3; j++) {
      url_embeddings.embeddings.push_back(RandomEmbedding());
    }
    database.AddUrlEmbeddings(std::move(url_embeddings));
  }
  Embedding query = RandomEmbedding();

  // An ordinary search with full results:
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 3u);
  }

  // Narrowed searches with time range.
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now, 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 3u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(30), 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 2u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(90), 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 1u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Minutes(2), 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 1u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(121), 3, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    CHECK_EQ(scored_urls.size(), 0u);
  }
}

// Note: Disabled by default so as to not burden the bots. Enable when needed.
TEST(HistoryEmbeddingsVectorDatabaseTest, DISABLED_ManyVectorsAreFastEnough) {
  VectorDatabaseInMemory database;
  size_t count = 0;
  // Estimate for expected URL count...
  for (size_t i = 0; i < 15000; i++) {
    UrlEmbeddings url_embeddings(i + 1, i + 1, base::Time::Now());
    // Times 3 embeddings each, on average.
    for (size_t j = 0; j < 3; j++) {
      url_embeddings.embeddings.push_back(RandomEmbedding());
      count++;
    }
    database.AddUrlEmbeddings(std::move(url_embeddings));
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
