// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/external_policy_data_updater.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/policy_logger.h"
#include "crypto/sha2.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Policies for exponential backoff of failed requests. There are 3 policies for
// different classes of errors.

// For temporary errors (HTTP 500, RST, etc).
const net::BackoffEntry::Policy kRetrySoonPolicy = {
    // Number of initial errors to ignore before starting to back off.
    0,

    // Initial delay in ms: 15 seconds.
    1000 * 15,

    // Factor by which the waiting time is multiplied.
    2,

    // Fuzzing percentage; this spreads delays randomly between 80% and 100%
    // of the calculated time.
    0.20,

    // Maximum delay in ms: 12 hours.
    1000 * 60 * 60 * 12,

    // When to discard an entry: never.
    -1,

    // |always_use_initial_delay|; false means that the initial delay is
    // applied after the first error, and starts backing off from there.
    false,
};

// For other errors (request failed, server errors).
const net::BackoffEntry::Policy kRetryLaterPolicy = {
    // Number of initial errors to ignore before starting to back off.
    0,

    // Initial delay in ms: 1 minute.
    1000 * 60,

    // Factor by which the waiting time is multiplied.
    2,

    // Fuzzing percentage; this spreads delays randomly between 80% and 100%
    // of the calculated time.
    0.20,

    // Maximum delay in ms: 12 hours.
    1000 * 60 * 60 * 12,

    // When to discard an entry: never.
    -1,

    // |always_use_initial_delay|; false means that the initial delay is
    // applied after the first error, and starts backing off from there.
    false,
};

// When the data fails validation (maybe because the policy URL and the data
// served at that URL are out of sync). This essentially retries every 12 hours,
// with some random jitter.
const net::BackoffEntry::Policy kRetryMuchLaterPolicy = {
    // Number of initial errors to ignore before starting to back off.
    0,

    // Initial delay in ms: 12 hours.
    1000 * 60 * 60 * 12,

    // Factor by which the waiting time is multiplied.
    2,

    // Fuzzing percentage; this spreads delays randomly between 80% and 100%
    // of the calculated time.
    0.20,

    // Maximum delay in ms: 12 hours.
    1000 * 60 * 60 * 12,

    // When to discard an entry: never.
    -1,

    // |always_use_initial_delay|; false means that the initial delay is
    // applied after the first error, and starts backing off from there.
    false,
};

// Maximum number of retries for requests that aren't likely to get a
// different response (e.g. HTTP 4xx replies).
const int kMaxLimitedRetries = 3;

}  // namespace

class ExternalPolicyDataUpdater::FetchJob final {
 public:
  FetchJob(ExternalPolicyDataUpdater* updater,
           const std::string& key,
           const ExternalPolicyDataUpdater::Request& request,
           const ExternalPolicyDataUpdater::FetchSuccessCallback& callback);
  FetchJob(const FetchJob&) = delete;
  FetchJob& operator=(const FetchJob&) = delete;
  virtual ~FetchJob();

  const std::string& key() const;
  const ExternalPolicyDataUpdater::Request& request() const;

  void Start();

  void OnFetchFinished(ExternalPolicyDataFetcher::Result result,
                       std::unique_ptr<std::string> data);

  bool IsRescheduleWithDelayRunning() const {
    return is_reschedule_with_delay_running_;
  }

  base::WeakPtr<FetchJob> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  void OnFailed(net::BackoffEntry* backoff_entry);
  void Reschedule();

  // Always valid as long as |this| is alive.
  const raw_ptr<ExternalPolicyDataUpdater> updater_;

  const std::string key_;
  const ExternalPolicyDataUpdater::Request request_;
  const ExternalPolicyDataUpdater::FetchSuccessCallback callback_;

  // If the job is currently running, a corresponding |fetch_job_| exists in the
  // |external_policy_data_fetcher_|. The job must eventually call back to the
  // |updater_|'s OnJobSucceeded() or OnJobFailed() method in this case.
  // If the job is currently not running, |fetch_job_| is NULL and no callbacks
  // should be invoked.
  raw_ptr<ExternalPolicyDataFetcher::Job> fetch_job_ = nullptr;  // Not owned.

  // Some errors should trigger a limited number of retries, even with backoff.
  // This counts down the number of such retries to stop retrying once the limit
  // is reached.
  int limited_retries_remaining_ = kMaxLimitedRetries;

  // Indicates that job rescheduling task is running. In this state the
  // job is not fetching any data.
  int is_reschedule_with_delay_running_ = false;

  // Various delays to retry a failed download, depending on the failure reason.
  net::BackoffEntry retry_soon_entry_{&kRetrySoonPolicy};
  net::BackoffEntry retry_later_entry_{&kRetryLaterPolicy};
  net::BackoffEntry retry_much_later_entry_{&kRetryMuchLaterPolicy};

  base::WeakPtrFactory<FetchJob> weak_factory_{this};
};

ExternalPolicyDataUpdater::Request::Request() = default;

ExternalPolicyDataUpdater::Request::Request(const std::string& url,
                                            const std::string& hash,
                                            int64_t max_size)
    : url(url), hash(hash), max_size(max_size) {}

bool ExternalPolicyDataUpdater::Request::operator==(
    const Request& other) const {
  return url == other.url && hash == other.hash && max_size == other.max_size;
}

ExternalPolicyDataUpdater::FetchJob::FetchJob(
    ExternalPolicyDataUpdater* updater,
    const std::string& key,
    const ExternalPolicyDataUpdater::Request& request,
    const ExternalPolicyDataUpdater::FetchSuccessCallback& callback)
    : updater_(updater), key_(key), request_(request), callback_(callback) {}

ExternalPolicyDataUpdater::FetchJob::~FetchJob() {
  if (fetch_job_) {
    // Cancel the fetch job in the |external_policy_data_fetcher_|.
    updater_->external_policy_data_fetcher_->CancelJob(fetch_job_);
    // Inform the |updater_| that the job was canceled.
    updater_->OnJobFailed(this);
  }
}

const std::string& ExternalPolicyDataUpdater::FetchJob::key() const {
  return key_;
}

const ExternalPolicyDataUpdater::Request&
ExternalPolicyDataUpdater::FetchJob::request() const {
  return request_;
}

void ExternalPolicyDataUpdater::FetchJob::Start() {
  DCHECK(!fetch_job_);
  DVLOG_POLICY(1, POLICY_FETCHING)
      << "Fetching data for " << key_ << " from " << request_.url << " .";
  // Start a fetch job in the |external_policy_data_fetcher_|. This will
  // eventually call back to OnFetchFinished() with the result.
  // Passing |this| as base::Unretained() is safe here because the |FetchJob|
  // destructor cancels the fetcher job if one is still running.
  fetch_job_ = updater_->external_policy_data_fetcher_->StartJob(
      GURL(request_.url), request_.max_size,
      base::BindOnce(&ExternalPolicyDataUpdater::FetchJob::OnFetchFinished,
                     base::Unretained(this)));
}

void ExternalPolicyDataUpdater::FetchJob::OnFetchFinished(
    ExternalPolicyDataFetcher::Result result,
    std::unique_ptr<std::string> data) {
  // The fetch job in the |external_policy_data_fetcher_| is finished.
  fetch_job_ = nullptr;

  switch (result) {
    case ExternalPolicyDataFetcher::CONNECTION_INTERRUPTED:
      // The connection was interrupted. Try again soon.
      DVLOG_POLICY(1, POLICY_FETCHING)
          << "Failed to fetch the data due to the interrupted connection.";
      OnFailed(&retry_soon_entry_);
      return;
    case ExternalPolicyDataFetcher::NETWORK_ERROR:
      // Another network error occurred. Try again later.
      DVLOG_POLICY(1, POLICY_FETCHING)
          << "Failed to fetch the data due to a network error.";
      OnFailed(&retry_later_entry_);
      return;
    case ExternalPolicyDataFetcher::SERVER_ERROR:
      // Problem at the server. Try again soon.
      LOG_POLICY(WARNING, POLICY_FETCHING)
          << "Failed to fetch the data due to a server HTTP error.";
      OnFailed(&retry_soon_entry_);
      return;
    case ExternalPolicyDataFetcher::CLIENT_ERROR:
      // Client error. This is unlikely to go away. Try again later, and give up
      // retrying after 3 attempts.
      LOG_POLICY(WARNING, POLICY_FETCHING)
          << "Failed to fetch the data due to a client HTTP error.";
      OnFailed(limited_retries_remaining_ ? &retry_later_entry_ : nullptr);
      if (limited_retries_remaining_)
        --limited_retries_remaining_;
      return;
    case ExternalPolicyDataFetcher::HTTP_ERROR:
      // Any other type of HTTP failure. Try again later.
      LOG_POLICY(WARNING, POLICY_FETCHING)
          << "Failed to fetch the data due to an HTTP error.";
      OnFailed(&retry_later_entry_);
      return;
    case ExternalPolicyDataFetcher::MAX_SIZE_EXCEEDED:
      // Received |data| exceeds maximum allowed size. This may be because the
      // data being served is stale. Try again much later.
      LOG_POLICY(WARNING, POLICY_FETCHING)
          << "Failed to fetch the data due to the excessive size (max "
          << request_.max_size << " bytes).";
      OnFailed(&retry_much_later_entry_);
      return;
    case ExternalPolicyDataFetcher::SUCCESS:
      break;
  }

  if (crypto::SHA256HashString(*data) != request_.hash) {
    // Received |data| does not match expected hash. This may be because the
    // data being served is stale. Try again much later.
    LOG_POLICY(ERROR, POLICY_FETCHING)
        << "The fetched data doesn't match the expected hash.";
    OnFailed(&retry_much_later_entry_);
    return;
  }

  // If the callback rejects the data, try again much later.
  if (!callback_.Run(*data)) {
    OnFailed(&retry_much_later_entry_);
    return;
  }

  // Signal success.
  updater_->OnJobSucceeded(this);
}

void ExternalPolicyDataUpdater::FetchJob::OnFailed(net::BackoffEntry* entry) {
  if (entry) {
    entry->InformOfRequest(false);
    const base::TimeDelta delay = entry->GetTimeUntilRelease();
    DVLOG_POLICY(1, POLICY_FETCHING)
        << "Rescheduling the fetch in " << delay << ".";

    is_reschedule_with_delay_running_ = true;
    // This function may have been invoked because the job was obsoleted and is
    // in the process of being deleted. If this is the case, the WeakPtr will
    // become invalid and the delayed task will never run.
    updater_->task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FetchJob::Reschedule, weak_factory_.GetWeakPtr()),
        delay);
  }

  updater_->OnJobFailed(this);
}

void ExternalPolicyDataUpdater::FetchJob::Reschedule() {
  is_reschedule_with_delay_running_ = false;
  updater_->ScheduleJob(this);
}

ExternalPolicyDataUpdater::ExternalPolicyDataUpdater(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher,
    size_t max_parallel_fetches)
    : task_runner_(task_runner),
      external_policy_data_fetcher_(std::move(external_policy_data_fetcher)),
      max_parallel_jobs_(max_parallel_fetches) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

ExternalPolicyDataUpdater::~ExternalPolicyDataUpdater() {
  // No RunsTasksInCurrentSequence() check to avoid unit tests failures.
  // In unit tests the browser process instance is deleted only after test ends
  // and test task scheduler is shutted down. Therefore we need to delete some
  // components of BrowserPolicyConnector (ResourceCache and
  // CloudExternalDataManagerBase::Backend) manually when task runner doesn't
  // accept new tasks (DeleteSoon in this case). This leads to the situation
  // when this destructor is called not on |task_runner|.

  // Raise the flag to prevent jobs from being started during the destruction of
  // |job_map_|.
  shutting_down_ = true;
}

void ExternalPolicyDataUpdater::FetchExternalData(
    const std::string& key,
    const Request& request,
    const FetchSuccessCallback& callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Check whether a job exists for this |key| already.
  FetchJob* job = job_map_[key].get();
  if (job) {
    // We should cancel the job which has been rescheduled for the future to
    // avoid potentially long delays in data fetching.
    if (!job->IsRescheduleWithDelayRunning()) {
      // If the current |job| is handling the given |request| already, nothing
      // needs to be done.
      if (job->request() == request) {
        DVLOG_POLICY(2, POLICY_FETCHING)
            << "Fetching job already scheduled for " << key
            << " with the same parameters.";
        return;
      }
    }

    // Otherwise, the current |job| is obsolete. If the |job| is on the queue,
    // its WeakPtr will be invalidated and skipped by StartNextJobs(). If |job|
    // is currently running, it will call OnJobFailed() immediately.
    DVLOG_POLICY(2, POLICY_FETCHING)
        << "Removing the old job for " << key << ".";
    job_map_.erase(key);
  }

  // Start a new job to handle |request|.
  job = new FetchJob(this, key, request, callback);
  job_map_[key] = base::WrapUnique(job);
  ScheduleJob(job);
}

void ExternalPolicyDataUpdater::CancelExternalDataFetch(
    const std::string& key) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // If a |job| exists for this |key|, delete it. If the |job| is on the queue,
  // its WeakPtr will be invalidated and skipped by StartNextJobs(). If |job| is
  // currently running, it will call OnJobFailed() immediately.
  auto job = job_map_.find(key);
  if (job != job_map_.end()) {
    DVLOG_POLICY(1, POLICY_FETCHING) << "Cancelling the job for " << key << ".";
    job_map_.erase(job);
  }
}

void ExternalPolicyDataUpdater::StartNextJobs() {
  if (shutting_down_)
    return;

  while (running_jobs_ < max_parallel_jobs_ && !job_queue_.empty()) {
    FetchJob* job = job_queue_.front().get();
    job_queue_.pop();

    // Some of the jobs may have been invalidated, and have to be skipped.
    if (job) {
      ++running_jobs_;
      // A started job will always call OnJobSucceeded() or OnJobFailed().
      job->Start();
    }
  }
}

void ExternalPolicyDataUpdater::ScheduleJob(FetchJob* job) {
  DCHECK_EQ(job_map_[job->key()].get(), job);

  job_queue_.push(job->AsWeakPtr());

  StartNextJobs();
}

void ExternalPolicyDataUpdater::OnJobSucceeded(FetchJob* job) {
  DCHECK(running_jobs_);
  DCHECK_EQ(job_map_[job->key()].get(), job);

  --running_jobs_;
  job_map_.erase(job->key());

  StartNextJobs();
}

void ExternalPolicyDataUpdater::OnJobFailed(FetchJob* job) {
  DCHECK(running_jobs_);
  --running_jobs_;

  // Don't touch |job_map_|; deletion of |FetchJob|s causes a call to this
  // method, so |job_map_| is possibly in an inconsistent state.

  // The job is not deleted when it fails because a retry attempt may have been
  // scheduled.
  StartNextJobs();
}

}  // namespace policy
