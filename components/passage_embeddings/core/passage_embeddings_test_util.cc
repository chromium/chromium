// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/core/passage_embeddings_test_util.h"

#include "base/task/sequenced_task_runner.h"

namespace passage_embeddings {

namespace {

EmbedderMetadata GetValidEmbedderMetadata() {
  return EmbedderMetadata(kEmbeddingsModelVersion, 3ul);
}

}  // namespace

TestEmbedder::TestEmbedder() = default;
TestEmbedder::~TestEmbedder() = default;

Embedder::Job TestEmbedder::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  uint64_t job_id = next_job_id_++;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::vector<std::string> passages, uint64_t job_id,
                        ComputePassagesEmbeddingsCallback callback) {
                       std::vector<Embedding> embeddings(
                           passages.size(), Embedding({1.0f, 0.0f, 0.0f}));
                       std::move(callback).Run(
                           passages, std::move(embeddings), job_id,
                           ComputeEmbeddingsStatus::kSuccess);
                     },
                     passages, job_id, std::move(callback)));
  return Embedder::Job(weak_ptr_factory_.GetWeakPtr(), job_id);
}

base::WeakPtr<Embedder> TestEmbedder::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TestEmbedder::ReprioritizeJobs(PassagePriority priority,
                                    const std::set<uint64_t>& job_ids) {}

bool TestEmbedder::TryCancel(uint64_t job_id) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////

TestEmbedderMetadataProvider::TestEmbedderMetadataProvider() = default;
TestEmbedderMetadataProvider::~TestEmbedderMetadataProvider() = default;

void TestEmbedderMetadataProvider::AddObserver(
    EmbedderMetadataObserver* observer) {
  observer->EmbedderMetadataUpdated(GetValidEmbedderMetadata());
  observer_list_.AddObserver(observer);
}
void TestEmbedderMetadataProvider::RemoveObserver(
    EmbedderMetadataObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////

TestEnvironment::TestEnvironment()
    : embedder_(std::make_unique<TestEmbedder>()),
      embedder_metadata_provider_(
          std::make_unique<TestEmbedderMetadataProvider>()) {}

TestEnvironment::~TestEnvironment() = default;

}  // namespace passage_embeddings
