// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_EXTERNAL_POLICY_DATA_UPDATER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_EXTERNAL_POLICY_DATA_UPDATER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class ExternalPolicyDataFetcher;

// This class downloads external policy data. Given a |Request|, data is fetched
// from the |url|, verified to not exceed |max_size| and to match the expected
// |hash| and then handed to a callback that can do further verification before
// finally deciding whether the fetched data is valid.
// If a fetch is not successful or retrieves invalid data, retries are scheduled
// with exponential backoff.
// The actual fetching is handled by an ExternalPolicyDataFetcher, allowing this
// class to run on a background thread where network I/O is not possible.
class POLICY_EXPORT ExternalPolicyDataUpdater {
 public:
  struct POLICY_EXPORT Request {
   public:
    Request();
    Request(const std::string& url, const std::string& hash, int64_t max_size);

    bool operator==(const Request& other) const;

    std::string url;
    std::string hash;
    int64_t max_size;
  };

  // This callback is invoked when a fetch has successfully retrieved |data|
  // that does not exceed |max_size| and matches the expected |hash|. The
  // callback can do further verification to decide whether the fetched data is
  // valid.
  // If the callback returns |true|, the data is accepted and the |Request| is
  // finished. If the callback returns |false|, the data is rejected and the
  // fetch is retried after a long backoff. Note that in this case, the callback
  // may be invoked multiple times as the fetch is repeated. Make sure to not
  // bind base::Passed() scoped_ptrs to the callback in such cases as these
  // become invalid after a callback has been run once. base::Owned() can be
  // used in all cases.
  typedef base::RepeatingCallback<bool(const std::string&)>
      FetchSuccessCallback;

  // This class runs on the background thread represented by |task_runner|,
  // which must support file I/O. All network I/O is forwarded to a different
  // thread by the |external_policy_data_fetcher|.
  ExternalPolicyDataUpdater(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher,
      size_t max_parallel_fetches);
  ExternalPolicyDataUpdater(const ExternalPolicyDataUpdater&) = delete;
  ExternalPolicyDataUpdater& operator=(const ExternalPolicyDataUpdater&) =
      delete;
  ~ExternalPolicyDataUpdater();

  // Fetches the external data specified in the |request|. The |key| is an
  // opaque identifier. If another request for the same |key| is still pending,
  // it will be canceled and replaced with the new |request|. The callback will
  // be invoked after a successful fetch. See the documentation of
  // |FetchSuccessCallback| for more details.
  void FetchExternalData(const std::string& key,
                         const Request& request,
                         const FetchSuccessCallback& callback);

  // Cancels the pending request identified by |key|. If no such request is
  // pending, does nothing.
  void CancelExternalDataFetch(const std::string& key);

 private:
  class FetchJob;

  // Starts jobs from the |job_queue_| until |max_parallel_jobs_| are running or
  // the queue is depleted.
  void StartNextJobs();

  // Appends |job| to the |job_queue_| and starts it immediately if less than
  // |max_parallel_jobs_| are running.
  void ScheduleJob(FetchJob* job);

  // Callback for jobs that succeeded.
  void OnJobSucceeded(FetchJob* job);

  // Callback for jobs that failed.
  void OnJobFailed(FetchJob* job);

  // |true| once the destructor starts. Prevents jobs from being started during
  // shutdown.
  bool shutting_down_ = false;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const std::unique_ptr<ExternalPolicyDataFetcher>
      external_policy_data_fetcher_;

  // The maximum number of jobs to run in parallel.
  const size_t max_parallel_jobs_;

  // The number of jobs currently running.
  size_t running_jobs_ = 0;

  // Queue of jobs waiting to be run. Jobs are taken off the queue and started
  // by StartNextJobs().
  base::queue<base::WeakPtr<FetchJob>> job_queue_;

  // Map that owns all existing jobs, regardless of whether they are currently
  // queued, running or waiting for a retry.
  std::map<std::string, std::unique_ptr<FetchJob>> job_map_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_EXTERNAL_POLICY_DATA_UPDATER_H_
