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

    PassageKind kind;
    std::vector<std::string> passages;
    ComputePassagesEmbeddingsCallback callback;

    // Completed embeddings; may be partial.
    std::vector<Embedding> embeddings;
  };
  struct JobComparator {
    bool operator()(const Job& a, const Job& b) { return a.kind > b.kind; }
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

  // Pop top priority job from `waiting_jobs_` and return it.
  Job PopTopJob();

  // Pull jobs into work queue as needed, and submit work to embedder.
  void SubmitWorkToEmbedder();

  // Jobs that haven't started yet, organized by priority.
  // TODO: b/343523145 - Simplify SchedulingEmbedder to use a single dequeue
  //  instead of dequeue + priority_queue. We can append work to the
  //  back of a single dequeue, reorder it in between work submissions,
  //  and then submit work batches from the front of the queue.
  std::priority_queue<Job, std::vector<Job>, JobComparator> waiting_jobs_;

  // Jobs that have started, organized in order of submission to embedder.
  // When this is non-empty, the embedder is working and its results will
  // be applied from front to back when `OnEmbeddingsComputed` is called.
  // Not all jobs in `working_jobs_` are necessarily being worked on by the
  // embedder. `working_jobs_` will contain jobs that were partially
  // completed, since a subset of a job's passages may be sent in each batch.
  std::deque<Job> working_jobs_;

  // The primary embedder that does the actual embedding computations.
  // This may be slow, and we await results before sending the next request.
  std::unique_ptr<Embedder> embedder_;

  // Starts false; set true when valid metadata is received from `embedder_`.
  bool embedder_ready_{false};

  // The minimum and maximum number of embeddings to submit to the primary
  // embedder via the scheduling embedder. Controlling these allows embedding
  // computations to be either batched together or broken down as needed.
  size_t scheduled_min_;
  size_t scheduled_max_;

  base::WeakPtrFactory<SchedulingEmbedder> weak_ptr_factory_{this};
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_SCHEDULING_EMBEDDER_H_
