// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/file_flusher.h"

#include <algorithm>
#include <set>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// FileFlusher::Job

class FileFlusher::Job {
 public:
  Job(const base::FilePath& path,
      bool recursive,
      const FileFlusher::OnFlushCallback& on_flush_callback,
      const base::WeakPtr<FileFlusher>& flusher,
      base::OnceClosure callback);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job() = default;

  void Start();
  void Cancel();

  const base::FilePath& path() const { return path_; }
  bool started() const { return started_; }

 private:
  // Flush files on a blocking pool thread.
  void FlushAsync();

  // Schedule a FinishOnUIThread task to run on the original sequence.
  void ScheduleFinish();

  // Finish the job by notifying |flusher_| and self destruct on the original
  // sequence.
  void FinishOnUIThread();

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath path_;
  const bool recursive_;
  const FileFlusher::OnFlushCallback on_flush_callback_;

  // Followings can be accessed only on the original sequence.
  base::WeakPtr<FileFlusher> flusher_;
  base::OnceClosure callback_;
  bool started_ = false;
  bool finish_scheduled_ = false;

  // Can be accessed on both the original sequence, or thread pool.
  base::AtomicFlag cancel_flag_;
};

FileFlusher::Job::Job(const base::FilePath& path,
                      bool recursive,
                      const FileFlusher::OnFlushCallback& on_flush_callback,
                      const base::WeakPtr<FileFlusher>& flusher,
                      base::OnceClosure callback)
    : path_(path),
      recursive_(recursive),
      on_flush_callback_(on_flush_callback),
      flusher_(flusher),
      callback_(std::move(callback)) {}

void FileFlusher::Job::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!started());

  started_ = true;

  if (cancel_flag_.IsSet()) {
    ScheduleFinish();
    return;
  }

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FileFlusher::Job::FlushAsync, base::Unretained(this)),
      base::BindOnce(&FileFlusher::Job::FinishOnUIThread,
                     base::Unretained(this)));
}

void FileFlusher::Job::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cancel_flag_.Set();

  // Cancel() could be called in an iterator/range loop in |flusher_| thus don't
  // invoke FinishOnUIThread in-place.
  if (!started()) {
    ScheduleFinish();
  }
}

void FileFlusher::Job::FlushAsync() {
  VLOG(1) << "Flushing files under " << path_.value();

  base::FileEnumerator traversal(path_, recursive_,
                                 base::FileEnumerator::FILES);
  for (base::FilePath current = traversal.Next();
       !current.empty() && !cancel_flag_.IsSet(); current = traversal.Next()) {
    base::File currentFile(current,
                           base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    if (!currentFile.IsValid()) {
      VLOG(1) << "Unable to flush file:" << current.value();
      continue;
    }

    currentFile.Flush();
    currentFile.Close();

    if (!on_flush_callback_.is_null()) {
      on_flush_callback_.Run(current);
    }
  }
}

void FileFlusher::Job::ScheduleFinish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (finish_scheduled_) {
    return;
  }

  finish_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Job::FinishOnUIThread, base::Unretained(this)));
}

void FileFlusher::Job::FinishOnUIThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!callback_.is_null()) {
    std::move(callback_).Run();
  }

  if (flusher_) {
    flusher_->OnJobDone(this);
  }

  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// FileFlusher

FileFlusher::FileFlusher() = default;

FileFlusher::~FileFlusher() {
  for (ash::FileFlusher::Job* job : jobs_) {
    job->Cancel();
  }
}

void FileFlusher::RequestFlush(const base::FilePath& path,
                               bool recursive,
                               base::OnceClosure callback) {
  for (ash::FileFlusher::Job* job : jobs_) {
    if (path == job->path() || path.IsParent(job->path())) {
      job->Cancel();
    }
  }

  jobs_.push_back(new Job(path, recursive, on_flush_callback_for_test_,
                          weak_factory_.GetWeakPtr(), std::move(callback)));
  ScheduleJob();
}

void FileFlusher::PauseForTest() {
  DCHECK(base::ranges::none_of(jobs_,
                               [](const Job* job) { return job->started(); }));
  paused_for_test_ = true;
}

void FileFlusher::ResumeForTest() {
  paused_for_test_ = false;
  ScheduleJob();
}

void FileFlusher::ScheduleJob() {
  if (jobs_.empty() || paused_for_test_) {
    return;
  }

  auto* job = jobs_.front().get();
  if (!job->started()) {
    job->Start();
  }
}

void FileFlusher::OnJobDone(FileFlusher::Job* job) {
  for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
    if (*it == job) {
      jobs_.erase(it);
      break;
    }
  }

  ScheduleJob();
}

}  // namespace ash
