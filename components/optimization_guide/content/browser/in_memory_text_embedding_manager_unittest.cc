// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "components/optimization_guide/content/browser/in_memory_text_embedding_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {

class InMemoryTextEmbeddingManagerTest : public testing::Test {
 public:
  InMemoryTextEmbeddingManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kQueryInMemoryTextEmbeddings);
  }

  ~InMemoryTextEmbeddingManagerTest() override = default;

  void SetUp() override {
    text_embedding_manager_ = std::make_unique<InMemoryTextEmbeddingManager>();
  }

  std::unique_ptr<InMemoryTextEmbeddingManager> text_embedding_manager_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(InMemoryTextEmbeddingManagerTest, DoNotQueryInvalidEmbeddings) {
  // Create InMemoryTextEmbeddings.
  text_embedding_manager_->AddEmbeddingForVisit(
      GURL("https://cat.com"), "cat", base::Time::Now(), absl::nullopt);
  text_embedding_manager_->AddEmbeddingForVisit(
      GURL("https://dog.com"), "dog", base::Time::Now(), absl::nullopt);

  // Query embeddings.
  std::vector<float> input_embedding = {6.6554, 4.7054, 9.8516, 2.589, 5.8918};
  history::QueryResults query_results =
      text_embedding_manager_->QueryEmbeddings(input_embedding);

  EXPECT_TRUE(query_results.empty());
}

TEST_F(InMemoryTextEmbeddingManagerTest, QueryValidEmbeddings) {
  // Create InMemoryTextEmbeddings.
  std::vector<float> cat_embedding = {5.9957, 0.9872, 2.3524, 6.0717, 4.9405};
  text_embedding_manager_->AddEmbeddingForVisit(
      GURL("https://cat.com"), "cat", base::Time::Now(), cat_embedding);
  std::vector<float> dog_embedding = {2.2856, 1.2177, 1.5583, 7.5789, 7.2837};
  text_embedding_manager_->AddEmbeddingForVisit(
      GURL("https://dog.com"), "dog", base::Time::Now(), dog_embedding);

  // Query embeddings.
  std::vector<float> input_embedding = {6.6554, 4.7054, 9.8516, 2.589, 5.8918};
  history::QueryResults query_results =
      text_embedding_manager_->QueryEmbeddings(input_embedding);

  EXPECT_EQ(query_results.size(), 2U);
  EXPECT_EQ(query_results[0].url(), GURL("https://cat.com"));
  EXPECT_EQ(query_results[1].url(), GURL("https://dog.com"));
}

}  // namespace optimization_guide
