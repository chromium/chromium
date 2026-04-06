// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/live_tab_context/search.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/i18n/string_search.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator {

std::vector<ScoredPassage> RankPassagesBySemanticSimilarity(
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<page_content_annotations::PassageEmbedding>&
        page_embeddings) {
  // A passage embedding and its score, used for sorting.
  struct ScoredPassageEmbedding {
    float score;
    size_t original_index;  // Used for tie-breaking during sorts; not returned.
    raw_ptr<const page_content_annotations::PassageEmbedding> embedding;
  };

  // Step 1: Score passages based on semantic similarity.
  std::vector<ScoredPassageEmbedding> scored_passages;
  scored_passages.reserve(page_embeddings.size());
  for (size_t i = 0; i < page_embeddings.size(); ++i) {
    scored_passages.push_back(ScoredPassageEmbedding{
        .score = query_embedding.ScoreWith(page_embeddings[i].embedding),
        .original_index = i,
        .embedding = &page_embeddings[i]});
  }
  size_t num_results = std::min(
      scored_passages.size(),
      static_cast<size_t>(
          features::kAccessibilityAnnotatorLiveTabContextMaxSearchResults
              .Get()));
  std::ranges::partial_sort(
      scored_passages, scored_passages.begin() + num_results,
      [](const ScoredPassageEmbedding& a, const ScoredPassageEmbedding& b) {
        if (a.score != b.score) {
          return a.score > b.score;
        }
        // Tie-breaker: preserve original ordering to ensure stability.
        return a.original_index < b.original_index;
      });

  // Step 2: Return top passages.
  // TODO(b/493768199): filter out results if score < threshold
  return base::ToVector(base::span(scored_passages).first(num_results),
                        [&](const ScoredPassageEmbedding& scored_passage) {
                          return ScoredPassage{
                              .score = scored_passage.score,
                              .passage = base::UTF8ToUTF16(
                                  scored_passage.embedding->passage.first)};
                        });
}

std::vector<ScoredPassage> FindPassagesByKeywordMatching(
    const std::u16string& query,
    const std::vector<std::string>& passages) {
  // Find relevant passages via keyword matching
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents searcher(query);
  std::vector<ScoredPassage> search_results;
  for (const std::string& passage : passages) {
    std::u16string passage_u16 = base::UTF8ToUTF16(passage);
    if (!searcher.Search(passage_u16,
                         /*match_index=*/nullptr, /*match_length=*/nullptr)) {
      continue;
    }
    // score=1.0 (may tune later)
    search_results.push_back(
        ScoredPassage{.score = 1.0f, .passage = std::move(passage_u16)});
    if (search_results.size() >=
        static_cast<size_t>(
            features::kAccessibilityAnnotatorLiveTabContextMaxSearchResults
                .Get())) {
      break;
    }
  }
  return search_results;
}

}  // namespace accessibility_annotator
