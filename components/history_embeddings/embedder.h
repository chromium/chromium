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

// Base class that hides implementation details for how text is embedded.
class Embedder {
 public:
  virtual ~Embedder() = default;

  // Computes embeddings for each entry in `passages`. Will invoke callback on
  // done. If successful, it is guaranteed that the number of passages in
  // `passages` will match the number of entries in the embeddings vector and in
  // the same order. If unsuccessful, the callback will still return the
  // original passages but an empty embeddings vector.
  using ComputePassagesEmbeddingsCallback = base::OnceCallback<void(
      std::vector<std::string> passages,
      std::vector<Embedding> embeddings,
      passage_embeddings::ComputeEmbeddingsStatus status)>;
  virtual void ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) = 0;

  // Sets the callback to run when the embedder is ready to process requests.
  // The callback is invoked immediately if the embedder is ready beforehand.
  using OnEmbedderReadyCallback =
      base::OnceCallback<void(passage_embeddings::EmbedderMetadata metadata)>;
  virtual void SetOnEmbedderReady(OnEmbedderReadyCallback callback) = 0;

 protected:
  Embedder() = default;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_
