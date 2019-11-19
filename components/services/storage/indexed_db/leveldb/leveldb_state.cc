// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/leveldb/leveldb_state.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace content {

// static
scoped_refptr<LevelDBState> LevelDBState::CreateForDiskDB(
    const leveldb::Comparator* comparator,
    std::unique_ptr<leveldb::DB> database,
    base::FilePath database_path) {
  return base::WrapRefCounted(new LevelDBState(
      nullptr, comparator, std::move(database), std::move(database_path),
      database_path.BaseName().AsUTF8Unsafe()));
}

// static
scoped_refptr<LevelDBState> LevelDBState::CreateForInMemoryDB(
    std::unique_ptr<leveldb::Env> in_memory_env,
    const leveldb::Comparator* comparator,
    std::unique_ptr<leveldb::DB> in_memory_database,
    std::string name_for_tracing) {
  return base::WrapRefCounted(new LevelDBState(
      std::move(in_memory_env), comparator, std::move(in_memory_database),
      base::FilePath(), std::move(name_for_tracing)));
}

LevelDBState::LevelDBState(std::unique_ptr<leveldb::Env> optional_in_memory_env,
                           const leveldb::Comparator* comparator,
                           std::unique_ptr<leveldb::DB> database,
                           base::FilePath database_path,
                           std::string name_for_tracing)
    : in_memory_env_(std::move(optional_in_memory_env)),
      comparator_(comparator),
      db_(std::move(database)),
      database_path_(std::move(database_path)),
      name_for_tracing_(std::move(name_for_tracing)),
      destruction_requested_(false) {}

bool LevelDBState::RequestDestruction(
    base::OnceClosure on_state_destruction,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  if (destruction_requested_.exchange(true, std::memory_order_relaxed))
    return false;

  DCHECK(!on_destruction_);
  DCHECK(!on_destruction_task_runner_);
  on_destruction_ = std::move(on_state_destruction);
  on_destruction_task_runner_ = std::move(task_runner);
  return true;
}

LevelDBState::~LevelDBState() {
  if (on_destruction_) {
    on_destruction_task_runner_->PostTask(FROM_HERE,
                                          std::move(on_destruction_));
  }
  if (!db_)
    return;
  base::TimeTicks begin_time = base::TimeTicks::Now();
  const_cast<std::unique_ptr<leveldb::DB>*>(&db_)->reset();
  base::UmaHistogramMediumTimes("WebCore.IndexedDB.LevelDB.CloseTime",
                                base::TimeTicks::Now() - begin_time);
}

}  // namespace content
