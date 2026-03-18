// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_EMBEDDINGS_COMMON_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_EMBEDDINGS_COMMON_H_

#include <string>
#include <utility>
#include <vector>

#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace page_content_annotations {

enum EmbeddingPassageType {
  kPageContent,
  kTitle,
  kTitleAndUrl,
};

// A passage from a page along with its computed embedding.
struct PassageEmbedding {
  PassageEmbedding();
  PassageEmbedding(std::pair<std::string, EmbeddingPassageType> passage,
                   passage_embeddings::Embedding embedding);
  PassageEmbedding(const PassageEmbedding&);
  ~PassageEmbedding();

  std::pair<std::string, EmbeddingPassageType> passage;
  passage_embeddings::Embedding embedding;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_EMBEDDINGS_COMMON_H_
