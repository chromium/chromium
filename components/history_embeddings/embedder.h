// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace history_embeddings {

class Embedding;

// The kind of passage may be specified as a hint for prioritization and
// control of compute processing.
enum class PassageKind {
  // Queries are given top priority and should be computed as quickly
  // as possible.
  QUERY,

  // Passages for new live page visits are next. Performance is not as critical
  // as for queries.
  PAGE_VISIT_PASSAGE,

  // Rebuilding deleted embeddings from previously stored passages takes lowest
  // priority and should be computed economically to avoid overtaxing
  // processors when a large database rebuild is needed.
  REBUILD_PASSAGE,
};

using ComputePassagesEmbeddingsCallback = base::OnceCallback<void(
    std::vector<std::string> passages,
    std::vector<Embedding> embeddings,
    passage_embeddings::ComputeEmbeddingsStatus status)>;
using OnEmbedderReadyCallback =
    base::OnceCallback<void(passage_embeddings::EmbedderMetadata metadata)>;

// Base class that hides implementation details for how text is embedded.
class Embedder {
 public:
  virtual ~Embedder() = default;

  // Computes embeddings for each entry in `passages`. Will invoke callback on
  // done. If successful, it is guaranteed that the number of passages in
  // `passages` will match the number of entries in the embeddings vector and in
  // the same order. If unsuccessful, the callback will still return the
  // original passages but an empty embeddings vector.
  virtual void ComputePassagesEmbeddings(
      PassageKind kind,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) = 0;

  // Set the callback to run when the embedder is ready to process requests.
  // The callback is invoked immediately if the embedder is ready beforehand.
  virtual void SetOnEmbedderReady(OnEmbedderReadyCallback callback) = 0;

 protected:
  Embedder() = default;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_
