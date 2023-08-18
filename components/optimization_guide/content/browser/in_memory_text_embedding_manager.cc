// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/in_memory_text_embedding_manager.h"

namespace optimization_guide {

InMemoryTextEmbedding::InMemoryTextEmbedding() = default;

InMemoryTextEmbedding::InMemoryTextEmbedding(GURL url,
                                             std::string page_title,
                                             base::Time visit_time,
                                             std::vector<float> embedding) {
  this->url = url;
  this->page_title = page_title;
  this->visit_time = visit_time;
  this->embedding = embedding;
}

InMemoryTextEmbedding::~InMemoryTextEmbedding() = default;
InMemoryTextEmbedding::InMemoryTextEmbedding(const InMemoryTextEmbedding&) =
    default;

InMemoryTextEmbeddingManager::InMemoryTextEmbeddingManager() = default;

InMemoryTextEmbeddingManager::~InMemoryTextEmbeddingManager() = default;

void InMemoryTextEmbeddingManager::AddEmbeddingForVisit(
    const GURL& url,
    const std::string& page_title,
    const base::Time& visit_time,
    const absl::optional<std::vector<float>>& embedding) {
  if (!embedding.has_value()) {
    return;
  }

  InMemoryTextEmbedding in_memory_text_embedding(url, page_title, visit_time,
                                                 embedding.value());

  in_memory_text_embeddings_.push_back(in_memory_text_embedding);
}

history::QueryResults InMemoryTextEmbeddingManager::QueryEmbeddings(
    const std::vector<float>& embedding) {
  history::QueryResults query_results;

  if (in_memory_text_embeddings_.empty() || embedding.empty()) {
    return query_results;
  }

  std::vector<std::pair<size_t, float>> sorted_text_embeddings;

  // Iterate through |in_memory_text_embeddings_| and find the dot
  // product of each embedding from |in_memory_text_embeddings_| with the input
  // embedding. Add the result to |sorted_text_embeddings|, which will hold the
  // index to |in_memory_text_embeddings_| paired with its similarity score.
  for (size_t in_memory_text_embeddings_indx = 0;
       in_memory_text_embeddings_indx < in_memory_text_embeddings_.size();
       in_memory_text_embeddings_indx++) {
    DCHECK_EQ(in_memory_text_embeddings_[in_memory_text_embeddings_indx]
                  .embedding.size(),
              embedding.size());
    float similarity_score = 0.0;
    for (size_t i = 0; i < embedding.size(); i++) {
      similarity_score +=
          embedding.at(i) *
          in_memory_text_embeddings_[in_memory_text_embeddings_indx]
              .embedding.at(i);
    }
    sorted_text_embeddings.emplace_back(in_memory_text_embeddings_indx,
                                        similarity_score);
  }

  // Sort the vector by similarity score in decreasing order and add each url
  // into a vector of URLResults.
  std::sort(
      sorted_text_embeddings.begin(), sorted_text_embeddings.end(),
      [](const std::pair<size_t, float>& a, const std::pair<size_t, float>& b) {
        return a.second > b.second;
      });

  std::vector<history::URLResult> results;
  for (auto& sorted_text_embedding : sorted_text_embeddings) {
    InMemoryTextEmbedding in_memory_text_embedding =
        in_memory_text_embeddings_.at(sorted_text_embedding.first);
    history::URLResult url_result(in_memory_text_embedding.url,
                                  in_memory_text_embedding.visit_time);
    results.push_back(std::move(url_result));
  }

  query_results.SetURLResults(std::move(results));
  return query_results;
}

}  // namespace optimization_guide
