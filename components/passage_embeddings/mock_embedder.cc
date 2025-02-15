// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/mock_embedder.h"

#include "base/task/sequenced_task_runner.h"

namespace passage_embeddings {

namespace {

constexpr int64_t kModelVersion = 1;
constexpr size_t kOutputSize = 768ul;
constexpr size_t kMockPassageWordCount = 10;

Embedding ComputeEmbeddingForPassage(const std::string& passage) {
  Embedding embedding(std::vector<float>(kOutputSize, 1.0f));
  embedding.Normalize();
  embedding.SetPassageWordCount(kMockPassageWordCount);
  return embedding;
}

}  // namespace

MockEmbedder::MockEmbedder() = default;
MockEmbedder::~MockEmbedder() = default;

Embedder::TaskId MockEmbedder::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(passages),
                     ComputeEmbeddingsForPassages(passages), kInvalidTaskId,
                     ComputeEmbeddingsStatus::kSuccess));
  return kInvalidTaskId;
}

bool MockEmbedder::TryCancel(TaskId task_id) {
  return false;
}

void MockEmbedder::SetOnEmbedderReadyCallback(
    OnEmbedderReadyCallback callback) {
  // The mock embedder is always ready, so we invoke the callback directly.
  std::move(callback).Run({kModelVersion, kOutputSize});
}

std::vector<Embedding> MockEmbedder::ComputeEmbeddingsForPassages(
    const std::vector<std::string>& passages) {
  return std::vector<Embedding>(passages.size(),
                                ComputeEmbeddingForPassage(""));
}

}  // namespace passage_embeddings
