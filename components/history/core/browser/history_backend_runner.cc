// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_backend_runner.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database_params.h"

namespace history {

HistoryBackendRunner::HistoryBackendRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<HistoryBackend> backend,
    bool no_db,
    const HistoryDatabaseParams& history_database_params,
    bool defer_init)
    : task_runner_(std::move(task_runner)),
      deferred_task_runner_(
          defer_init ? base::MakeRefCounted<base::DeferredSequencedTaskRunner>(
                           task_runner_)
                     : nullptr),
      backend_(std::move(backend)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(task_runner_);
  CHECK(backend_);
  backend_->SetInitParams(no_db, history_database_params);
  if (!defer_init) {
    EnsureInitScheduled();
  }
}

HistoryBackendRunner::~HistoryBackendRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (deferred_task_runner_) {
    // Drop any unexecuted buffered tasks before `ReleaseSoon()` so that
    // `HistoryBackend` references bound into those closures are released while
    // `backend_` still holds a strong reference, ensuring `HistoryBackend` is
    // always destroyed on `task_runner_`.
    deferred_task_runner_->StopAndDropPendingTasks();
    deferred_task_runner_.reset();
  }
  if (backend_) {
    task_runner_->ReleaseSoon(FROM_HERE, std::move(backend_));
  }
}

void HistoryBackendRunner::EnsureInitScheduled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (init_scheduled_) {
    return;
  }
  init_scheduled_ = true;
  CHECK(backend_);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&HistoryBackend::InitWithCachedParams,
                                        std::move(backend_)));
  if (deferred_task_runner_) {
    deferred_task_runner_->Start();
  }
}

base::SequencedTaskRunner* HistoryBackendRunner::GetTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return deferred_task_runner_ ? deferred_task_runner_.get()
                               : task_runner_.get();
}

}  // namespace history
