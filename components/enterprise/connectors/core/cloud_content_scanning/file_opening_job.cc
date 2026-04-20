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

void FileOpeningJob::Cancel() {
  cancel_time_ = base::TimeTicks::Now();

  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableCancelUploadOnContentAnalysis)) {
    return;
  }

  // Store the atomic bool first to ensure that all threads see the cancellation
  // before the job handle is cancelled.
  is_cancelled_.store(true, std::memory_order_relaxed);
  if (file_opening_job_handle_) {
    file_opening_job_handle_.Cancel();
  }

  // Cancel all tasks that have not been taken yet. That ensures that the
  // request will release the reference to *this.
  for (auto& task : tasks_) {
    if (!task.taken.exchange(true, std::memory_order_relaxed)) {
      task.request->Cancel();
    }
  }
}

FileOpeningJob::~FileOpeningJob() {
  if (file_opening_job_handle_) {
    file_opening_job_handle_.Cancel();
  }
  if (!cancel_time_.is_null()) {
    base::UmaHistogramMediumTimes("Enterprise.FileOpeningJob.CancelDuration",
                                  base::TimeTicks::Now() - cancel_time_);
  }
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

    // Since we know we now have taken `tasks_[i]`, we can do the file opening
    // work safely.
    tasks_[i].request->OpenFile(&is_cancelled_);

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
  return std::min(num_unopened_files(), max_threads_);
}

}  // namespace safe_browsing
