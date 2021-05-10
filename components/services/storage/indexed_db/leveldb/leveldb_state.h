// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_LEVELDB_STATE_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_LEVELDB_STATE_H_

#include <atomic>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace content {

// Encapsulates a leveldb database and comparator, allowing them to be used
// safely across thread boundaries.
class LevelDBState : public base::RefCountedThreadSafe<LevelDBState> {
 public:
  static scoped_refptr<LevelDBState> CreateForDiskDB(
      const leveldb::Comparator* comparator,
      std::unique_ptr<leveldb::DB> database,
      base::FilePath database_path);

  static scoped_refptr<LevelDBState> CreateForInMemoryDB(
      std::unique_ptr<leveldb::Env> in_memory_env,
      const leveldb::Comparator* comparator,
      std::unique_ptr<leveldb::DB> in_memory_database,
      std::string name_for_tracing);

  // Can only be called once. |signal_on_destruction| must outlive this class,
  // and will be signaled on the destruction of this state (in the destructor).
  // Can be called on any thread.
  void RequestDestruction(base::WaitableEvent* signal_on_destruction);

  bool destruction_requested() const {
    return destruction_requested_.load(std::memory_order_relaxed);
  }
  // Only valid if destruction_requested() returns true.
  base::WaitableEvent* destruction_event() const {
    return signal_on_destruction_;
  }

  const leveldb::Comparator* comparator() const { return comparator_; }
  leveldb::DB* db() const { return db_.get(); }
  const std::string& name_for_tracing() const { return name_for_tracing_; }

  // Null for on-disk databases.
  leveldb::Env* in_memory_env() const { return in_memory_env_.get(); }
  // Empty for in-memory databases.
  const base::FilePath& database_path() const { return database_path_; }

 private:
  friend class base::RefCountedThreadSafe<LevelDBState>;

  LevelDBState(std::unique_ptr<leveldb::Env> optional_in_memory_env,
               const leveldb::Comparator* comparator,
               std::unique_ptr<leveldb::DB> database,
               base::FilePath database_path,
               std::string name_for_tracing);
  ~LevelDBState();

  const std::unique_ptr<leveldb::Env> in_memory_env_;
  const leveldb::Comparator* comparator_;
  const std::unique_ptr<leveldb::DB> db_;
  const base::FilePath database_path_;
  const std::string name_for_tracing_;

  // This member transitions from false to true at most once in the instance's
  // lifetime.
  std::atomic_bool destruction_requested_;
  // |signal_on_destruction_| is written only once (when
  // |destruction_requested_| transitions from false to true) and read only once
  // in the destructor, so it is thread-compatible.
  base::WaitableEvent* signal_on_destruction_ = nullptr;
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LEVELDB_LEVELDB_STATE_H_
