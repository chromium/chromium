// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_INTERNAL_SCHEDULING_EMBEDDER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_INTERNAL_SCHEDULING_EMBEDDER_H_

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
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
                     bool use_performance_scenario);
  ~SchedulingEmbedder() override;

  // Embedder:
  TaskId ComputePassagesEmbeddings(
      PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;
  void ReprioritizeTasks(PassagePriority priority,
                         const std::set<TaskId>& tasks) override;
  bool TryCancel(TaskId task_id) override;

 private:
  // A job consists of multiple passages, and each passage must have its
  // embedding computed. When all are finished, the job is done and its
  // callback will be invoked. Multiple jobs may be batched together when
  // submitting work to the `embedder_remote_proxy`, and jobs can also be broken
  // down so that partial progress is made across multiple work submissions.
  struct Job {
    Job(PassagePriority priority,
        TaskId task_id,
        std::vector<std::string> passages,
        ComputePassagesEmbeddingsCallback callback);
    ~Job();
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    Job(Job&&);
    Job& operator=(Job&&);

    // Data for the job is saved from calls to `ComputePassagesEmbeddings`.
    PassagePriority priority;
    TaskId task_id;
    std::vector<std::string> passages;
    ComputePassagesEmbeddingsCallback callback;

    bool in_progress = false;

    // Completed embeddings; may be partial.
    std::vector<Embedding> embeddings;

    // Measures total job duration, from creation to completion.
    base::ElapsedTimer timer;
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
  // Continues processing next job if one is pending.
  void OnEmbeddingsComputed(
      std::vector<mojom::PassageEmbeddingsResultPtr> results,
      ComputeEmbeddingsStatus status);

  // Stable-sort jobs by priority and submit a batch of work to embedder.
  // This will only submit new work if the embedder is not already working.
  void SubmitWorkToEmbedder();

  // Returns true if currently in a work ready performance scenario state.
  bool IsPerformanceScenarioReady();

  // Call the callback with status, etc. and record relevant histograms.
  static void FinishJob(Job job, ComputeEmbeddingsStatus status);

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

  // ID to assign to the next Job.
  TaskId next_task_id_ = 1;

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

  base::WeakPtrFactory<SchedulingEmbedder> weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_INTERNAL_SCHEDULING_EMBEDDER_H_
