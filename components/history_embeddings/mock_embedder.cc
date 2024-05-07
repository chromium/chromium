// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/mock_embedder.h"

#include "base/task/sequenced_task_runner.h"
#include "components/history_embeddings/vector_database.h"

namespace history_embeddings {

namespace {

Embedding ComputeEmbeddingForPassage(const std::string& passage) {
  Embedding embedding(std::vector<float>(768, 1.0f));
  embedding.Normalize();
  return embedding;
}

std::vector<Embedding> ComputeEmbeddingsForPassages(
    const std::vector<std::string>& passages) {
  return std::vector<Embedding>(passages.size(),
                                ComputeEmbeddingForPassage(""));
}

}  // namespace

MockEmbedder::MockEmbedder() = default;
MockEmbedder::~MockEmbedder() = default;

void MockEmbedder::ComputePassagesEmbeddings(
    PassageKind kind,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(passages),
                                ComputeEmbeddingsForPassages(passages)));
}

}  // namespace history_embeddings
