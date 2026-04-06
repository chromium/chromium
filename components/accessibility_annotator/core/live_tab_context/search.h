// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_LIVE_TAB_CONTEXT_SEARCH_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_LIVE_TAB_CONTEXT_SEARCH_H_

#include <string>
#include <vector>

#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace accessibility_annotator {

struct ScoredPassage {
  float score;
  std::u16string passage;
};

// Returns the `page_embeddings` closest to `query_embedding`, sorted by
// relevance.
std::vector<ScoredPassage> RankPassagesBySemanticSimilarity(
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<page_content_annotations::PassageEmbedding>&
        page_embeddings);

// Returns passages that contain `query`.
std::vector<ScoredPassage> FindPassagesByKeywordMatching(
    const std::u16string& query,
    const std::vector<std::string>& passages);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_LIVE_TAB_CONTEXT_SEARCH_H_
