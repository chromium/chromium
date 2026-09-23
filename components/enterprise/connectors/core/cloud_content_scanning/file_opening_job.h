// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILE_OPENING_JOB_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILE_OPENING_JOB_H_

#include <atomic>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_job.h"
#include "base/time/time.h"

namespace enterprise_connectors {
class FileAnalysisRequestBase;
}  // namespace enterprise_connectors

namespace safe_browsing {

// This class encapsulates a base::PostJob call made to open multiple files to
// set up multiple FileAnalysisRequestBases to avoid using too many system
// resources.
class FileOpeningJob : public base::RefCounted<FileOpeningJob> {
 public:
  // Struct to store data necessary for a synchronized file opening task.
  struct FileOpeningTask {
    FileOpeningTask();
    ~FileOpeningTask();

    // Non-owning pointer to the request corresponding to the file to open.
    raw_ptr<enterprise_connectors::FileAnalysisRequestBase,
            AcrossTasksDanglingUntriaged>
        request = nullptr;

    // Indicates if this task has been taken and is owned by a thread.
    std::atomic_bool taken{false};
  };

  static size_t GetMaxFileOpeningThreads();

  // Cancels the file opening job. Background tasks will abort on their next
  // check of is_cancelled(), rather than having to wait for the job's
  // destruction.
  void Cancel();

  // Signals cancellation without waiting for in-progress work to finish.
  // Unlike Cancel(), this does not touch `file_opening_job_handle_`, so it
  // never blocks the calling thread. Workers observe `is_cancelled_` and
  // unwind on their own, and the blocking join is left to the destructor, by
  // which point there is typically nothing left to wait for. This is the path
  // taken when the user actively cancels a scan from the UI.
  void SignalCancelled();

  // This constructor will call base::PostJob on the given tasks and only cancel
  // that job in the destructor. This means that if the result of the tasks is
  // no longer useful (if a folder upload is cancelled for instance), `this`
  // should be deleted so that resources aren't spent opening files.
  explicit FileOpeningJob(std::vector<FileOpeningTask> tasks);

 private:
  FRIEND_TEST_ALL_PREFIXES(FileOpeningJobTest, MaxThreadsFlag);
  friend class base::RefCounted<FileOpeningJob>;
  ~FileOpeningJob();

  // Records when cancellation was first requested and whether any file work
  // was in flight at that moment. Idempotent: only the first call has an
  // effect, so the timestamp always reflects the earliest cancellation signal.
  // Called unconditionally by both cancellation entry points, including when
  // the features below are disabled, so that all experiment arms report over
  // the same population.
  void RecordCancelIntent();

  // Marks every task that no worker has taken yet as taken and cancels its
  // request, releasing that request's reference to `this`. Does not block.
  void CancelUntakenTasks();

  // Cancels `file_opening_job_handle_` if it is set, and records how long the
  // calling thread was blocked doing so. base::JobHandle::Cancel() forces
  // workers to yield and does not return until they have all returned from
  // ProcessNextTask(), so this blocks the caller (the UI thread).
  void CancelJobHandle();

  // Processes the next file opening task that hasn't been taken so far. This is
  // the main callback passed to base::PostJob and it will run concurrently on
  // multiple threads.
  void ProcessNextTask(base::JobDelegate* job_delegate);

  // Synchronized getter method to read `num_unopened_files_`.
  size_t num_unopened_files();

  // Returns the maximum number of threads that should be opening files. This
  // is dependant on the corresponding flags and on the number of remaining
  // files to open. As this is meant to be used by base::PostJob, this method
  // needs a `worker_count` argument in its signature, even though it is unused.
  size_t MaxConcurrentThreads(size_t /*worker_count*/);

  // Returns whether this job has been cancelled.
  bool is_cancelled() const {
    return is_cancelled_.load(std::memory_order_relaxed);
  }

  // Initialized with the size of `tasks_` in the constructor. This should
  // always be accessed through its corresponding getter method to avoid
  // synchronization bugs.
  std::atomic_size_t num_unopened_files_;

  // Number of tasks currently being processed by a worker thread, i.e. threads
  // currently inside FileAnalysisRequestBase::OpenFile(). Used to tell whether
  // a cancellation actually had to wait on in-progress file work.
  std::atomic_size_t num_active_tasks_{0};

  // Points to the job initialized in the constructor.
  base::JobHandle file_opening_job_handle_;

  std::vector<FileOpeningTask> tasks_;
  size_t max_threads_;

  // If true, this job has been cancelled and will not process any more tasks.
  std::atomic<bool> is_cancelled_{false};

  // Timestamp of when Cancel() was called to measure cancellation duration.
  base::TimeTicks cancel_time_;

  // Whether any file was still being opened or hashed when Cancel() was
  // called. Cancellations of already drained jobs complete instantly and would
  // otherwise dominate the cancellation latency histograms.
  bool cancelled_while_in_flight_ = false;

  // Ensures the blocking duration is only recorded for the first
  // base::JobHandle::Cancel() call, since both Cancel() and the destructor may
  // reach CancelJobHandle().
  bool blocking_duration_recorded_ = false;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_FILE_OPENING_JOB_H_
