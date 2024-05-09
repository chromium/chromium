// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_SCHEDULING_EMBEDDER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_SCHEDULING_EMBEDDER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/history_embeddings/embedder.h"

namespace history_embeddings {

// The SchedulingEmbedder wraps another primary embedder and adds scheduling
// control with batching and priorities so that high priority queries can be
// computed as soon as possible. Scheduling is also needed to avoid clogging the
// pipes for a slow remote embedder. Even single pages can take a while, and
// when the model changes, all existing passages need their embeddings
// recomputed, which can take a very long time and should be done at lower
// priority.
class SchedulingEmbedder : public Embedder {
 public:
  SchedulingEmbedder(std::unique_ptr<Embedder> embedder,
                     size_t scheduled_min,
                     size_t scheduled_max);
  ~SchedulingEmbedder() override;

  // Embedder:
  void ComputePassagesEmbeddings(
      PassageKind kind,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;

  void SetOnEmbedderReady(OnEmbedderReadyCallback callback) override;

 private:
  // Invoked after the embedding for the original search query has been
  // computed. Continues processing next query if one is pending.
  void OnQueryEmbeddingComputed(ComputePassagesEmbeddingsCallback callback,
                                std::vector<std::string> query_passages,
                                std::vector<Embedding> query_embedding);

  // Requests the embedder to embed the next query if one is pending.
  void SubmitQueryToEmbedder();

  // Time when last query was submitted, if awaiting an embedder response;
  // or nullopt if no query is currently submitted.
  std::optional<base::Time> query_submission_time_;

  // The next query to submit for embedding. Empty query strings are allowed,
  // so optional is used to determine whether a query is pending.
  std::optional<std::string> next_query_;

  // The callback associated with `next_query_` is also saved until it's
  // submitted to the embedder.
  ComputePassagesEmbeddingsCallback next_query_callback_;

  // The primary embedder that does the actual embedding computations.
  // This may be slow, and we await results before sending the next request.
  std::unique_ptr<Embedder> embedder_;

  // The minimum and maximum number of embeddings to submit to the primary
  // embedder via the scheduling embedder. Controlling these allows embedding
  // computations to be either batched together or broken down as needed.
  size_t scheduled_min_;
  size_t scheduled_max_;

  base::WeakPtrFactory<SchedulingEmbedder> weak_ptr_factory_{this};
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_SCHEDULING_EMBEDDER_H_
