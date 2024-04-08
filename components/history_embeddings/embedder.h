// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace history_embeddings {

class Embedding;

using ComputePassagesEmbeddingsCallback =
    base::OnceCallback<void(std::vector<std::string> passages,
                            std::vector<Embedding>)>;

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
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) = 0;

 protected:
  Embedder() = default;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_EMBEDDER_H_
