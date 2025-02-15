// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_ML_EMBEDDER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_ML_EMBEDDER_H_

#include "base/memory/raw_ptr.h"
#include "components/passage_embeddings/embedder.h"

namespace passage_embeddings {

class PassageEmbeddingsServiceController;

// An embedder that returns embeddings from a machine learning model.
class MlEmbedder : public Embedder {
 public:
  explicit MlEmbedder(PassageEmbeddingsServiceController* service_controller);
  ~MlEmbedder() override;

  // Embedder:
  TaskId ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;

  bool TryCancel(TaskId task_id) override;

 private:
  // The controller used to interact with the PassageEmbeddingsService.
  // It is a singleton and guaranteed not to be nullptr.
  raw_ptr<PassageEmbeddingsServiceController> service_controller_;
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_ML_EMBEDDER_H_
