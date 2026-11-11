// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_RUNNER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_RUNNER_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"

namespace history {

class HistoryBackend;
struct HistoryDatabaseParams;

// HistoryBackendRunner manages the backend SequencedTaskRunner and the
// initialization lifecycle of HistoryBackend.
//
// In particular, it supports deferring HistoryBackend initialization to avoid
// resource contention during startup, queueing tasks posted to GetTaskRunner()
// until EnsureInitScheduled() schedules HistoryBackend::Init.
class HistoryBackendRunner {
 public:
  HistoryBackendRunner(scoped_refptr<base::SequencedTaskRunner> task_runner,
                       scoped_refptr<HistoryBackend> backend,
                       bool no_db,
                       const HistoryDatabaseParams& history_database_params,
                       bool defer_init);

  HistoryBackendRunner(const HistoryBackendRunner&) = delete;
  HistoryBackendRunner& operator=(const HistoryBackendRunner&) = delete;

  ~HistoryBackendRunner();

  // Schedules HistoryBackend::Init if it has not already been scheduled, and
  // flushes any tasks queued on GetTaskRunner().
  void EnsureInitScheduled();

  // Returns a task runner that executes tasks on `task_runner_` after
  // EnsureInitScheduled() has been called. When `defer_init` is true, tasks
  // posted before EnsureInitScheduled() are queued in FIFO order without
  // scheduling HistoryBackend::Init immediately.
  base::SequencedTaskRunner* GetTaskRunner();

  // Returns the underlying task runner without queueing behind Init.
  // This is used for tasks that do not require the backend database to be
  // initialized (e.g. updating in-memory sync/device metadata, shutdown).
  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  // Returns true if HistoryBackend::Init has already been scheduled.
  bool is_init_scheduled() const { return init_scheduled_; }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::DeferredSequencedTaskRunner> deferred_task_runner_;
  scoped_refptr<HistoryBackend> backend_;
  bool init_scheduled_ = false;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_RUNNER_H_
