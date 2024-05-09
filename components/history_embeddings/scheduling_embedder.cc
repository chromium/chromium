// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/scheduling_embedder.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/history_embeddings/vector_database.h"

namespace history_embeddings {

SchedulingEmbedder::SchedulingEmbedder(std::unique_ptr<Embedder> embedder,
                                       size_t scheduled_min,
                                       size_t scheduled_max)
    : embedder_(std::move(embedder)),
      scheduled_min_(scheduled_min),
      scheduled_max_(scheduled_max) {}

SchedulingEmbedder::~SchedulingEmbedder() = default;

void SchedulingEmbedder::ComputePassagesEmbeddings(
    PassageKind kind,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  if (kind == PassageKind::QUERY) {
    CHECK_EQ(passages.size(), 1u);
    std::string& query = passages[0];
    // Only submit this query if there's none in progress.
    bool submit = !query_submission_time_.has_value();
    if (next_query_.has_value()) {
      VLOG(2) << "Dropped pending query '" << next_query_.value()
              << "'. Next query: '" << query << "'";
      next_query_.reset();
      // TODO(b/332394465): Distinguish skipped queries from errors.
      std::move(next_query_callback_).Run({}, {});
    }

    next_query_ = std::move(query);
    next_query_callback_ =
        base::BindOnce(&SchedulingEmbedder::OnQueryEmbeddingComputed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback));

    if (submit) {
      SubmitQueryToEmbedder();
    }
  } else {
    // Immediate pass-through for other passage kinds.
    embedder_->ComputePassagesEmbeddings(kind, std::move(passages),
                                         std::move(callback));
  }
}

void SchedulingEmbedder::SetOnEmbedderReady(OnEmbedderReadyCallback callback) {
  embedder_->SetOnEmbedderReady(std::move(callback));
}

void SchedulingEmbedder::OnQueryEmbeddingComputed(
    ComputePassagesEmbeddingsCallback callback,
    std::vector<std::string> query_passages,
    std::vector<Embedding> query_embeddings) {
  std::move(callback).Run(std::move(query_passages),
                          std::move(query_embeddings));

  // If another query is pending, submit it for embedding.
  query_submission_time_.reset();
  SubmitQueryToEmbedder();
}

void SchedulingEmbedder::SubmitQueryToEmbedder() {
  if (!next_query_.has_value()) {
    return;
  }
  VLOG(2) << "Submitting query to embedder: '" << next_query_.value() << "'";

  // The embedder could call back synchronously and immediately, so be ready.
  std::string query = std::move(next_query_.value());
  auto callback = std::move(next_query_callback_);
  next_query_.reset();
  next_query_callback_.Reset();
  query_submission_time_ = base::Time::Now();
  embedder_->ComputePassagesEmbeddings(PassageKind::QUERY, {std::move(query)},
                                       std::move(callback));
}

}  // namespace history_embeddings
