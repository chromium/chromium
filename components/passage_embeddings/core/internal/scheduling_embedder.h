// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_CORE_INTERNAL_SCHEDULING_EMBEDDER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_CORE_INTERNAL_SCHEDULING_EMBEDDER_H_

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace passage_embeddings {

// The SchedulingEmbedder adds scheduling control with batching and priorities
// so that high priority queries can be computed as soon as possible. Scheduling
// is also needed to avoid clogging the pipes for a slow remote embedder. Even
// single pages can take a while, and when the model changes, all existing
// passages need their embeddings recomputed, which can take a very long time
// and should be done at lower priority.
class SchedulingEmbedder
    : public Embedder,
      public EmbedderMetadataObserver,
      public performance_scenarios::PerformanceScenarioObserver {
 public:
  using GetEmbeddingsResultCallback = base::OnceCallback<void(
      std::vector<mojom::PassageEmbeddingsResultPtr> results,
      ComputeEmbeddingsStatus status)>;
  using GetEmbeddingsCallback =
      base::RepeatingCallback<void(std::vector<std::string> passages,
                                   PassagePriority priority,
                                   GetEmbeddingsResultCallback callback)>;
  SchedulingEmbedder(EmbedderMetadataProvider* embedder_metadata_provider,
                     GetEmbeddingsCallback get_embeddings_callback,
                     size_t max_jobs,
                     size_t scheduled_max_batch_size,
                     bool use_performance_scenario,
                     bool execute_for_gemma = false);
  ~SchedulingEmbedder() override;

  // Embedder:
  Embedder::Job ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;
  base::WeakPtr<Embedder> GetWeakPtr() override;

 protected:
  // Embedder:
  void ReprioritizeJobs(PassagePriority priority,
                        const std::set<uint64_t>& job_ids) override;
  bool TryCancel(uint64_t job_id) override;

 private:
  // A job consists of multiple passages, and each passage must have its
  // embedding computed. When all are finished, the job is done and its
  // callback will be invoked. Multiple jobs may be batched together when
  // submitting work to the `embedder_remote_proxy`, and jobs can also be broken
  // down so that partial progress is made across multiple work submissions.
  struct Job {
    Job(PassagePriority priority,
        uint64_t job_id,
        std::vector<std::string> passages,
        ComputePassagesEmbeddingsCallback callback);
    ~Job();
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    Job(Job&&);
    Job& operator=(Job&&);

    // Data for the job is saved from calls to `ComputePassagesEmbeddings`.
    PassagePriority priority;
    uint64_t job_id;
    std::vector<std::string> passages;
    ComputePassagesEmbeddingsCallback callback;

    // Completed embeddings; may be partial.
    std::vector<Embedding> embeddings;

    // Measures total job duration, from creation to completion.
    base::ElapsedTimer timer;
  };

  // `passage_count` consecutive passages of a batch that come from the job with
  // `job_id`. Their results come back in the same positions.
  struct BatchSlice {
    uint64_t job_id;
    size_t passage_count;
  };

  // EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(EmbedderMetadata metadata) override;

  // PerformanceScenarioObserver:
  void OnLoadingScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::LoadingScenario old_scenario,
      performance_scenarios::LoadingScenario new_scenario) override;
  void OnInputScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::InputScenario old_scenario,
      performance_scenarios::InputScenario new_scenario) override;

  // Invoked after the embedding for the current job has been computed.
  // Continues processing next job if one is pending. `batch` lists the slices
  // of the batch in the order their passages were submitted.
  void OnEmbeddingsComputed(
      const std::vector<BatchSlice>& batch,
      std::vector<mojom::PassageEmbeddingsResultPtr> results,
      ComputeEmbeddingsStatus status);

  // Stable-sort jobs by priority and submit a batch of work to embedder.
  // This will only submit new work if the embedder is not already working.
  void SubmitWorkToEmbedder();

  // Returns true if currently in a work ready performance scenario state.
  bool IsPerformanceScenarioReady();

  // Returns an iterator to the highest priority non-empty queue in
  // pending_jobs_, or pending_jobs_.end() if all queues are empty.
  std::map<PassagePriority, std::deque<Job>>::iterator GetFirstNonEmptyQueue();

  // Call the callback with status, etc. and record relevant histograms.
  static void FinishJob(Job job,
                        ComputeEmbeddingsStatus status,
                        bool record_histograms);

  // Posts `job`'s callback with kCanceled and no embeddings, and records its
  // status histograms.
  void CancelJob(Job job);

  // Jobs that are waiting to be scheduled or resumed, grouped by priority.
  std::map<PassagePriority, std::deque<Job>> pending_jobs_;

  // Jobs that have passages in the current in-flight batch, in batch order.
  // TryCancel() may remove any of them, but nothing reorders them.
  // OnEmbeddingsComputed() relies on this order to match results to jobs.
  std::deque<Job> active_jobs_;

  // ID to assign to the next Job.
  uint64_t next_job_id_ = 1;

  // Whether the embedder is currently working on some passages. Note, this
  // is not the same concept as having a job in progress since multiple
  // embedder work submissions may be required to complete a job.
  bool work_submitted_ = false;

  // The callback that does the actual embeddings computations.
  // May be slow; await results before sending the next request.
  GetEmbeddingsCallback get_embeddings_callback_;

  // Metadata about the embedder; Set when valid metadata is received from
  // `embedder_metadata_provider`.
  EmbedderMetadata embedder_metadata_{0, 0};

  // The maximum number of jobs to hold at once. Exceeding the cap
  // will cause job failures on last pending jobs to avoid very high memory use.
  // When the limit is reached, the last pending job is canceled instead of
  // failing to accept the new job so that queries can still be accepted even
  // if the queue is full of lower priority jobs awaiting performance scenario.
  size_t max_jobs_;

  // The maximum number of embeddings to submit to the primary embedder.
  size_t max_batch_size_;

  // Whether to block embedding work submission on performance scenario.
  bool use_performance_scenario_;

  base::ScopedObservation<
      performance_scenarios::PerformanceScenarioObserverList,
      performance_scenarios::PerformanceScenarioObserver>
      performance_scenario_observation_{this};

  base::ScopedObservation<EmbedderMetadataProvider, EmbedderMetadataObserver>
      embedder_metadata_observation_{this};

  const bool execute_for_gemma_ = false;

  base::WeakPtrFactory<SchedulingEmbedder> weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_CORE_INTERNAL_SCHEDULING_EMBEDDER_H_
