// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TEST_UTIL_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace passage_embeddings {

inline constexpr int64_t kEmbeddingsModelVersion = 1l;
inline constexpr size_t kEmbeddingsModelOutputSize = 768ul;

// Returns a model info builder preloaded with valid model info.
optimization_guide::TestModelInfoBuilder GetBuilderWithValidModelInfo();

// Returns valid Embeddings for the given passages.
std::vector<Embedding> ComputeEmbeddingsForPassages(
    const std::vector<std::string>& passages);

////////////////////////////////////////////////////////////////////////////////

// An Embedder that generates Embeddings asynchronously.
class TestEmbedder : public Embedder {
 public:
  TestEmbedder() = default;
  ~TestEmbedder() override = default;

  // Embedder:
  TaskId ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;
  void ReprioritizeTasks(PassagePriority priority,
                         const std::set<TaskId>& tasks) override;
  bool TryCancel(TaskId task_id) override;
};

////////////////////////////////////////////////////////////////////////////////

// An EmbedderMetadataProvider that notifies the observer immediately with valid
// embedder metadata.
class TestEmbedderMetadataProvider : public EmbedderMetadataProvider {
 public:
  TestEmbedderMetadataProvider();
  ~TestEmbedderMetadataProvider() override;

  // EmbedderMetadataProvider:
  void AddObserver(EmbedderMetadataObserver* observer) override;
  void RemoveObserver(EmbedderMetadataObserver* observer) override;

 private:
  base::ObserverList<EmbedderMetadataObserver> observer_list_;
};

////////////////////////////////////////////////////////////////////////////////

// The TestEnvironment that encapsulates test helper instances.
class TestEnvironment {
 public:
  TestEnvironment();
  ~TestEnvironment();

  Embedder* embedder() { return embedder_.get(); }
  EmbedderMetadataProvider* embedder_metadata_provider() {
    return embedder_metadata_provider_.get();
  }

 private:
  std::unique_ptr<TestEmbedder> embedder_;
  std::unique_ptr<TestEmbedderMetadataProvider> embedder_metadata_provider_;
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_TEST_UTIL_H_
