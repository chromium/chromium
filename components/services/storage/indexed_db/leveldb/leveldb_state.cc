// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/leveldb/leveldb_state.h"

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace content::indexed_db {

// static
scoped_refptr<LevelDBState> LevelDBState::CreateForDiskDB(
    const leveldb::Comparator* comparator,
    std::unique_ptr<leveldb::DB> database,
    base::FilePath database_path) {
  auto name_for_tracing = database_path.BaseName().AsUTF8Unsafe();
  return base::WrapRefCounted(
      new LevelDBState(nullptr, comparator, std::move(database),
                       std::move(database_path), std::move(name_for_tracing)));
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

void LevelDBState::RequestDestruction(
    base::WaitableEvent* signal_on_destruction) {
  DCHECK(signal_on_destruction);
  bool destruct_already_requested =
      destruction_requested_.exchange(true, std::memory_order_relaxed);
  CHECK(!destruct_already_requested)
      << "RequestDestruction can only be called one time.";
  DCHECK(!signal_on_destruction_);
  signal_on_destruction_ = signal_on_destruction;
}

LevelDBState::~LevelDBState() {
  if (db_) {
    base::TimeTicks begin_time = base::TimeTicks::Now();
    const_cast<std::unique_ptr<leveldb::DB>*>(&db_)->reset();
    base::UmaHistogramMediumTimes("WebCore.IndexedDB.LevelDB.CloseTime",
                                  base::TimeTicks::Now() - begin_time);
  }
  if (signal_on_destruction_)
    signal_on_destruction_->Signal();
}

}  // namespace content::indexed_db
