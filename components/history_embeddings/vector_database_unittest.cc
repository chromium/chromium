// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <memory>

#include "base/logging.h"
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
  std::make_unique<VectorDatabase>();
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

// Note: Disabled by default so as to not burden the bots. Enable when needed.
TEST(HistoryEmbeddingsVectorDatabaseTest, DISABLED_ManyVectorsAreFastEnough) {
  VectorDatabase database;
  size_t count = 0;
  // 95th percentile for URL count
  for (size_t i = 0; i < 15000; i++) {
    UrlEmbeddings url_embeddings;
    // Times 3 embeddings each, on average
    for (size_t j = 0; j < 3; j++) {
      url_embeddings.embeddings.push_back(RandomEmbedding());
      count++;
    }
    database.Add(std::move(url_embeddings));
  }
  Embedding query = RandomEmbedding();
  base::ElapsedTimer timer;
  database.FindNearest(3, query);
  // This could be an assertion with an extraordinarily high threshold, but for
  // now we avoid any possibility of blowing up trybots and just need the info.
  LOG(INFO) << "Searched " << count << " embeddings in " << timer.Elapsed();
}

}  // namespace history_embeddings
