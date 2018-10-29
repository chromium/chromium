// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_schema.h"
#include "sql/database.h"

namespace offline_pages {
namespace {

const char kPrefetchStoreFileName[] = "PrefetchStore.db";

bool PrepareDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_OK;
  if (!base::DirectoryExists(path.DirName())) {
    if (!base::CreateDirectoryAndGetError(path.DirName(), &error)) {
      LOG(ERROR) << "Failed to create prefetch db directory: "
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
bool InitializeSync(sql::Database* db,
                    const base::FilePath& path,
                    bool in_memory) {
  // These values are default.
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("PrefetchStore");
  db->set_exclusive_locking();

  if (!in_memory && !PrepareDirectory(path))
    return false;

  bool open_db_result = false;
  if (in_memory)
    open_db_result = db->OpenInMemory();
  else
    open_db_result = db->Open(path);

  if (!open_db_result) {
    LOG(ERROR) << "Failed to open database, in memory: " << in_memory;
    return false;
  }
  db->Preload();

  return PrefetchStoreSchema::CreateOrUpgradeIfNeeded(db);
}

void CloseDatabaseSync(
    sql::Database* db,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner,
    base::OnceClosure callback) {
  if (db)
    db->Close();
  callback_runner->PostTask(FROM_HERE, std::move(callback));
}

void ReportStoreEvent(OfflinePagesStoreEvent event) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.PrefetchStore.StoreEvent", event);
}

}  // namespace

// static
constexpr base::TimeDelta PrefetchStore::kClosingDelay;

PrefetchStore::PrefetchStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : blocking_task_runner_(std::move(blocking_task_runner)),
      in_memory_(true),
      db_(nullptr, base::OnTaskRunnerDeleter(blocking_task_runner_)),
      initialization_status_(InitializationStatus::NOT_INITIALIZED),
      weak_ptr_factory_(this),
      closing_weak_ptr_factory_(this) {}

PrefetchStore::PrefetchStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& path)
    : blocking_task_runner_(std::move(blocking_task_runner)),
      db_file_path_(path.AppendASCII(kPrefetchStoreFileName)),
      in_memory_(false),
      db_(nullptr, base::OnTaskRunnerDeleter(blocking_task_runner_)),
      initialization_status_(InitializationStatus::NOT_INITIALIZED),
      weak_ptr_factory_(this),
      closing_weak_ptr_factory_(this) {}

PrefetchStore::~PrefetchStore() {}

void PrefetchStore::Initialize(base::OnceClosure pending_command) {
  TRACE_EVENT_ASYNC_BEGIN1("offline_pages", "Prefetch Store", this, "is reopen",
                           !last_closing_time_.is_null());
  DCHECK_EQ(initialization_status_, InitializationStatus::NOT_INITIALIZED);
  initialization_status_ = InitializationStatus::INITIALIZING;

  if (!last_closing_time_.is_null()) {
    ReportStoreEvent(OfflinePagesStoreEvent::kReopened);
    UMA_HISTOGRAM_CUSTOM_TIMES("OfflinePages.PrefetchStore.TimeFromCloseToOpen",
                               base::Time::Now() - last_closing_time_,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromMinutes(10),
                               50 /* buckets */);
  } else {
    ReportStoreEvent(OfflinePagesStoreEvent::kOpenedFirstTime);
  }

  // This is how we reset a pointer and provide deleter. This is necessary to
  // ensure that we can close the store more than once.
  db_ = DatabaseUniquePtr(new sql::Database,
                          base::OnTaskRunnerDeleter(blocking_task_runner_));

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&InitializeSync, db_.get(), db_file_path_, in_memory_),
      base::BindOnce(&PrefetchStore::OnInitializeDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_command)));
}

void PrefetchStore::OnInitializeDone(base::OnceClosure pending_command,
                                     bool success) {
  // TODO(carlosk): Add initializing error reporting here.
  TRACE_EVENT_ASYNC_STEP_PAST1("offline_pages", "Prefetch Store", this,
                               "Initializing", "succeeded", success);
  DCHECK_EQ(initialization_status_, InitializationStatus::INITIALIZING);
  if (success) {
    initialization_status_ = InitializationStatus::SUCCESS;
  } else {
    initialization_status_ = InitializationStatus::FAILURE;
    db_.reset();
    TRACE_EVENT_ASYNC_END0("offline_pages", "Prefetch Store", this);
  }

  CHECK(!pending_command.is_null());
  std::move(pending_command).Run();

  // Once pending commands are empty, we get back to NOT_INITIALIZED state, to
  // make it possible to retry initialization next time a DB operation is
  // attempted.
  if (initialization_status_ == InitializationStatus::FAILURE)
    initialization_status_ = InitializationStatus::NOT_INITIALIZED;
}

void PrefetchStore::CloseInternal() {
  if (initialization_status_ != InitializationStatus::SUCCESS) {
    ReportStoreEvent(OfflinePagesStoreEvent::kCloseSkipped);
    return;
  }
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages", "Prefetch Store", this, "Open");

  last_closing_time_ = base::Time::Now();
  ReportStoreEvent(OfflinePagesStoreEvent::kClosed);

  initialization_status_ = InitializationStatus::NOT_INITIALIZED;
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CloseDatabaseSync, db_.get(), base::ThreadTaskRunnerHandle::Get(),
          base::BindOnce(&PrefetchStore::CloseInternalDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(db_))));
}

void PrefetchStore::CloseInternalDone(DatabaseUniquePtr db) {
  db.reset();
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages", "Prefetch Store", this,
                               "Closing");
  TRACE_EVENT_ASYNC_END0("offline_pages", "Prefetch Store", this);
}

}  // namespace offline_pages
