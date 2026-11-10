// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/passage_embeddings/core/internal/scheduling_embedder.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

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
                             uint64_t job_id,
                             std::vector<std::string> passages,
                             ComputePassagesEmbeddingsCallback callback)
    : priority(priority),
      job_id(job_id),
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
    bool use_performance_scenario,
    bool execute_for_gemma)
    : get_embeddings_callback_(get_embeddings_callback),
      max_jobs_(max_jobs),
      max_batch_size_(max_batch_size),
      use_performance_scenario_(use_performance_scenario),
      execute_for_gemma_(execute_for_gemma) {
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

Embedder::Job SchedulingEmbedder::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  size_t pending_job_count = 0;
  for (const auto& [prio, queue] : pending_jobs_) {
    pending_job_count += queue.size();
  }

  if (!execute_for_gemma_) {
    base::UmaHistogramCounts1000("History.Embeddings.ScheduledJobCount",
                                 pending_job_count + active_jobs_.size());

    auto count_remaining_passages = [](size_t sum, const Job& job) {
      return sum + job.passages.size() - job.embeddings.size();
    };
    size_t pending_passage_count = 0;
    for (const auto& [prio, queue] : pending_jobs_) {
      pending_passage_count += std::accumulate(queue.begin(), queue.end(), 0u,
                                               count_remaining_passages);
    }
    base::UmaHistogramCounts1000(
        "History.Embeddings.ScheduledPassageCount",
        pending_passage_count + std::accumulate(active_jobs_.begin(),
                                                active_jobs_.end(), 0u,
                                                count_remaining_passages));
  }

  const uint64_t job_id = next_job_id_++;

  // Zero size jobs are expected, and can be called back immediately
  // instead of waiting in line for nothing.
  if (passages.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<std::string>(),
                       std::vector<Embedding>(), job_id,
                       ComputeEmbeddingsStatus::kSuccess));
    return Embedder::Job(weak_ptr_factory_.GetWeakPtr(), job_id);
  }

  // Limit the number of jobs accepted to avoid high memory use when
  // waiting a long time to process the queue.
  if (pending_job_count + active_jobs_.size() >= max_jobs_) {
    // Find worst job. Reverse iterator starts at lowest priority (largest key).
    auto map_it = std::ranges::find_if(
        pending_jobs_.rbegin(), pending_jobs_.rend(),
        [](const auto& pair) { return !pair.second.empty(); });

    if (map_it == pending_jobs_.rend() || priority >= map_it->first) {
      // New job has lower priority than the worst pending job.
      // Drop the new job immediately.
      FinishJob(Job(priority, job_id, std::move(passages), std::move(callback)),
                ComputeEmbeddingsStatus::kCanceled,
                /*record_histograms=*/!execute_for_gemma_);
      return Embedder::Job(weak_ptr_factory_.GetWeakPtr(), job_id);
    } else {
      // Drop the worst job from the queue to make room for the new job.
      Job worst_job = std::move(map_it->second.front());
      map_it->second.pop_front();
      FinishJob(std::move(worst_job), ComputeEmbeddingsStatus::kCanceled,
                /*record_histograms=*/!execute_for_gemma_);
    }
  }

  pending_jobs_[priority].emplace_back(priority, job_id, std::move(passages),
                                       std::move(callback));

  SubmitWorkToEmbedder();

  return Embedder::Job(weak_ptr_factory_.GetWeakPtr(), job_id);
}

base::WeakPtr<Embedder> SchedulingEmbedder::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

  // Find the highest priority non-empty queue.
  auto map_it = GetFirstNonEmptyQueue();

  if (map_it == pending_jobs_.end()) {
    // No jobs to start.
    VLOG(5) << "SubmitWorkToEmbedder: no jobs";
    return;
  }

  if (use_performance_scenario_ && !IsPerformanceScenarioReady()) {
    // Waiting for a suitable performance scenario.
    VLOG(5) << "SubmitWorkToEmbedder: unsuitable scenario";
    return;
  }

  PassagePriority priority = map_it->first;
  std::vector<std::string> passages;
  std::vector<BatchSlice> batch;
  std::deque<Job>& queue = map_it->second;

  while (passages.size() < max_batch_size_ && !queue.empty()) {
    Job job = std::move(queue.front());
    queue.pop_front();

    size_t accept = std::min(max_batch_size_ - passages.size(),
                             job.passages.size() - job.embeddings.size());
    VLOG(3) << "Batching range [" << job.embeddings.size() << ','
            << job.embeddings.size() + accept << ") of " << job.passages.size()
            << " passages from job";
    batch.push_back({job.job_id, accept});
    for (size_t i = job.embeddings.size();
         i < job.passages.size() && accept > 0; i++, accept--) {
      passages.push_back(job.passages[i]);
    }
    active_jobs_.push_back(std::move(job));
  }

  work_submitted_ = true;
  get_embeddings_callback_.Run(
      std::move(passages), priority,
      base::BindOnce(&SchedulingEmbedder::OnEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(batch)));
}

bool SchedulingEmbedder::IsPerformanceScenarioReady() {
  // Find highest priority non-empty queue.
  auto map_it = GetFirstNonEmptyQueue();

  if (map_it != pending_jobs_.end() &&
      (map_it->first == PassagePriority::kUserInitiated ||
       map_it->first == PassagePriority::kUrgent)) {
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

std::map<PassagePriority, std::deque<SchedulingEmbedder::Job>>::iterator
SchedulingEmbedder::GetFirstNonEmptyQueue() {
  return std::ranges::find_if(
      pending_jobs_, [](const auto& pair) { return !pair.second.empty(); });
}

void SchedulingEmbedder::ReprioritizeJobs(PassagePriority priority,
                                          const std::set<uint64_t>& job_ids) {
  // Update active jobs. They just stay in place but get new priority.
  for (Job& job : active_jobs_) {
    if (job_ids.contains(job.job_id)) {
      job.priority = priority;
    }
  }

  bool jobs_moved = false;
  // Update pending jobs. We need to move them if their priority changes.
  for (auto& [prio, queue] : pending_jobs_) {
    if (prio == priority) {
      // No need to move jobs that are already at the target priority.
      continue;
    }

    std::deque<Job> kept_jobs;
    for (Job& job : queue) {
      if (job_ids.contains(job.job_id)) {
        job.priority = priority;
        pending_jobs_[priority].push_back(std::move(job));
        jobs_moved = true;
      } else {
        kept_jobs.push_back(std::move(job));
      }
    }
    queue = std::move(kept_jobs);
  }

  if (jobs_moved) {
    // Sort by job_id to ensure that reprioritized jobs maintain strict FIFO
    // order relative to jobs that were already in the destination queue.
    std::ranges::sort(pending_jobs_[priority], {}, &Job::job_id);
  }
}

bool SchedulingEmbedder::TryCancel(uint64_t job_id) {
  for (auto& [priority, queue] : pending_jobs_) {
    auto it =
        std::find_if(queue.begin(), queue.end(),
                     [job_id](const Job& job) { return job.job_id == job_id; });
    if (it != queue.end()) {
      CancelJob(std::move(*it));
      queue.erase(it);
      return true;
    }
  }

  // A job with passages in the in-flight batch is dropped the same way.
  // OnEmbeddingsComputed() skips the results of jobs that are gone.
  auto it = std::ranges::find(active_jobs_, job_id, &Job::job_id);
  if (it == active_jobs_.end()) {
    return false;
  }
  CancelJob(std::move(*it));
  active_jobs_.erase(it);
  return true;
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
    const std::vector<BatchSlice>& batch,
    std::vector<mojom::PassageEmbeddingsResultPtr> results,
    ComputeEmbeddingsStatus status) {
  std::vector<Embedding> embeddings;
  bool normalization_failed = false;
  for (auto& result : results) {
    std::optional<std::vector<float>> normalized =
        Embedding::Normalize(std::move(result->embeddings));
    if (!normalized) {
      // The service returned a vector that can't be scaled to unit length.
      // Results are matched to jobs by position, so keep the ones before it
      // and drop the rest. The job that owns the bad vector fails below.
      normalization_failed = true;
      break;
    }
    embeddings.emplace_back(std::move(normalized.value()));
  }

  VLOG(3) << embeddings.size() << " embeddings computed with status "
          << static_cast<int>(status);

  if (active_jobs_.empty()) {
    // Every job in the batch was canceled while it was being embedded.
    work_submitted_ = false;
    SubmitWorkToEmbedder();
    return;
  }

  // Take embeddings into jobs slice by slice and pop the jobs as they're
  // filled. A slice whose job isn't at the front of `active_jobs_` belongs to a
  // job that was canceled while the batch was in flight, so its embeddings are
  // skipped. Embeddings past the last slice are ignored.
  size_t read_index = 0;
  for (const BatchSlice& slice : batch) {
    if (read_index >= embeddings.size() || active_jobs_.empty()) {
      break;
    }
    if (active_jobs_.front().job_id != slice.job_id) {
      DCHECK(!std::ranges::contains(active_jobs_, slice.job_id, &Job::job_id));
      read_index += slice.passage_count;
      continue;
    }
    Job& job = active_jobs_.front();
    const size_t slice_end =
        std::min(read_index + slice.passage_count, embeddings.size());
    while (read_index < slice_end) {
      job.embeddings.push_back(std::move(embeddings[read_index]));
      read_index++;
    }
    if (job.embeddings.size() == job.passages.size()) {
      Job completed_job = std::move(job);
      active_jobs_.pop_front();
      FinishJob(std::move(completed_job), status,
                /*record_histograms=*/!execute_for_gemma_);
    } else {
      // Job is not fully completed. Stop processing results. Below, it either
      // fails or goes back to pending.
      break;
    }
  }

  // Fail the job whose slice holds the first missing embedding if a vector
  // couldn't be normalized or if the service returned no results:
  // - If normalization_failed, this job owns the bad vector, and resubmitting
  //   its passage would get the same vector back. A bad vector past the last
  //   slice belongs to no job.
  // - If results was empty, the failing passage isn't known; failing the lead
  //   job avoids retry loops while upholding the 1:1 callback guarantee (even
  //   if a later job's passage caused the batch failure, as the embedder is
  //   not expected to fail in practice).
  // If this job was canceled, no other job fails in its place. That can't
  // cause a retry loop, because a canceled job is never resubmitted. Remaining
  // jobs are re-queued below so they still have a chance to succeed.
  if ((embeddings.empty() || normalization_failed) && !active_jobs_.empty()) {
    size_t slice_start = 0;
    for (const BatchSlice& slice : batch) {
      if (embeddings.size() < slice_start + slice.passage_count) {
        // Every earlier slice had all its embeddings, so its job either
        // completed above or was canceled. This slice's job is now at the
        // front unless it was canceled.
        if (active_jobs_.front().job_id == slice.job_id) {
          Job completed_job = std::move(active_jobs_.front());
          active_jobs_.pop_front();
          FinishJob(std::move(completed_job), status,
                    /*record_histograms=*/!execute_for_gemma_);
        }
        break;
      }
      slice_start += slice.passage_count;
    }
  }

  // Move any remaining active jobs (including any partially filled one) back to
  // pending since the current batch is done. They are moved back in reverse
  // order (back to front) to the front of pending_jobs_ so that their relative
  // order is preserved.
  while (!active_jobs_.empty()) {
    Job job = std::move(active_jobs_.back());
    pending_jobs_[job.priority].push_front(std::move(job));
    active_jobs_.pop_back();
  }

  // Note, this could call back later/asynchronously or
  // immediately/synchronously, depending on the embedder.
  work_submitted_ = false;
  SubmitWorkToEmbedder();
}

// static
void SchedulingEmbedder::FinishJob(Job job,
                                   ComputeEmbeddingsStatus status,
                                   bool record_histograms) {
  VLOG(2) << "Finished embedding work with status " << static_cast<int>(status)
          << " for " << job.passages.size() << " passages starting with `"
          << (job.passages.empty() ? "" : job.passages[0]) << "`";
  if (job.passages.size() != job.embeddings.size()) {
    job.embeddings.clear();
    // If the service reported success but provided no embeddings for the lead
    // job, ensure that we report a failure to the client to maintain the
    // invariant that success implies all embeddings were provided.
    if (status == ComputeEmbeddingsStatus::kSuccess) {
      status = ComputeEmbeddingsStatus::kExecutionFailure;
    }
  }

  std::move(job.callback)
      .Run(std::move(job.passages), std::move(job.embeddings), job.job_id,
           status);
  if (record_histograms) {
    if (status == ComputeEmbeddingsStatus::kSuccess) {
      RecordDurationHistograms(job.priority, job.timer.Elapsed());
    }
    RecordStatusHistograms(job.priority, status);
  }
}

void SchedulingEmbedder::CancelJob(Job job) {
  VLOG(2) << "Aborted embedding work for " << job.passages.size()
          << " passages starting with `"
          << (job.passages.empty() ? "" : job.passages[0]) << "`";

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(job.callback), std::move(job.passages),
                     std::vector<Embedding>(), job.job_id,
                     ComputeEmbeddingsStatus::kCanceled));
  if (!execute_for_gemma_) {
    RecordStatusHistograms(job.priority, ComputeEmbeddingsStatus::kCanceled);
  }
}

}  // namespace passage_embeddings
