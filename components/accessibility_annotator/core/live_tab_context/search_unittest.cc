// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/live_tab_context/search.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using ::testing::SizeIs;

namespace {

const int kMaxSearchResultsForTesting = 3;

class LiveTabContextSearchTest : public testing::Test {
 public:
  LiveTabContextSearchTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kAccessibilityAnnotatorLiveTabContext,
        {{"live_tab_context_max_search_results",
          base::NumberToString(kMaxSearchResultsForTesting)}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that keyword-based matching correctly matches query against passages.
TEST_F(LiveTabContextSearchTest, FindPassagesByKeywordMatching) {
  std::vector<std::string> passages{"filler keyword filler",
                                    "filler filler filler"};
  std::vector<ScoredPassage> results = FindPassagesByKeywordMatching(
      /*query=*/u"keyword", passages);
  ASSERT_THAT(results, SizeIs(1));
  EXPECT_EQ(1.0f, results[0].score);
  EXPECT_EQ(results[0].passage, u"filler keyword filler");
}

// Tests that keyword-based matching truncates results.
TEST_F(LiveTabContextSearchTest, FindPassagesByKeywordMatching_Truncation) {
  // Initialize more passages than max search results.
  const size_t num_passages = kMaxSearchResultsForTesting + 10;
  std::vector<std::string> passages;
  for (size_t i = 0; i < num_passages; ++i) {
    passages.push_back("keyword " + base::NumberToString(i));
  }

  // Don't return more than max search results.
  std::vector<ScoredPassage> results = FindPassagesByKeywordMatching(
      /*query=*/u"keyword", passages);
  EXPECT_THAT(results, SizeIs(kMaxSearchResultsForTesting));
}

// Tests that keyword-based matching returns empty if no passages are provided.
TEST_F(LiveTabContextSearchTest, FindPassagesByKeywordMatching_EmptyPassages) {
  std::vector<ScoredPassage> results = FindPassagesByKeywordMatching(
      /*query=*/u"keyword", /*passages=*/{});
  EXPECT_THAT(results, SizeIs(0));
}

// Tests that semantic similarity ranks passages by relevance to query.
TEST_F(LiveTabContextSearchTest, RankPassagesBySemanticSimilarity) {
  // Create query embedding [1, 0, 0]
  passage_embeddings::Embedding query_embedding({1.0f, 0.0f, 0.0f});

  // Create page embedding close to query embedding.
  std::vector<page_content_annotations::PassageEmbedding> page_embeddings;
  page_embeddings.emplace_back(
      std::make_pair("semantic passage similar",
                     page_content_annotations::kPageContent),
      passage_embeddings::Embedding({0.9f, 0.1f, 0.0f}));

  // Create page embedding further from query embedding.
  page_embeddings.emplace_back(
      std::make_pair("semantic passage less similar",
                     page_content_annotations::kPageContent),
      passage_embeddings::Embedding({0.8f, 0.2f, 0.0f}));

  // Run semantic similarity as usual.
  std::vector<ScoredPassage> results =
      RankPassagesBySemanticSimilarity(query_embedding, page_embeddings);

  ASSERT_THAT(results, SizeIs(2));
  EXPECT_FLOAT_EQ(0.9f, results[0].score);
  EXPECT_EQ(u"semantic passage similar", results[0].passage);
  EXPECT_FLOAT_EQ(0.8f, results[1].score);
  EXPECT_EQ(u"semantic passage less similar", results[1].passage);
}

// Tests that semantic similarity truncates results.
TEST_F(LiveTabContextSearchTest, RankPassagesBySemanticSimilarity_Truncation) {
  // Create query embedding [1, 1, 1]
  passage_embeddings::Embedding query_embedding({1.0f, 1.0f, 1.0f});
  std::vector<page_content_annotations::PassageEmbedding> page_embeddings;

  // Create enough page embeddings to truncate.
  const size_t num_passages = kMaxSearchResultsForTesting + 10;
  for (size_t i = 0; i < num_passages; ++i) {
    page_embeddings.emplace_back(
        std::make_pair("passage " + base::NumberToString(i),
                       page_content_annotations::kPageContent),
        passage_embeddings::Embedding({1.0f, 1.0f, 1.0f}));
  }

  // Don't return more than max search results.
  std::vector<ScoredPassage> results =
      RankPassagesBySemanticSimilarity(query_embedding, page_embeddings);
  EXPECT_THAT(results, SizeIs(kMaxSearchResultsForTesting));
}

// Tests that semantic similarity maintains original ordering for equal scores.
TEST_F(LiveTabContextSearchTest, RankPassagesBySemanticSimilarity_TieBreaker) {
  // Create query embedding [1, 0, 0]
  passage_embeddings::Embedding query_embedding({1.0f, 0.0f, 0.0f});

  // Create page embeddings with identical vectors.
  std::vector<page_content_annotations::PassageEmbedding> page_embeddings;
  for (int i = 0; i < 3; ++i) {
    page_embeddings.emplace_back(
        std::make_pair("passage " + base::NumberToString(i),
                       page_content_annotations::kPageContent),
        passage_embeddings::Embedding({0.5f, 0.0f, 0.0f}));
  }

  std::vector<ScoredPassage> results =
      RankPassagesBySemanticSimilarity(query_embedding, page_embeddings);

  ASSERT_THAT(results, SizeIs(kMaxSearchResultsForTesting));
  // Since all scores are equal, the original order should be preserved.
  EXPECT_EQ(u"passage 0", results[0].passage);
  EXPECT_EQ(u"passage 1", results[1].passage);
  EXPECT_EQ(u"passage 2", results[2].passage);
}

}  // namespace

}  // namespace accessibility_annotator
