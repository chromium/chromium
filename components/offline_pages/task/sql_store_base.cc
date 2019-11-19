// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/task/sql_store_base.h"

#include <iterator>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"

namespace offline_pages {
namespace {

bool PrepareDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_OK;
  if (!base::DirectoryExists(path.DirName())) {
    if (!base::CreateDirectoryAndGetError(path.DirName(), &error)) {
      DLOG(ERROR) << "Failed to create prefetch db directory: "
                  << base::File::ErrorToString(error);
      return false;
    }
  }
  return true;
}

// TODO(fgorski): This function and this part of the system in general could
// benefit from a better status code reportable through UMA to better capture
// the reason for failure, aiding the process of repeated attempts to
// open/initialize the database.
bool InitializeSync(
    sql::Database* db,
    const base::FilePath& path,
    const std::string& histogram_tag,
    base::OnceCallback<bool(sql::Database*)> initialize_schema) {
  // These values are default.
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag(histogram_tag);
  db->set_exclusive_locking();
  const bool in_memory = path.empty();
  if (!in_memory && !PrepareDirectory(path))
    return false;

  bool open_db_result = false;
  if (in_memory)
    open_db_result = db->OpenInMemory();
  else
    open_db_result = db->Open(path);

  if (!open_db_result) {
    DLOG(ERROR) << "Failed to open database, in memory: " << in_memory;
    return false;
  }
  db->Preload();

  return std::move(initialize_schema).Run(db);
}

void CloseDatabaseSync(
    sql::Database* db,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner,
    base::OnceClosure callback) {
  if (db)
    db->Close();
  callback_runner->PostTask(FROM_HERE, std::move(callback));
}

}  // namespace

// static
constexpr base::TimeDelta SqlStoreBase::kClosingDelay;

SqlStoreBase::SqlStoreBase(
    const std::string& histogram_tag,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& file_path)
    : background_task_runner_(background_task_runner),
      histogram_tag_(histogram_tag),
      db_file_path_(file_path),
      db_(nullptr, base::OnTaskRunnerDeleter(background_task_runner_)) {}

SqlStoreBase::~SqlStoreBase() = default;

void SqlStoreBase::SetInitializationStatusForTesting(
    InitializationStatus initialization_status,
    bool reset_db) {
  initialization_status_ = initialization_status;
  if (reset_db)
    db_.reset(nullptr);
}

void SqlStoreBase::Initialize(base::OnceClosure pending_command) {
  OnOpenStart(last_closing_time_);

  DCHECK_EQ(initialization_status_, InitializationStatus::kNotInitialized);
  initialization_status_ = InitializationStatus::kInProgress;

  // This is how we reset a pointer and provide deleter. This is necessary to
  // ensure that we can close the store more than once.
  db_ = DatabaseUniquePtr(new sql::Database,
                          base::OnTaskRunnerDeleter(background_task_runner_));

  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&InitializeSync, db_.get(), db_file_path_, histogram_tag_,
                     GetSchemaInitializationFunction()),
      base::BindOnce(&SqlStoreBase::InitializeDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_command)));
}

void SqlStoreBase::InitializeDone(base::OnceClosure pending_command,
                                  bool success) {
  DCHECK_EQ(initialization_status_, InitializationStatus::kInProgress);
  if (success) {
    initialization_status_ = InitializationStatus::kSuccess;
  } else {
    initialization_status_ = InitializationStatus::kFailure;
    db_.reset();
  }

  CHECK(!pending_command.is_null());
  std::move(pending_command).Run();
  for (auto command_iter = std::make_move_iterator(pending_commands_.begin());
       command_iter != std::make_move_iterator(pending_commands_.end());
       ++command_iter) {
    (*command_iter).Run();
  }
  pending_commands_.clear();

  // Once pending commands are empty, we get back to kNotInitialized state, to
  // make it possible to retry initialization next time a DB operation is
  // attempted.
  if (initialization_status_ == InitializationStatus::kFailure)
    initialization_status_ = InitializationStatus::kNotInitialized;

  OnOpenDone(success);
}

void SqlStoreBase::ExecuteInternal(base::OnceClosure command) {
  if (initialization_status_ == InitializationStatus::kInProgress) {
    pending_commands_.push_back(std::move(command));
    return;
  }

  if (initialization_status_ == InitializationStatus::kNotInitialized) {
    Initialize(std::move(command));
    return;
  }

  std::move(command).Run();
}

sql::Database* SqlStoreBase::ExecuteBegin() {
  OnTaskBegin(initialization_status_ == InitializationStatus::kSuccess);
  // Ensure that any scheduled close operations are canceled.
  closing_weak_ptr_factory_.InvalidateWeakPtrs();

  return initialization_status_ == InitializationStatus::kSuccess ? db_.get()
                                                                  : nullptr;
}

void SqlStoreBase::CloseInternal() {
  OnCloseStart(initialization_status_);

  last_closing_time_ = base::TimeTicks::Now();

  initialization_status_ = InitializationStatus::kNotInitialized;
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CloseDatabaseSync, db_.get(), base::ThreadTaskRunnerHandle::Get(),
          base::BindOnce(&SqlStoreBase::CloseInternalDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(db_))));
}

void SqlStoreBase::RescheduleClosingBefore() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SqlStoreBase::CloseInternal,
                     closing_weak_ptr_factory_.GetWeakPtr()),
      kClosingDelay);

  // Note: the time recorded for this trace step will include thread hop wait
  // times to the background thread and back.
  OnTaskRunComplete();
}

void SqlStoreBase::CloseInternalDone(DatabaseUniquePtr db) {
  db.reset();
  OnCloseComplete();
}

}  // namespace offline_pages
