// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_IN_MEMORY_TEXT_EMBEDDING_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_IN_MEMORY_TEXT_EMBEDDING_MANAGER_H_

#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "url/gurl.h"

namespace optimization_guide {

// Contains visit information along with its associated embedding.
struct InMemoryTextEmbedding {
  InMemoryTextEmbedding();
  InMemoryTextEmbedding(GURL url,
                        std::string page_title,
                        base::Time visit_time,
                        std::vector<float> embedding);
  ~InMemoryTextEmbedding();
  InMemoryTextEmbedding(const InMemoryTextEmbedding&);

  GURL url;
  std::string page_title;
  base::Time visit_time;
  std::vector<float> embedding;
};

// An EmbeddingManager that stores information from page visits and queries
// embeddings.
class InMemoryTextEmbeddingManager {
 public:
  InMemoryTextEmbeddingManager();
  ~InMemoryTextEmbeddingManager();

  InMemoryTextEmbeddingManager(const InMemoryTextEmbeddingManager&) = delete;
  InMemoryTextEmbeddingManager& operator=(const InMemoryTextEmbeddingManager&) =
      delete;

  // Creates a new instance of InMemoryTextEmbedding and adds to
  // |in_memory_text_embeddings_|.
  void AddEmbeddingForVisit(
      const GURL& url,
      const std::string& page_title,
      const base::Time& visit_time,
      const absl::optional<std::vector<float>>& embedding);

  // Returns QueryResults based on vector similarity between
  // |in_memory_text_embeddings_| and the input embedding when embeddings are
  // queried.
  history::QueryResults QueryEmbeddings(const std::vector<float>& embedding);

 private:
  std::vector<InMemoryTextEmbedding> in_memory_text_embeddings_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_IN_MEMORY_TEXT_EMBEDDING_MANAGER_H_
