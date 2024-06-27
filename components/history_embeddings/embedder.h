// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"

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

// The status of an embeddings generation attempt.
enum class ComputeEmbeddingsStatus {
  // Embeddings are generated successfully.
  SUCCESS,

  // The model files required for generation are not available .
  MODEL_UNAVAILABLE,

  // Failure occurred during model execution.
  EXECUTION_FAILURE,

  // The generation request was skipped. This could happen if the embeddings
  // request for a user query, which may have been obsolete (by a newer user
  // query) by the time the embedder is free.
  SKIPPED,
};

struct EmbedderMetadata {
  EmbedderMetadata(int64_t model_version,
                   size_t output_size,
                   std::optional<double> search_score_threshold = std::nullopt)
      : model_version(model_version),
        output_size(output_size),
        search_score_threshold(search_score_threshold) {}

  int64_t model_version;
  size_t output_size;
  std::optional<double> search_score_threshold;
};

using ComputePassagesEmbeddingsCallback =
    base::OnceCallback<void(std::vector<std::string> passages,
                            std::vector<Embedding> embeddings,
                            ComputeEmbeddingsStatus status)>;
using OnEmbedderReadyCallback =
    base::OnceCallback<void(EmbedderMetadata metadata)>;

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
