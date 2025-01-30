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
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/history_embeddings/embedder.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

#if BUILDFLAG(USE_BLINK)
#include "third_party/blink/public/common/performance/performance_scenario_observer.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#endif

namespace history_embeddings {

// The SchedulingEmbedder wraps a primary embedder and adds scheduling control
// with batching and priorities so that high priority queries can be computed as
// soon as possible. Scheduling is also needed to avoid clogging the pipes for a
// slow remote embedder. Even single pages can take a while, and when the model
// changes, all existing passages need their embeddings recomputed, which can
// take a very long time and should be done at lower priority.
class SchedulingEmbedder
#if BUILDFLAG(USE_BLINK)
    : public blink::performance_scenarios::PerformanceScenarioObserver
#endif
{
 public:
  using TaskId = uint64_t;
  static constexpr TaskId kInvalidTaskId = 0;

  SchedulingEmbedder(std::unique_ptr<Embedder> embedder,
                     size_t scheduled_max,
                     bool use_performance_scenario);
#if BUILDFLAG(USE_BLINK)
  ~SchedulingEmbedder() override;
#else
  ~SchedulingEmbedder();
#endif

  // Computes embeddings for each entry in `passages`. Will invoke callback on
  // done. If successful, it is guaranteed that the number of passages in
  // `passages` will match the number of entries in the embeddings vector and in
  // the same order. If unsuccessful, the callback will still return the
  // original passages but with an empty embeddings vector and an appropriate
  // status.
  using ComputePassagesEmbeddingsCallback = base::OnceCallback<void(
      std::vector<std::string> passages,
      std::vector<Embedding> embeddings,
      TaskId task_id,
      passage_embeddings::ComputeEmbeddingsStatus status)>;
  TaskId ComputePassagesEmbeddings(passage_embeddings::PassagePriority priority,
                                   std::vector<std::string> passages,
                                   ComputePassagesEmbeddingsCallback callback);

  // Sets the callback to run when the embedder is ready to process requests.
  // The callback is invoked immediately if the embedder is ready beforehand.
  using OnEmbedderReadyCallback =
      base::OnceCallback<void(passage_embeddings::EmbedderMetadata metadata)>;
  void SetOnEmbedderReady(OnEmbedderReadyCallback callback);

  // Cancels computation of embeddings iff none of the passages given to
  // `ComputePassagesEmbeddings()` has been submitted to the embedder yet.
  // If successful, the callback for the canceled task will be invoked with
  // `ComputeEmbeddingsStatus::kCanceled` status.
  bool TryCancel(TaskId task_id);

#if BUILDFLAG(USE_BLINK)
  // PerformanceScenarioObserver:
  void OnLoadingScenarioChanged(
      blink::performance_scenarios::ScenarioScope scope,
      blink::performance_scenarios::LoadingScenario old_scenario,
      blink::performance_scenarios::LoadingScenario new_scenario) override;
  void OnInputScenarioChanged(
      blink::performance_scenarios::ScenarioScope scope,
      blink::performance_scenarios::InputScenario old_scenario,
      blink::performance_scenarios::InputScenario new_scenario) override;
#endif

 private:
  // A job consists of multiple passages, and each passage must have its
  // embedding computed. When all are finished, the job is done and its
  // callback will be invoked. Multiple jobs may be batched together when
  // when submitting work to the `embedder_`, and jobs can also be broken
  // down so that partial progress is made across multiple work submissions.
  struct Job {
    Job(passage_embeddings::PassagePriority priority,
        TaskId task_id,
        std::vector<std::string> passages,
        ComputePassagesEmbeddingsCallback callback);
    ~Job();
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    Job(Job&&);
    Job& operator=(Job&&);

    // Data for the job is saved from calls to `ComputePassagesEmbeddings`.
    passage_embeddings::PassagePriority priority;
    TaskId task_id = kInvalidTaskId;
    bool in_progress = false;
    std::vector<std::string> passages;
    ComputePassagesEmbeddingsCallback callback;

    // Completed embeddings; may be partial.
    std::vector<Embedding> embeddings;

    // Measures total job duration, from creation to completion.
    base::ElapsedTimer timer;
  };

  // Intercepts metadata so that work can be queued up while the primary
  // embedder isn't ready. For the MlEmbedder, this avoids failing when the
  // model hasn't loaded yet. We just wait until it's ready, then start work.
  void OnEmbedderReady(OnEmbedderReadyCallback callback,
                       passage_embeddings::EmbedderMetadata metadata);

  // Invoked after the embedding for the current job has been computed.
  // Continues processing next job if one is pending.
  void OnEmbeddingsComputed(std::vector<std::string> passages,
                            std::vector<Embedding> embedding,
                            passage_embeddings::ComputeEmbeddingsStatus status);

  // Stable-sort jobs by priority and submit a batch of work to embedder.
  // This will only submit new work if the embedder is not already working.
  void SubmitWorkToEmbedder();

  // Returns true if currently in a work ready performance scenario state.
  bool IsPerformanceScenarioReady();

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

  // The primary embedder that does the actual embedding computations.
  // This may be slow, and we await results before sending the next request.
  std::unique_ptr<Embedder> embedder_;

  // Starts false; set true when valid metadata is received from `embedder_`.
  bool embedder_ready_{false};

  // The maximum number of embeddings to submit to the primary embedder.
  size_t scheduled_max_;

  // Whether to block embedding work submission on performance scenario.
  bool use_performance_scenario_;

#if BUILDFLAG(USE_BLINK)
  blink::performance_scenarios::LoadingScenario loading_scenario_ =
      blink::performance_scenarios::LoadingScenario::kNoPageLoading;
  blink::performance_scenarios::InputScenario input_scenario_ =
      blink::performance_scenarios::InputScenario::kNoInput;

  base::ScopedObservation<
      blink::performance_scenarios::PerformanceScenarioObserverList,
      SchedulingEmbedder>
      performance_scenario_observation_{this};
#endif

  base::WeakPtrFactory<SchedulingEmbedder> weak_ptr_factory_{this};
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_SCHEDULING_EMBEDDER_H_
