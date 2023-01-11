// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_proto/table_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "sql/database.h"

namespace sqlite_proto {

using DBTask = base::OnceCallback<void(sql::Database*)>;

base::SequencedTaskRunner* TableManager::GetTaskRunner() {
  return db_task_runner_.get();
}

void TableManager::ScheduleDBTask(const base::Location& from_here,
                                  DBTask task) {
  GetTaskRunner()->PostTask(
      from_here, base::BindOnce(&TableManager::ExecuteDBTaskOnDBSequence, this,
                                std::move(task)));
}

void TableManager::ScheduleDBTaskWithReply(const base::Location& from_here,
                                           DBTask task,
                                           base::OnceClosure reply) {
  GetTaskRunner()->PostTaskAndReply(
      from_here,
      base::BindOnce(&TableManager::ExecuteDBTaskOnDBSequence, this,
                     std::move(task)),
      std::move(reply));
}

void TableManager::ExecuteDBTaskOnDBSequence(DBTask task) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  std::move(task).Run(DB());
}

TableManager::TableManager(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : db_task_runner_(std::move(db_task_runner)), db_(nullptr) {}

TableManager::~TableManager() = default;

void TableManager::Initialize(sql::Database* db) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_ = db;
  CreateOrClearTablesIfNecessary();
}

void TableManager::SetCancelled() {
  cancelled_.Set();
}

bool TableManager::IsCancelled() {
  return cancelled_.IsSet();
}

sql::Database* TableManager::DB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return db_;
}

void TableManager::ResetDB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  db_ = nullptr;
}

bool TableManager::CantAccessDatabase() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return cancelled_.IsSet() || !db_;
}

}  // namespace sqlite_proto
