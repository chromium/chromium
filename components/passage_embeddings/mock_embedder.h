// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_MOCK_EMBEDDER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_MOCK_EMBEDDER_H_

#include <string>
#include <vector>

#include "components/passage_embeddings/embedder.h"

namespace passage_embeddings {

class MockEmbedder : public Embedder {
 public:
  MockEmbedder();
  ~MockEmbedder() override;

  // Embedder:
  TaskId ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;

  bool TryCancel(TaskId task_id) override;

  using OnEmbedderReadyCallback = base::OnceCallback<void(EmbedderMetadata)>;
  void SetOnEmbedderReadyCallback(OnEmbedderReadyCallback callback);

 protected:
  std::vector<Embedding> ComputeEmbeddingsForPassages(
      const std::vector<std::string>& passages);
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_MOCK_EMBEDDER_H_
