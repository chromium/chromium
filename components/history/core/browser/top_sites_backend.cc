// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_backend.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/top_sites_database.h"
#include "sql/database.h"

namespace history {

TopSitesBackend::TopSitesBackend()
    : db_(new TopSitesDatabase()),
      db_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()})) {
  DCHECK(db_task_runner_);
}

void TopSitesBackend::Init(const base::FilePath& path) {
  db_path_ = path;
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TopSitesBackend::InitDBOnDBThread, this, path));
}

void TopSitesBackend::Shutdown() {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TopSitesBackend::ShutdownDBOnDBThread, this));
}

void TopSitesBackend::GetMostVisitedSites(
    GetMostVisitedSitesCallback callback,
    base::CancelableTaskTracker* tracker) {
  tracker->PostTaskAndReplyWithResult(
      db_task_runner_.get(), FROM_HERE,
      base::BindOnce(&TopSitesBackend::GetMostVisitedSitesOnDBThread, this),
      std::move(callback));
}

void TopSitesBackend::UpdateTopSites(const TopSitesDelta& delta,
                                     const RecordHistogram record_or_not) {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TopSitesBackend::UpdateTopSitesOnDBThread,
                                this, delta, record_or_not));
}

void TopSitesBackend::ResetDatabase() {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TopSitesBackend::ResetDatabaseOnDBThread, this,
                                db_path_));
}

TopSitesBackend::~TopSitesBackend() {
  DCHECK(!db_);  // Shutdown should have happened first (which results in
                 // nulling out db).
}

void TopSitesBackend::InitDBOnDBThread(const base::FilePath& path) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!db_->Init(path)) {
    LOG(ERROR) << "Failed to initialize database.";
    db_.reset();
  }
}

void TopSitesBackend::ShutdownDBOnDBThread() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_.reset();
}

MostVisitedURLList TopSitesBackend::GetMostVisitedSitesOnDBThread() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  MostVisitedURLList list;
  if (db_)
    db_->GetSites(&list);
  return list;
}

void TopSitesBackend::UpdateTopSitesOnDBThread(
    const TopSitesDelta& delta, const RecordHistogram record_or_not) {
  TRACE_EVENT0("startup", "history::TopSitesBackend::UpdateTopSitesOnDBThread");

  if (!db_)
    return;

  base::TimeTicks begin_time = base::TimeTicks::Now();

  db_->ApplyDelta(delta);

  if (record_or_not == RECORD_HISTOGRAM_YES) {
    UMA_HISTOGRAM_TIMES("History.FirstUpdateTime",
                        base::TimeTicks::Now() - begin_time);
  }
}

void TopSitesBackend::ResetDatabaseOnDBThread(const base::FilePath& file_path) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_.reset(nullptr);
  sql::Database::Delete(db_path_);
  db_.reset(new TopSitesDatabase());
  InitDBOnDBThread(db_path_);
}

}  // namespace history
