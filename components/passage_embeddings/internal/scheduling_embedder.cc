// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/internal/scheduling_embedder.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace passage_embeddings {

namespace {

using ScenarioScope = performance_scenarios::ScenarioScope;
using LoadingScenario = performance_scenarios::LoadingScenario;
using InputScenario = performance_scenarios::InputScenario;

std::string PassagePriorityToString(PassagePriority priority) {
  switch (priority) {
    case PassagePriority::kUserInitiated:
      return "UserInitiated";
    case PassagePriority::kUrgent:
      return "Urgent";
    case PassagePriority::kPassive:
      return "Passive";
    case PassagePriority::kLatent:
      return "Latent";
  }
}

void RecordDurationHistograms(PassagePriority priority,
                              base::TimeDelta duration) {
  base::UmaHistogramTimes("History.Embeddings.ScheduledJobDuration", duration);
  base::UmaHistogramTimes(
      base::StringPrintf("History.Embeddings.ScheduledJobDuration.%s",
                         PassagePriorityToString(priority)),
      duration);
}

void RecordStatusHistograms(PassagePriority priority,
                            ComputeEmbeddingsStatus status) {
  base::UmaHistogramEnumeration("History.Embeddings.ScheduledJobStatus",
                                status);
  base::UmaHistogramEnumeration(
      base::StringPrintf("History.Embeddings.ScheduledJobStatus.%s",
                         PassagePriorityToString(priority)),
      status);
}

}  // namespace

SchedulingEmbedder::Job::Job(PassagePriority priority,
                             TaskId task_id,
                             std::vector<std::string> passages,
                             ComputePassagesEmbeddingsCallback callback)
    : priority(priority),
      task_id(task_id),
      passages(std::move(passages)),
      callback(std::move(callback)) {}

SchedulingEmbedder::Job::~Job() = default;

SchedulingEmbedder::Job::Job(Job&&) = default;

SchedulingEmbedder::Job& SchedulingEmbedder::Job::operator=(Job&&) = default;

////////////////////////////////////////////////////////////////////////////////

SchedulingEmbedder::SchedulingEmbedder(
    EmbedderMetadataProvider* embedder_metadata_provider,
    GetEmbeddingsCallback get_embeddings_callback,
    size_t max_jobs,
    size_t max_batch_size,
    bool use_performance_scenario)
    : get_embeddings_callback_(get_embeddings_callback),
      max_jobs_(max_jobs),
      max_batch_size_(max_batch_size),
      use_performance_scenario_(use_performance_scenario) {
  if (embedder_metadata_provider) {
    embedder_metadata_observation_.Observe(embedder_metadata_provider);
  }
  if (use_performance_scenario_) {
    performance_scenario_observation_.Observe(
        performance_scenarios::PerformanceScenarioObserverList::GetForScope(
            ScenarioScope::kGlobal)
            .get());
  }
}

SchedulingEmbedder::~SchedulingEmbedder() = default;

SchedulingEmbedder::TaskId SchedulingEmbedder::ComputePassagesEmbeddings(
    PassagePriority priority,
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

  const TaskId task_id = next_task_id_++;

  // Zero size jobs are expected, and can be called back immediately
  // instead of waiting in line for nothing.
  if (passages.empty()) {
    std::move(callback).Run(
        /*passages=*/{}, /*embeddings=*/{}, task_id,
        ComputeEmbeddingsStatus::kSuccess);
    return task_id;
  }

  // Limit the number of jobs accepted to avoid high memory use when
  // waiting a long time to process the queue.
  while (jobs_.size() >= max_jobs_ && !jobs_.back().in_progress) {
    FinishJob(std::move(jobs_.back()), ComputeEmbeddingsStatus::kCanceled);
    jobs_.pop_back();
  }

  jobs_.emplace_back(priority, task_id, std::move(passages),
                     std::move(callback));

  SubmitWorkToEmbedder();

  return task_id;
}

void SchedulingEmbedder::SubmitWorkToEmbedder() {
  if (!embedder_metadata_.IsValid()) {
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
  while (passages.size() < max_batch_size_ && job_index < jobs_.size() &&
         jobs_.at(job_index).priority == priority) {
    Job& job = jobs_.at(job_index);
    job.in_progress = true;
    size_t accept = std::min(max_batch_size_ - passages.size(),
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
  get_embeddings_callback_.Run(
      std::move(passages), priority,
      base::BindOnce(&SchedulingEmbedder::OnEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool SchedulingEmbedder::IsPerformanceScenarioReady() {
  if (!jobs_.empty() &&
      (jobs_.front().priority == PassagePriority::kUserInitiated ||
       jobs_.front().priority == PassagePriority::kUrgent)) {
    // Do not block on performance scenario if user initiated a query or it's
    // urgent.
    return true;
  }

  LoadingScenario loading_scenario =
      performance_scenarios::GetLoadingScenario(ScenarioScope::kGlobal)
          ->load(std::memory_order_relaxed);
  InputScenario input_scenario =
      performance_scenarios::GetInputScenario(ScenarioScope::kGlobal)
          ->load(std::memory_order_relaxed);
  return (loading_scenario == LoadingScenario::kNoPageLoading ||
          loading_scenario == LoadingScenario::kBackgroundPageLoading) &&
         input_scenario == InputScenario::kNoInput;
}

void SchedulingEmbedder::ReprioritizeTasks(PassagePriority priority,
                                           const std::set<TaskId>& tasks) {
  for (Job& job : jobs_) {
    const auto loc = tasks.find(job.task_id);
    if (loc != tasks.end()) {
      job.priority = priority;
    }
  }

  // Note: the jobs will be reordered to account for the new priorities on the
  // next call to SubmitWorkToEmbedder().
}

bool SchedulingEmbedder::TryCancel(TaskId task_id) {
  for (auto itr = jobs_.begin(); itr < jobs_.end(); itr++) {
    Job& job = *itr;
    if (task_id == job.task_id && !job.in_progress) {
      VLOG(2) << "Aborted embedding work for " << job.passages.size()
              << " passages starting with `"
              << (job.passages.empty() ? "" : job.passages[0]) << "`";
      std::move(job.callback)
          .Run(std::move(job.passages), {}, job.task_id,
               ComputeEmbeddingsStatus::kCanceled);
      RecordStatusHistograms(job.priority, ComputeEmbeddingsStatus::kCanceled);
      jobs_.erase(itr);
      return true;
    }
  }
  return false;
}

void SchedulingEmbedder::EmbedderMetadataUpdated(EmbedderMetadata metadata) {
  VLOG(4) << "SchedulingEmbedder received metadata with version: "
          << metadata.model_version;
  embedder_metadata_ = metadata;
  SubmitWorkToEmbedder();
}

void SchedulingEmbedder::OnLoadingScenarioChanged(
    ScenarioScope scope,
    LoadingScenario old_scenario,
    LoadingScenario new_scenario) {
  VLOG(5) << "SchedulingEmbedder using new loading scenario: "
          << static_cast<int>(new_scenario);
  SubmitWorkToEmbedder();
}

void SchedulingEmbedder::OnInputScenarioChanged(ScenarioScope scope,
                                                InputScenario old_scenario,
                                                InputScenario new_scenario) {
  VLOG(5) << "SchedulingEmbedder using new input scenario: "
          << static_cast<int>(new_scenario);
  SubmitWorkToEmbedder();
}

void SchedulingEmbedder::OnEmbeddingsComputed(
    std::vector<mojom::PassageEmbeddingsResultPtr> results,
    ComputeEmbeddingsStatus status) {
  std::vector<Embedding> embeddings;
  for (auto& result : results) {
    embeddings.emplace_back(result->embeddings);
    embeddings.back().Normalize();
  }

  VLOG(3) << embeddings.size() << " embeddings computed with status "
          << static_cast<int>(status);

  if (embeddings.empty()) {
    FinishJob(std::move(jobs_.front()), status);
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

  // Take embeddings into jobs and pop them as they're filled. The
  // !jobs_.empty() check ensures we don't overrun the available jobs if the
  // service were to maliciously send too many embeddings.
  size_t read_index = 0;
  while (read_index < embeddings.size() && !jobs_.empty()) {
    Job& job = jobs_.front();
    while (job.embeddings.size() < job.passages.size() &&
           read_index < embeddings.size()) {
      job.embeddings.push_back(std::move(embeddings[read_index]));
      read_index++;
    }
    if (job.embeddings.size() == job.passages.size()) {
      FinishJob(std::move(job), status);
      jobs_.pop_front();
    }
  }

  // Note, this could call back later/asynchronously or
  // immediately/synchronously, depending on the embedder.
  work_submitted_ = false;
  SubmitWorkToEmbedder();
}

// static
void SchedulingEmbedder::FinishJob(Job job, ComputeEmbeddingsStatus status) {
  VLOG(2) << "Finished embedding work with status " << static_cast<int>(status)
          << " for " << job.passages.size() << " passages starting with `"
          << job.passages[0] << "`";
  if (job.passages.size() != job.embeddings.size()) {
    job.embeddings.clear();
  }
  std::move(job.callback)
      .Run(std::move(job.passages), std::move(job.embeddings), job.task_id,
           status);
  if (status == ComputeEmbeddingsStatus::kSuccess) {
    RecordDurationHistograms(job.priority, job.timer.Elapsed());
  }
  RecordStatusHistograms(job.priority, status);
}

}  // namespace passage_embeddings
