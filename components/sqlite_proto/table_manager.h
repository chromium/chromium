// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_PROTO_TABLE_MANAGER_H_
#define COMPONENTS_SQLITE_PROTO_TABLE_MANAGER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"

namespace base {
class Location;
class SequencedTaskRunner;
}  // namespace base

namespace sql {
class Database;
}

namespace sqlite_proto {

// Base class encapsulating database operation scheduling and management, scoped
// to a collection of tables (possibly, but not necessarily, all of the
// database's tables).
//
// Refcounted as it is created and destroyed in the main thread (e.g., the UI
// thread in the browser process) but all database related functions need to
// happen in the database sequence. The task runner for this sequence is
// provided by the client to the constructor of this class.
class TableManager : public base::RefCountedThreadSafe<TableManager> {
 public:
  // Returns a SequencedTaskRunner that is used to run tasks on the DB sequence.
  base::SequencedTaskRunner* GetTaskRunner();

  TableManager(const TableManager&) = delete;
  TableManager& operator=(const TableManager&) = delete;

  virtual void ScheduleDBTask(const base::Location& from_here,
                              base::OnceCallback<void(sql::Database*)> task);

  virtual void ExecuteDBTaskOnDBSequence(
      base::OnceCallback<void(sql::Database*)> task);

 protected:
  explicit TableManager(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);
  virtual ~TableManager();

  // DB sequence functions.
  virtual void CreateTablesIfNonExistent() = 0;
  virtual void LogDatabaseStats() = 0;
  void Initialize(sql::Database* db);
  void SetCancelled();
  bool IsCancelled();
  sql::Database* DB();
  void ResetDB();

  bool CantAccessDatabase();

 private:
  base::AtomicFlag cancelled_;

  friend class base::RefCountedThreadSafe<TableManager>;

  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  sql::Database* db_;
};

}  // namespace sqlite_proto

#endif  // COMPONENTS_SQLITE_PROTO_TABLE_MANAGER_H_
