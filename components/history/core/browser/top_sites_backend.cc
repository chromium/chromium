// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_backend.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/top_sites_database.h"
#include "sql/database.h"

namespace history {

TopSitesBackend::TopSitesBackend()
    : db_(new TopSitesDatabase()),
      db_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
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

void TopSitesBackend::UpdateTopSites(const TopSitesDelta& delta) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TopSitesBackend::UpdateTopSitesOnDBThread, this, delta));
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
  return db_ ? db_->GetSites() : MostVisitedURLList();
}

void TopSitesBackend::UpdateTopSitesOnDBThread(const TopSitesDelta& delta) {
  TRACE_EVENT0("startup", "history::TopSitesBackend::UpdateTopSitesOnDBThread");

  if (!db_)
    return;

  db_->ApplyDelta(delta);
}

void TopSitesBackend::ResetDatabaseOnDBThread(const base::FilePath& file_path) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_.reset(nullptr);
  sql::Database::Delete(db_path_);
  db_ = std::make_unique<TopSitesDatabase>();
  InitDBOnDBThread(db_path_);
}

}  // namespace history
