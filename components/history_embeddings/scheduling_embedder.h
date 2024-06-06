// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_SCHEDULING_EMBEDDER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_SCHEDULING_EMBEDDER_H_

#include <memory>
#include <optional>
#include <queue>
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
                     size_t scheduled_max);
  ~SchedulingEmbedder() override;

  // Embedder:
  void ComputePassagesEmbeddings(
      PassageKind kind,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;

  void SetOnEmbedderReady(OnEmbedderReadyCallback callback) override;

 private:
  // A job consists of multiple passages, and each passage must have its
  // embedding computed. When all are finished, the job is done and its
  // callback will be invoked. Multiple jobs may be batched together when
  // when submitting work to the `embedder_`, and jobs can also be broken
  // down so that partial progress is made across multiple work submissions.
  struct Job {
    Job(PassageKind kind,
        std::vector<std::string> passages,
        ComputePassagesEmbeddingsCallback callback);
    ~Job();
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    Job(Job&&);
    Job& operator=(Job&&);

    // Data for the job is saved from calls to `ComputePassagesEmbeddings`.
    PassageKind kind;
    std::vector<std::string> passages;
    ComputePassagesEmbeddingsCallback callback;

    // Completed embeddings; may be partial.
    std::vector<Embedding> embeddings;
  };

  // Intercepts metadata so that work can be queued up while the primary
  // embedder isn't ready. For the MlEmbedder, this avoids failing when the
  // model hasn't loaded yet. We just wait until it's ready, then start work.
  void OnEmbedderReady(OnEmbedderReadyCallback callback,
                       EmbedderMetadata metadata);

  // Invoked after the embedding for the current job has been computed.
  // Continues processing next job if one is pending.
  void OnEmbeddingsComputed(std::vector<std::string> passages,
                            std::vector<Embedding> embedding,
                            ComputeEmbeddingsStatus status);

  // Stable-sort jobs by priority and submit a batch of work to embedder.
  // This should only be called when the embedder is not already working.
  void SubmitWorkToEmbedder();

  // When this is non-empty, the embedder is working and its results will be
  // applied from front to back when `OnEmbeddingsComputed` is called. Not all
  // of these jobs are necessarily being worked on by the embedder. It may
  // contain a mix of in-progress, partially completed, and not-yet-started
  // jobs. In-progress jobs are ordered first, and in the same order as
  // submitted to the embedder. Partially completed jobs may follow,
  // still in the order they were last submitted to the embedder.
  // Not-yet-started jobs are ordered last. All jobs will be re-ordered by
  // priority before submitting the next batch to the embedder.
  std::deque<Job> jobs_;

  // The primary embedder that does the actual embedding computations.
  // This may be slow, and we await results before sending the next request.
  std::unique_ptr<Embedder> embedder_;

  // Starts false; set true when valid metadata is received from `embedder_`.
  bool embedder_ready_{false};

  // The maximum number of embeddings to submit to the primary embedder.
  size_t scheduled_max_;

  base::WeakPtrFactory<SchedulingEmbedder> weak_ptr_factory_{this};
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_SCHEDULING_EMBEDDER_H_
