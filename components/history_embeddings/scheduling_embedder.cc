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
                                       size_t scheduled_max)
    : embedder_(std::move(embedder)),
      scheduled_max_(scheduled_max) {}

SchedulingEmbedder::~SchedulingEmbedder() = default;

void SchedulingEmbedder::ComputePassagesEmbeddings(
    PassageKind kind,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  // Zero size jobs are expected, and can be called back immediately
  // instead of waiting in line for nothing.
  if (passages.empty()) {
    std::move(callback).Run(
        /*passages=*/{}, /*embeddings=*/{}, ComputeEmbeddingsStatus::SUCCESS);
    return;
  }

  // Only start work if none is in progress. If work is already begun
  // then it will continue later when embeddings are returned.
  bool submit = jobs_.empty();

  jobs_.emplace_back(kind, std::move(passages), std::move(callback));

  if (submit) {
    SubmitWorkToEmbedder();
  }
}

void SchedulingEmbedder::SubmitWorkToEmbedder() {
  if (!embedder_ready_) {
    // Underlying embedder not ready yet. Wait for it.
    return;
  }

  if (jobs_.empty()) {
    // No jobs to start.
    return;
  }

  // Put higher priority jobs at the front. This may suspend partially
  // completed jobs of lower priority by pushing them toward the back.
  std::stable_sort(jobs_.begin(), jobs_.end(),
                   [](const Job& a, const Job& b) { return a.kind < b.kind; });

  // When submitting a query, only the latest is kept, and old waiting queries
  // can be removed. They will be contiguous at the front due to above sort.
  if (jobs_.front().kind == PassageKind::QUERY) {
    while (jobs_.size() > 1 && jobs_.at(1).kind == PassageKind::QUERY) {
      VLOG(2) << "Dropped pending query '" << jobs_.front().passages[0]
              << "'. Next query: '" << jobs_.at(1).passages[0] << "'";
      std::move(jobs_.front().callback)
          .Run({}, {}, ComputeEmbeddingsStatus::SKIPPED);
      jobs_.pop_front();
    }
  }

  // Submit a batch of passages taken from jobs near the front of the queue.
  // Only submit one kind of passage, regardless of count.
  PassageKind kind = jobs_.front().kind;
  std::vector<std::string> passages;
  size_t job_index = 0;
  while (passages.size() < scheduled_max_ && job_index < jobs_.size() &&
         jobs_.at(job_index).kind == kind) {
    const Job& job = jobs_.at(job_index);
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
  embedder_->ComputePassagesEmbeddings(
      kind, std::move(passages),
      base::BindOnce(&SchedulingEmbedder::OnEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr()));
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

  // Work doesn't start until after the embedder is ready. There may or may not
  // be jobs waiting, but no work is in progress yet, so it can be started now.
  SubmitWorkToEmbedder();
}

void SchedulingEmbedder::OnEmbeddingsComputed(std::vector<std::string> passages,
                                              std::vector<Embedding> embeddings,
                                              ComputeEmbeddingsStatus status) {
  VLOG(3) << embeddings.size() << " embeddings computed for " << passages.size()
          << " passages with status " << static_cast<int>(status);
  CHECK_EQ(passages.size(), embeddings.size());

  if (embeddings.empty()) {
    Job& job = jobs_.front();
    VLOG(2) << "Aborted embedding work for " << job.passages.size()
            << " passages starting with `"
            << (job.passages.empty() ? "" : job.passages[0]) << "`";
    std::move(job.callback).Run({}, {}, status);
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
      VLOG(2) << "Finished embedding work for " << job.passages.size()
              << " passages starting with `" << job.passages[0] << "`";
      std::move(job.callback)
          .Run(std::move(job.passages), std::move(job.embeddings), status);
      jobs_.pop_front();
    }
  }

  // Note, this could call back later/asynchronously or
  // immediately/synchronously, depending on the embedder.
  SubmitWorkToEmbedder();
}

}  // namespace history_embeddings
