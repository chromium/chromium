// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_TEST_UTIL_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace passage_embeddings {

inline constexpr int64_t kEmbeddingsModelVersion = 1l;

// Returns a model info builder preloaded with valid model info.
optimization_guide::TestModelInfoBuilder GetBuilderWithValidModelInfo();

////////////////////////////////////////////////////////////////////////////////

// An Embedder that generates Embeddings asynchronously.
class TestEmbedder : public Embedder {
 public:
  TestEmbedder();
  ~TestEmbedder() override;

  // Embedder:
  Job ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;
  base::WeakPtr<Embedder> GetWeakPtr() override;

 protected:
  void ReprioritizeJobs(PassagePriority priority,
                        const std::set<uint64_t>& job_ids) override;
  bool TryCancel(uint64_t job_id) override;

 private:
  uint64_t next_job_id_ = 1;
  base::WeakPtrFactory<TestEmbedder> weak_ptr_factory_{this};
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

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_TEST_UTIL_H_
