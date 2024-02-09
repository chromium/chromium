// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_EXTERNAL_POLICY_DATA_FETCHER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_EXTERNAL_POLICY_DATA_FETCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/policy/policy_export.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}

namespace network {
class PendingSharedURLLoaderFactory;
class SharedURLLoaderFactory;
}

namespace policy {

// This class handles network fetch jobs for the ExternalPolicyDataUpdater by
// forwarding them to a job running on a different thread.
// The class can be instantiated on any thread but from then on, it must be
// accessed and destroyed on the background thread that the
// ExternalPolicyDataUpdater runs on only.
class POLICY_EXPORT ExternalPolicyDataFetcher {
 public:
  // The result of a fetch job.
  enum Result {
    // Successful fetch.
    SUCCESS,
    // The connection was interrupted.
    CONNECTION_INTERRUPTED,
    // Another network error occurred.
    NETWORK_ERROR,
    // Problem at the server.
    SERVER_ERROR,
    // Client error.
    CLIENT_ERROR,
    // Any other type of HTTP failure.
    HTTP_ERROR,
    // Received data exceeds maximum allowed size.
    MAX_SIZE_EXCEEDED,
  };

  // Encapsulates the state for a fetch job.
  class Job;

  // Callback invoked when a fetch job finishes. If the fetch was successful,
  // the Result is SUCCESS and the scoped_ptr contains the retrieved data.
  // Otherwise, Result indicates the type of error that occurred and the
  // scoped_ptr is NULL.
  using FetchCallback =
      base::OnceCallback<void(Result, std::unique_ptr<std::string>)>;

  // |task_runner| represents the background thread that |this| runs on.
  ExternalPolicyDataFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ExternalPolicyDataFetcher(const ExternalPolicyDataFetcher&) = delete;
  ExternalPolicyDataFetcher& operator=(const ExternalPolicyDataFetcher&) =
      delete;
  ~ExternalPolicyDataFetcher();

  // Fetch data from |url| and invoke |callback| with the result. See the
  // documentation of FetchCallback and Result for more details. If a fetch
  // should be retried after an error, it is the caller's responsibility to call
  // StartJob() again. Returns an opaque job identifier. Ownership of the job
  // identifier is retained by |this|.
  Job* StartJob(const GURL& url, int64_t max_size, FetchCallback callback);

  // Cancel the fetch job identified by |job|. The job is canceled silently,
  // without invoking the |callback| that was passed to StartJob().
  void CancelJob(Job* job);

 private:
  // Invoked when a fetch job finishes in the |backend_|.
  void OnJobFinished(FetchCallback callback,
                     Job* job,
                     Result result,
                     std::unique_ptr<std::string> data);

  // Task runner representing the thread that |this| runs on.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Task runner for running the fetch jobs. It's the task runner on which this
  // instance was created.
  const scoped_refptr<base::SequencedTaskRunner> job_task_runner_;

  // The information for the lazy creation of |cloned_url_loader_factory_|.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;
  // The cloned factory that can be used from |task_runner_|. It's created
  // lazily, as our constructor runs on a difference sequence.
  scoped_refptr<network::SharedURLLoaderFactory> cloned_url_loader_factory_;

  // Set that owns all currently running Jobs.
  typedef std::set<raw_ptr<Job, SetExperimental>> JobSet;
  JobSet jobs_;

  base::WeakPtrFactory<ExternalPolicyDataFetcher> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_EXTERNAL_POLICY_DATA_FETCHER_H_
