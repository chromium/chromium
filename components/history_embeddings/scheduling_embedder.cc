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

SchedulingEmbedder::Job::Job(PassageKind kind,
                             std::vector<std::string> passages,
                             ComputePassagesEmbeddingsCallback callback)
    : kind(kind),
      passages(std::move(passages)),
      callback(std::move(callback)) {}
SchedulingEmbedder::Job::~Job() = default;
SchedulingEmbedder::Job::Job(Job&&) = default;
SchedulingEmbedder::Job& SchedulingEmbedder::Job::operator=(Job&&) = default;

////////////////////////////////////////////////////////////////////////////////

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
  // When adding a new query, old waiting queries can be removed. They will
  // be on top because they have highest priority.
  if (kind == PassageKind::QUERY) {
    CHECK_EQ(passages.size(), 1u);
    while (!waiting_jobs_.empty() &&
           waiting_jobs_.top().kind == PassageKind::QUERY) {
      VLOG(2) << "Dropped pending query '" << waiting_jobs_.top().passages[0]
              << "'. Next query: '" << passages[0] << "'";
      Job job = PopTopJob();
      std::move(job.callback).Run({}, {}, ComputeEmbeddingsStatus::SKIPPED);

      // At most one is expected since previous calls will have dropped already.
      CHECK(waiting_jobs_.empty() ||
            waiting_jobs_.top().kind != PassageKind::QUERY);
    }
  }

  waiting_jobs_.emplace(kind, std::move(passages), std::move(callback));

  // Only start work if none is in progress. If work is already begun
  // then it will continue later when embeddings are returned.
  if (working_jobs_.empty()) {
    SubmitWorkToEmbedder();
  }
}

SchedulingEmbedder::Job SchedulingEmbedder::PopTopJob() {
  Job job = std::move(const_cast<Job&>(waiting_jobs_.top()));
  waiting_jobs_.pop();
  return job;
}

void SchedulingEmbedder::SubmitWorkToEmbedder() {
  if (!embedder_ready_) {
    // Underlying embedder not ready yet. Wait for it.
    return;
  }

  // Pull jobs into work queue if needed. Note that higher-priority waiting jobs
  // can jump to the front of the line and suspend partially completed jobs.
  if (!waiting_jobs_.empty()) {
    if (working_jobs_.empty() ||
        waiting_jobs_.top().kind < working_jobs_.front().kind) {
      // Only batch on one kind of passage.
      PassageKind kind = waiting_jobs_.top().kind;
      size_t taken_count = 0;
      while (!waiting_jobs_.empty() && waiting_jobs_.top().kind == kind &&
             taken_count < scheduled_min_) {
        working_jobs_.push_back(PopTopJob());
        // This keeps track of how many passages we're taking from waiting
        // jobs into working jobs, but that does not determine the total
        // amount of work submitted to the embedder. These may be combined
        // with in-progress jobs and batched up to the maximum down below.
        taken_count += working_jobs_.back().passages.size();
        VLOG(2) << "Started embedding work for "
                << working_jobs_.back().passages.size()
                << " passages starting with `"
                << working_jobs_.back().passages[0] << "`";
      }
      // Put higher priority jobs at the front. This is usually a no-op, but may
      // suspend partially completed jobs of lower priority.
      std::stable_sort(
          working_jobs_.begin(), working_jobs_.end(),
          [](const Job& a, const Job& b) { return a.kind < b.kind; });
    }
  }

  // Submit a batch of passages taken from the working jobs.
  if (!working_jobs_.empty()) {
    // Only submit one kind of passage, regardless of count.
    PassageKind kind = working_jobs_.front().kind;
    std::vector<std::string> passages;
    size_t job_index = 0;
    while (passages.size() < scheduled_max_ &&
           job_index < working_jobs_.size()) {
      const Job& job = working_jobs_.at(job_index);
      size_t accept = std::min(scheduled_max_ - passages.size(),
                               job.passages.size() - job.embeddings.size());
      for (size_t i = job.embeddings.size();
           i < job.passages.size() && accept > 0; i++, accept--) {
        passages.push_back(job.passages[i]);
      }
      job_index++;
    }
    VLOG(3) << passages.size() << " passages submitted to embedder";
    embedder_->ComputePassagesEmbeddings(
        kind, std::move(passages),
        base::BindOnce(&SchedulingEmbedder::OnEmbeddingsComputed,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SchedulingEmbedder::SetOnEmbedderReady(OnEmbedderReadyCallback callback) {
  embedder_->SetOnEmbedderReady(
      base::BindOnce(&SchedulingEmbedder::OnEmbedderReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SchedulingEmbedder::OnEmbedderReady(OnEmbedderReadyCallback callback,
                                         EmbedderMetadata metadata) {
  embedder_ready_ = metadata.model_version != 0;
  std::move(callback).Run(metadata);
  if (working_jobs_.empty()) {
    SubmitWorkToEmbedder();
  }
}

void SchedulingEmbedder::OnEmbeddingsComputed(std::vector<std::string> passages,
                                              std::vector<Embedding> embeddings,
                                              ComputeEmbeddingsStatus status) {
  VLOG(3) << embeddings.size() << " embeddings computed for " << passages.size()
          << " passages with status " << static_cast<int>(status);
  CHECK_EQ(passages.size(), embeddings.size());

  if (embeddings.empty()) {
    Job& job = working_jobs_.front();
    VLOG(2) << "Aborted embedding work for " << job.passages.size()
            << " passages starting with `" << job.passages[0] << "`";
    std::move(job.callback).Run({}, {}, status);
    working_jobs_.pop_front();
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
    Job& job = working_jobs_.front();
    while (job.embeddings.size() < job.passages.size() &&
           read_index < embeddings.size()) {
      CHECK_EQ(job.passages[job.embeddings.size()], passages[read_index]);
      job.embeddings.push_back(std::move(embeddings[read_index]));
      read_index++;
    }
    if (job.embeddings.size() == job.passages.size()) {
      VLOG(2) << "Finished embedding work for " << job.passages.size()
              << " passages starting with `" << job.passages[0] << "`";
      std::move(job.callback)
          .Run(std::move(job.passages), std::move(job.embeddings), status);
      working_jobs_.pop_front();
    }
  }

  // Note, this could call back later/asynchronously or
  // immediately/synchronously, depending on the embedder.
  SubmitWorkToEmbedder();
}

}  // namespace history_embeddings
