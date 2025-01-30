// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/scheduling_embedder.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/history_embeddings/vector_database.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace history_embeddings {

namespace {

using passage_embeddings::PassagePriority;

}  // namespace

SchedulingEmbedder::Job::Job(passage_embeddings::PassagePriority priority,
                             TaskId task_id,
                             std::vector<std::string> passages,
                             ComputePassagesEmbeddingsCallback callback)
    : priority(priority),
      task_id(task_id),
      passages(std::move(passages)),
      callback(std::move(callback)) {
  // No Job should have an invalid task Id.
  CHECK_NE(task_id, kInvalidTaskId);
}
SchedulingEmbedder::Job::~Job() = default;
SchedulingEmbedder::Job::Job(Job&&) = default;
SchedulingEmbedder::Job& SchedulingEmbedder::Job::operator=(Job&&) = default;

////////////////////////////////////////////////////////////////////////////////

SchedulingEmbedder::SchedulingEmbedder(std::unique_ptr<Embedder> embedder,
                                       size_t scheduled_max,
                                       bool use_performance_scenario)
    : embedder_(std::move(embedder)),
      scheduled_max_(scheduled_max),
      use_performance_scenario_(use_performance_scenario) {
#if BUILDFLAG(USE_BLINK)
  if (use_performance_scenario_) {
    performance_scenario_observation_.Observe(
        blink::performance_scenarios::PerformanceScenarioObserverList::
            GetForScope(blink::performance_scenarios::ScenarioScope::kGlobal)
                .get());
  }
#else
  // Performance scenario is not supported on some builds (e.g. iOS by default).
  use_performance_scenario_ = false;
#endif
}

SchedulingEmbedder::~SchedulingEmbedder() = default;

SchedulingEmbedder::TaskId SchedulingEmbedder::ComputePassagesEmbeddings(
    passage_embeddings::PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  base::UmaHistogramCounts1000("History.Embeddings.ScheduledJobCount",
                               jobs_.size());
  base::UmaHistogramCounts1000(
      "History.Embeddings.ScheduledPassageCount",
      std::accumulate(
          jobs_.begin(), jobs_.end(), 0u, [](size_t sum, const Job& job) {
            return sum + job.passages.size() - job.embeddings.size();
          }));

  TaskId task_id = next_task_id_;
  next_task_id_++;

  // Zero size jobs are expected, and can be called back immediately
  // instead of waiting in line for nothing.
  if (passages.empty()) {
    std::move(callback).Run(
        /*passages=*/{}, /*embeddings=*/{}, task_id,
        passage_embeddings::ComputeEmbeddingsStatus::kSuccess);
    return task_id;
  }

  jobs_.emplace_back(priority, task_id, std::move(passages),
                     std::move(callback));

  SubmitWorkToEmbedder();

  return task_id;
}

void SchedulingEmbedder::SubmitWorkToEmbedder() {
  if (!embedder_ready_) {
    // Underlying embedder not ready yet. Wait for it.
    VLOG(5) << "SubmitWorkToEmbedder: embedder not ready";
    return;
  }

  if (work_submitted_) {
    // Waiting for work in progress to complete.
    VLOG(5) << "SubmitWorkToEmbedder: work already in progress";
    return;
  }

  if (jobs_.empty()) {
    // No jobs to start.
    VLOG(5) << "SubmitWorkToEmbedder: no jobs";
    return;
  }

  if (use_performance_scenario_ && !IsPerformanceScenarioReady()) {
    // Waiting for a suitable performance scenario.
    VLOG(5) << "SubmitWorkToEmbedder: unsuitable scenario";
    return;
  }

  // Put higher priority jobs at the front. This may suspend partially
  // completed jobs of lower priority by pushing them toward the back.
  std::stable_sort(jobs_.begin(), jobs_.end(), [](const Job& a, const Job& b) {
    return a.priority < b.priority;
  });

  // Submit a batch of passages taken from jobs near the front of the queue.
  // Only submit one priority type of passage, regardless of count.
  PassagePriority priority = jobs_.front().priority;
  std::vector<std::string> passages;
  size_t job_index = 0;
  while (passages.size() < scheduled_max_ && job_index < jobs_.size() &&
         jobs_.at(job_index).priority == priority) {
    Job& job = jobs_.at(job_index);
    job.in_progress = true;
    size_t accept = std::min(scheduled_max_ - passages.size(),
                             job.passages.size() - job.embeddings.size());
    VLOG(3) << "Batching range [" << job.embeddings.size() << ','
            << job.embeddings.size() + accept << ") of " << job.passages.size()
            << " passages from job " << job_index << '/' << jobs_.size();
    for (size_t i = job.embeddings.size();
         i < job.passages.size() && accept > 0; i++, accept--) {
      passages.push_back(job.passages[i]);
    }
    job_index++;
  }

  work_submitted_ = true;
  embedder_->ComputePassagesEmbeddings(
      priority, std::move(passages),
      base::BindOnce(&SchedulingEmbedder::OnEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool SchedulingEmbedder::IsPerformanceScenarioReady() {
#if BUILDFLAG(USE_BLINK)
  if (!jobs_.empty() &&
      jobs_.front().priority ==
          passage_embeddings::PassagePriority::kUserInitiated) {
    // Do not block on performance scenario if user initiated a query.
    return true;
  }
  return (loading_scenario_ ==
              blink::performance_scenarios::LoadingScenario::kNoPageLoading ||
          loading_scenario_ == blink::performance_scenarios::LoadingScenario::
                                   kBackgroundPageLoading) &&
         input_scenario_ ==
             blink::performance_scenarios::InputScenario::kNoInput;
#else
  return true;
#endif
}

void SchedulingEmbedder::SetOnEmbedderReady(OnEmbedderReadyCallback callback) {
  embedder_->SetOnEmbedderReady(
      base::BindOnce(&SchedulingEmbedder::OnEmbedderReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool SchedulingEmbedder::TryCancel(TaskId task_id) {
  // No Job should have an invalid task Id.
  CHECK_NE(task_id, kInvalidTaskId);

  for (auto itr = jobs_.begin(); itr < jobs_.end(); itr++) {
    Job& job = *itr;
    if (task_id == job.task_id && !job.in_progress) {
      VLOG(2) << "Aborted embedding work for " << job.passages.size()
              << " passages starting with `"
              << (job.passages.empty() ? "" : job.passages[0]) << "`";
      std::move(job.callback)
          .Run(std::move(job.passages), {}, job.task_id,
               passage_embeddings::ComputeEmbeddingsStatus::kCanceled);
      jobs_.erase(itr);
      return true;
    }
  }
  return false;
}

#if BUILDFLAG(USE_BLINK)
void SchedulingEmbedder::OnLoadingScenarioChanged(
    blink::performance_scenarios::ScenarioScope scope,
    blink::performance_scenarios::LoadingScenario old_scenario,
    blink::performance_scenarios::LoadingScenario new_scenario) {
  VLOG(5) << "SchedulingEmbedder using new loading scenario: "
          << static_cast<int>(new_scenario);
  loading_scenario_ = new_scenario;
  SubmitWorkToEmbedder();
}

void SchedulingEmbedder::OnInputScenarioChanged(
    blink::performance_scenarios::ScenarioScope scope,
    blink::performance_scenarios::InputScenario old_scenario,
    blink::performance_scenarios::InputScenario new_scenario) {
  VLOG(5) << "SchedulingEmbedder using new input scenario: "
          << static_cast<int>(new_scenario);
  input_scenario_ = new_scenario;
  SubmitWorkToEmbedder();
}
#endif

void SchedulingEmbedder::OnEmbedderReady(
    OnEmbedderReadyCallback callback,
    passage_embeddings::EmbedderMetadata metadata) {
  embedder_ready_ = metadata.model_version != 0;
  std::move(callback).Run(metadata);
  SubmitWorkToEmbedder();
}

void SchedulingEmbedder::OnEmbeddingsComputed(
    std::vector<std::string> passages,
    std::vector<Embedding> embeddings,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  VLOG(3) << embeddings.size() << " embeddings computed for " << passages.size()
          << " passages with status " << static_cast<int>(status);
  CHECK_EQ(passages.size(), embeddings.size());

  if (embeddings.empty()) {
    Job& job = jobs_.front();
    VLOG(2) << "Aborted embedding work for " << job.passages.size()
            << " passages starting with `"
            << (job.passages.empty() ? "" : job.passages[0]) << "`";
    std::move(job.callback)
        .Run(std::move(job.passages), {}, job.task_id, status);
    jobs_.pop_front();
    // Continue on to allow possibility of resuming any remaining jobs.
    // This upholds the 1:1 callback requirement and gives jobs another
    // chance to succeed even when primary embedder fails a batch.
    // Note, we don't fail all jobs here, only the first. Failing fewer could
    // result in retry loops requiring special handling in order to keep the 1:1
    // callback guarantee. And failing more than the first is unnecessary since
    // progress can be made while giving the later jobs another chance to
    // succeed. Note, if a failure is caused by a passage from a later job
    // in a batch, failing the first job may not be the optimal recovery
    // strategy, but the underlying embedder is not expected to fail at all.
  }

  // Take embeddings into jobs and pop them as they're filled.
  size_t read_index = 0;
  while (read_index < embeddings.size()) {
    Job& job = jobs_.front();
    while (job.embeddings.size() < job.passages.size() &&
           read_index < embeddings.size()) {
      CHECK_EQ(job.passages[job.embeddings.size()], passages[read_index]);
      job.embeddings.push_back(std::move(embeddings[read_index]));
      read_index++;
    }
    if (job.embeddings.size() == job.passages.size()) {
      base::UmaHistogramTimes("History.Embeddings.ScheduledJobDuration",
                              job.timer.Elapsed());
      VLOG(2) << "Finished embedding work for " << job.passages.size()
              << " passages starting with `" << job.passages[0] << "`";
      std::move(job.callback)
          .Run(std::move(job.passages), std::move(job.embeddings), job.task_id,
               status);
      jobs_.pop_front();
    }
  }

  // Note, this could call back later/asynchronously or
  // immediately/synchronously, depending on the embedder.
  work_submitted_ = false;
  SubmitWorkToEmbedder();
}

}  // namespace history_embeddings
