// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_TASK_SQL_STORE_BASE_H_
#define COMPONENTS_OFFLINE_PAGES_TASK_SQL_STORE_BASE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "sql/database.h"

namespace offline_pages {

// Maintains an SQLite database and permits safe access.
// Opens the database only when queried. Automatically closes the database
// if it's not being used.
// This is a base class, and must be overridden to configure the database
// schema.
class SqlStoreBase {
 public:
  enum class InitializationStatus {
    kNotInitialized,
    kInProgress,
    kSuccess,
    kFailure,
  };

  // Definition of the callback that is going to run the core of the command in
  // the |Execute| method.
  template <typename T>
  using RunCallback = base::OnceCallback<T(sql::Database*)>;

  // Definition of the callback used to pass the result back to the caller of
  // |Execute| method.
  template <typename T>
  using ResultCallback = base::OnceCallback<void(T)>;

  // Defines inactivity time of DB after which it is going to be closed.
  // TODO(crbug.com/933369): Derive appropriate value in a scientific way.
  static constexpr base::TimeDelta kClosingDelay =
      base::TimeDelta::FromSeconds(20);

  // If |file_path| is empty, this constructs an in-memory database.
  SqlStoreBase(const std::string& histogram_tag,
               scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
               const base::FilePath& file_path);
  virtual ~SqlStoreBase();

  // Gets the initialization status of the store.
  InitializationStatus initialization_status_for_testing() const {
    return initialization_status_;
  }

  // Executes a |run_callback| on SQL store on the blocking sequence, and posts
  // its result back to calling thread through |result_callback|.
  // Calling |Execute| when store is kNotInitialized will cause the store
  // initialization to start.
  // Store initialization status needs to be kSuccess for run_callback to run.
  // If initialization fails, |result_callback| is invoked with |default_value|.
  template <typename T>
  void Execute(RunCallback<T> run_callback,
               ResultCallback<T> result_callback,
               T default_value) {
    ExecuteInternal(
        base::BindOnce(&SqlStoreBase::ExecuteAfterInitialized<T>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(run_callback),
                       std::move(result_callback), std::move(default_value)));
  }

  void SetInitializationStatusForTesting(
      InitializationStatus initialization_status,
      bool reset_db);

 protected:
  // Returns a function that installs the latest schema to |db| (if necessary),
  // and returns whether the database was successfully verified to have the
  // current schema. |GetSchemaInitializationFunction| is called on the main
  // thread, but the returned function is executed on the blocking sequence.
  virtual base::OnceCallback<bool(sql::Database* db)>
  GetSchemaInitializationFunction() = 0;

  // Optional tracing methods. The derived class may implement tracing events
  // with these methods.

  // Called on attempt to open the database. |last_closing_time| is the time
  // since the last time the database was closed, or null if the database was
  // not previously opened since creation of this.
  virtual void OnOpenStart(base::TimeTicks last_closing_time) {}
  // Called when done attempting to open the database.
  virtual void OnOpenDone(bool success) {}
  // Called before a task is executed through |Execute()|.
  virtual void OnTaskBegin(bool is_initialized) {}
  // Called after calling the |run_callback| in |Execute()|.
  virtual void OnTaskRunComplete() {}
  // Called after calling the |result_callback| in |Execute()|.
  virtual void OnTaskReturnComplete() {}
  // Called when starting to close the database.
  virtual void OnCloseStart(InitializationStatus status_before_close) {}
  // Called after closing the database.
  virtual void OnCloseComplete() {}

 private:
  using DatabaseUniquePtr =
      std::unique_ptr<sql::Database, base::OnTaskRunnerDeleter>;

  void Initialize(base::OnceClosure pending_command);
  void InitializeDone(base::OnceClosure pending_command, bool success);

  void ExecuteInternal(base::OnceClosure command);
  sql::Database* ExecuteBegin();

  template <typename T>
  void ExecuteAfterInitialized(RunCallback<T> run_callback,
                               ResultCallback<T> result_callback,
                               T default_value) {
    DCHECK_NE(initialization_status_, InitializationStatus::kNotInitialized);
    DCHECK_NE(initialization_status_, InitializationStatus::kInProgress);
    sql::Database* db = ExecuteBegin();
    if (!db) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(result_callback), std::move(default_value)));
      return;
    }
    base::PostTaskAndReplyWithResult(
        background_task_runner_.get(), FROM_HERE,
        base::BindOnce(std::move(run_callback), db),
        base::BindOnce(&SqlStoreBase::RescheduleClosing<T>,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(result_callback)));
  }

  // Reschedules the closing with a delay. Ensures that |result_callback| is
  // called.
  template <typename T>
  void RescheduleClosing(ResultCallback<T> result_callback, T result) {
    RescheduleClosingBefore();
    std::move(result_callback).Run(std::move(result));
    OnTaskReturnComplete();
  }

  void RescheduleClosingBefore();

  // Internal function initiating the closing.
  void CloseInternal();

  // Completes the closing. Main purpose is to destroy the db pointer.
  void CloseInternalDone(DatabaseUniquePtr db);

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Histogram tag for the sqlite database.
  std::string histogram_tag_;

  // Path to the database on disk. If empty, the database is in memory only.
  base::FilePath db_file_path_;

  // Database connection.
  DatabaseUniquePtr db_;

  // Pending commands.
  std::vector<base::OnceClosure> pending_commands_;

  // State of initialization.
  InitializationStatus initialization_status_ =
      InitializationStatus::kNotInitialized;

  // Time of the last time the store was closed. Kept for metrics reporting.
  base::TimeTicks last_closing_time_;

  base::WeakPtrFactory<SqlStoreBase> weak_ptr_factory_{this};
  base::WeakPtrFactory<SqlStoreBase> closing_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SqlStoreBase);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_TASK_SQL_STORE_BASE_H_
