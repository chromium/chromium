// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/file_opening_job.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/timer/elapsed_timer.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"

namespace safe_browsing {

namespace {

constexpr size_t kDefaultMaxFileOpeningThreads = 5;

}  // namespace

// static
size_t FileOpeningJob::GetMaxFileOpeningThreads() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWpMaxFileOpeningThreads)) {
    int parsed_max;
    if (base::StringToInt(command_line->GetSwitchValueASCII(
                              switches::kWpMaxFileOpeningThreads),
                          &parsed_max) &&
        parsed_max > 0) {
      return parsed_max;
    } else {
      LOG(ERROR) << switches::kWpMaxFileOpeningThreads << " had invalid value";
    }
  }

  return kDefaultMaxFileOpeningThreads;
}

FileOpeningJob::FileOpeningTask::FileOpeningTask() = default;
FileOpeningJob::FileOpeningTask::~FileOpeningTask() = default;

FileOpeningJob::FileOpeningJob(std::vector<FileOpeningTask> tasks)
    : tasks_(std::move(tasks)), max_threads_(GetMaxFileOpeningThreads()) {
  for (const auto& task : tasks_) {
    task.request->set_file_opening_job(base::WrapRefCounted(this));
  }
  num_unopened_files_ = tasks_.size();

  // The base::Unretained calls are safe because `file_opening_job_handle_` is
  // destroyed when `this` is.
  file_opening_job_handle_ =
      base::PostJob(FROM_HERE,
                    {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
                     base::ThreadPolicy::PREFER_BACKGROUND},
                    base::BindRepeating(&FileOpeningJob::ProcessNextTask,
                                        base::Unretained(this)),
                    base::BindRepeating(&FileOpeningJob::MaxConcurrentThreads,
                                        base::Unretained(this)));
}

void FileOpeningJob::RecordCancelIntent() {
  // Cancellation is signalled more than once per job (SignalCancelled(), then
  // Cancel() twice during teardown). Only the earliest one is the user's.
  if (!cancel_time_.is_null()) {
    return;
  }
  cancel_time_ = base::TimeTicks::Now();

  // Recorded before any feature check so all arms classify the same population.
  cancelled_while_in_flight_ =
      num_active_tasks_.load(std::memory_order_relaxed) > 0;
}

void FileOpeningJob::CancelUntakenTasks() {
  // Cancel all tasks that have not been taken yet. That ensures that the
  // request will release the reference to *this.
  for (auto& task : tasks_) {
    if (!task.taken.exchange(true, std::memory_order_relaxed)) {
      task.request->Cancel();
    }
  }
}

void FileOpeningJob::CancelJobHandle() {
  if (!file_opening_job_handle_) {
    return;
  }
  // JobHandle::Cancel() blocks the UI thread until every worker has returned
  // from ProcessNextTask(). That stall is the user visible cost of cancelling,
  // so time it separately from overall teardown.
  base::ElapsedTimer blocking_timer;
  file_opening_job_handle_.Cancel();

  // Only for user cancellations, and only once: normal completion also reaches
  // here via the destructor.
  if (cancel_time_.is_null() || blocking_duration_recorded_) {
    return;
  }
  blocking_duration_recorded_ = true;
  const base::TimeDelta blocking_duration = blocking_timer.Elapsed();
  base::UmaHistogramMediumTimes(
      "Enterprise.FileOpeningJob.CancelBlockingDuration", blocking_duration);
  if (cancelled_while_in_flight_) {
    // The idle case is not split out: it is by construction a no-op join and
    // is already identifiable from CancelledWhileInFlight.
    base::UmaHistogramMediumTimes(
        "Enterprise.FileOpeningJob.CancelBlockingDuration.InFlight",
        blocking_duration);
  }
}

void FileOpeningJob::SignalCancelled() {
  RecordCancelIntent();

  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableCancelUploadOnContentAnalysis)) {
    return;
  }

  // Deliberately no CancelJobHandle(): `is_cancelled_` alone stops workers
  // taking new tasks and aborts an in-progress hash at its next chunk. Waiting
  // for them to return is the destructor's job.
  is_cancelled_.store(true, std::memory_order_relaxed);
  CancelUntakenTasks();
}

void FileOpeningJob::Cancel() {
  RecordCancelIntent();

  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableCancelUploadOnContentAnalysis)) {
    return;
  }

  // Store the atomic bool first to ensure that all threads see the cancellation
  // before the job handle is cancelled.
  is_cancelled_.store(true, std::memory_order_relaxed);
  CancelJobHandle();
  CancelUntakenTasks();
}

FileOpeningJob::~FileOpeningJob() {
  is_cancelled_.store(true, std::memory_order_relaxed);
  CancelJobHandle();

  if (cancel_time_.is_null()) {
    return;
  }

  // Guardrail only: its start point differs between arms, so prefer
  // CancelBlockingDuration when comparing them.
  base::UmaHistogramMediumTimes("Enterprise.FileOpeningJob.CancelDuration",
                                base::TimeTicks::Now() - cancel_time_);
  base::UmaHistogramBoolean("Enterprise.FileOpeningJob.CancelledWhileInFlight",
                            cancelled_while_in_flight_);
}

void FileOpeningJob::ProcessNextTask(base::JobDelegate* job_delegate) {
  // Loop over `tasks_` until one can safely be taken by this thread.
  for (size_t i = 0; i < tasks_.size() && num_unopened_files() != 0 &&
                     !job_delegate->ShouldYield();
       ++i) {
    if (is_cancelled()) {
      break;
    }
    // The task's `taken` value is atomic, so exchanging it to find it used to
    // be true indicates we were the not the thread that took it.
    // std::memory_order_relaxed is safe here since `taken` is not synchronized
    // with other state.
    if (tasks_[i].taken.exchange(true, std::memory_order_relaxed)) {
      continue;
    }

    // `num_active_tasks_` is held across the call so that Cancel() can tell
    // whether it is about to wait on real work.
    num_active_tasks_.fetch_add(1, std::memory_order_relaxed);
    tasks_[i].request->OpenFile(&is_cancelled_);
    num_active_tasks_.fetch_sub(1, std::memory_order_relaxed);

    // Now that the file opening work is done, `num_unopened_files_` is
    // decremented atomically and we return to free the thread.
    num_unopened_files_.fetch_sub(1, std::memory_order_relaxed);
    return;
  }
}

size_t FileOpeningJob::num_unopened_files() {
  return num_unopened_files_.load(std::memory_order_relaxed);
}

size_t FileOpeningJob::MaxConcurrentThreads(size_t /*worker_count*/) {
  // A cancelled job needs no workers. Without this, cancelled tasks never
  // decrement `num_unopened_files_`, so the scheduler keeps re-invoking
  // ProcessNextTask() and spins until the job is destroyed.
  if (is_cancelled()) {
    return 0;
  }
  return std::min(num_unopened_files(), max_threads_);
}

}  // namespace safe_browsing
