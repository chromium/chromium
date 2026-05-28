// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/core/passage_embeddings_types.h"

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "base/not_fatal_until.h"

namespace passage_embeddings {

namespace {

float GetMagnitude(const std::vector<float>& data) {
  float sum = 0.0f;
  for (float s : data) {
    sum += s * s;
  }
  return std::sqrt(sum);
}

}  // namespace

Embedding::Embedding(std::vector<float> data) : data_(std::move(data)) {
  CHECK(!data_.empty(), base::NotFatalUntil::M152);
  DCHECK_LT(std::abs(GetMagnitude(data_) - 1.0f), 0.0001f);
}

Embedding::~Embedding() = default;
Embedding::Embedding(const Embedding&) = default;
Embedding& Embedding::operator=(const Embedding&) = default;
Embedding::Embedding(Embedding&&) = default;
Embedding& Embedding::operator=(Embedding&&) = default;

// static
std::optional<std::vector<float>> Embedding::Normalize(
    std::vector<float> data) {
  float magnitude = GetMagnitude(data);
  if (magnitude <= std::numeric_limits<float>::epsilon()) {
    return std::nullopt;
  }
  for (float& v : data) {
    v /= magnitude;
  }
  return data;
}

float Embedding::ScoreWith(const Embedding& other_embedding) const {
  // This check is redundant since the database layers ensure embeddings
  // always have a fixed consistent size, but code can change with time,
  // and being sure directly before use may eventually catch a bug.
  DCHECK_EQ(data_.size(), other_embedding.data_.size());

  float embedding_score = 0.0f;
  for (size_t i = 0; i < data_.size(); i++) {
    embedding_score += data_[i] * other_embedding.data_[i];
  }
  return embedding_score;
}

Embedder::Embedder() = default;

Embedder::Job::Job(base::WeakPtr<Embedder> embedder, TaskId task_id)
    : embedder_(std::move(embedder)), task_id_(task_id) {
  DCHECK_NE(task_id_, 0u);
}

Embedder::Job::Job(Job&& other)
    : embedder_(std::move(other.embedder_)), task_id_(other.task_id_) {
  other.task_id_ = 0;
}

Embedder::Job& Embedder::Job::operator=(Job&& other) {
  if (this != &other) {
    if (task_id_ != 0) {
      // Job handles must not outlive the Embedder that generated them.
      CHECK(embedder_);
      embedder_->TryCancel(task_id_);
    }
    embedder_ = std::move(other.embedder_);
    task_id_ = other.task_id_;
    other.task_id_ = 0;
  }
  return *this;
}

Embedder::Job::~Job() {
  if (task_id_ != 0) {
    // Job handles must not outlive the Embedder that generated them.
    CHECK(embedder_);
    embedder_->TryCancel(task_id_);
  }
}

void Embedder::Job::Reprioritize(PassagePriority priority) {
  DCHECK_NE(task_id_, 0u);
  // Job handles must not outlive the Embedder that generated them.
  CHECK(embedder_);
  embedder_->ReprioritizeTasks(priority, {task_id_});
}

bool Embedder::JobTaskIdComparator::operator()(const Job& a,
                                               const Job& b) const {
  return a.task_id() < b.task_id();
}

bool Embedder::JobTaskIdComparator::operator()(const Job& a, TaskId b) const {
  return a.task_id() < b;
}

bool Embedder::JobTaskIdComparator::operator()(TaskId a, const Job& b) const {
  return a < b.task_id();
}

}  // namespace passage_embeddings
