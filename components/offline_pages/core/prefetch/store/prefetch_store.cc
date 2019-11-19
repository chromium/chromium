// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store.h"

#include <utility>

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

void ReportStoreEvent(OfflinePagesStoreEvent event) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.PrefetchStore.StoreEvent", event);
}

}  // namespace

PrefetchStore::PrefetchStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : SqlStoreBase("PrefetchStore",
                   std::move(blocking_task_runner),
                   base::FilePath()) {}

PrefetchStore::PrefetchStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& path)
    : SqlStoreBase("PrefetchStore",
                   std::move(blocking_task_runner),
                   path.AppendASCII(kPrefetchStoreFileName)) {}

PrefetchStore::~PrefetchStore() = default;

base::OnceCallback<bool(sql::Database* db)>
PrefetchStore::GetSchemaInitializationFunction() {
  return base::BindOnce(&PrefetchStoreSchema::CreateOrUpgradeIfNeeded);
}

void PrefetchStore::OnOpenStart(base::TimeTicks last_closing_time) {
  TRACE_EVENT_ASYNC_BEGIN1("offline_pages", "Prefetch Store", this, "is reopen",
                           !last_closing_time.is_null());
  ReportStoreEvent(last_closing_time.is_null()
                       ? OfflinePagesStoreEvent::kOpenedFirstTime
                       : OfflinePagesStoreEvent::kReopened);
}

void PrefetchStore::OnOpenDone(bool success) {
  // TODO(carlosk): Add initializing error reporting here.
  TRACE_EVENT_ASYNC_STEP_PAST1("offline_pages", "Prefetch Store", this,
                               "Initializing", "succeeded", success);
  if (!success) {
    TRACE_EVENT_ASYNC_END0("offline_pages", "Prefetch Store", this);
  }
}

void PrefetchStore::OnTaskBegin(bool is_initialized) {
  TRACE_EVENT_ASYNC_BEGIN1("offline_pages", "Prefetch Store: task execution",
                           this, "is store loaded", is_initialized);
}

void PrefetchStore::OnTaskRunComplete() {
  // Note: the time recorded for this trace step will include thread hop wait
  // times to the background thread and back.
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages",
                               "Prefetch Store: task execution", this, "Task");
}

void PrefetchStore::OnTaskReturnComplete() {
  TRACE_EVENT_ASYNC_STEP_PAST0(
      "offline_pages", "Prefetch Store: task execution", this, "Callback");
  TRACE_EVENT_ASYNC_END0("offline_pages", "Prefetch Store: task execution",
                         this);
}

void PrefetchStore::OnCloseStart(InitializationStatus initialization_status) {
  if (initialization_status != InitializationStatus::kSuccess) {
    ReportStoreEvent(OfflinePagesStoreEvent::kCloseSkipped);
    return;
  }
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages", "Prefetch Store", this, "Open");

  ReportStoreEvent(OfflinePagesStoreEvent::kClosed);
}

void PrefetchStore::OnCloseComplete() {
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages", "Prefetch Store", this,
                               "Closing");
  TRACE_EVENT_ASYNC_END0("offline_pages", "Prefetch Store", this);
}

}  // namespace offline_pages
