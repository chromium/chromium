// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"

#include <inttypes.h>
#include <stdint.h>

#include <algorithm>
#include <cerrno>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/transactional_leveldb/leveldb_write_batch.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

using leveldb_env::DBTracker;

namespace content::indexed_db {

namespace {

// As `TransactionLevelDBDatabase` is only used for internal transactions such
// as DB initialization and via `LevelDBDirectTransaction`, this constant
// doesn't apply to web API IndexedDB "readwrite" transactions.
const bool kSyncWrites = true;

}  // namespace

LevelDBSnapshot::LevelDBSnapshot(TransactionalLevelDBDatabase* db)
    : db_(db->db()), snapshot_(db_->GetSnapshot()) {}

LevelDBSnapshot::~LevelDBSnapshot() {
  db_->ReleaseSnapshot(snapshot_.ExtractAsDangling());
}

// static
constexpr const size_t
    TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase;

TransactionalLevelDBDatabase::TransactionalLevelDBDatabase(
    scoped_refptr<LevelDBState> level_db_state,
    std::unique_ptr<LevelDBScopes> leveldb_scopes,
    TransactionalLevelDBFactory* class_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t max_open_iterators)
    : level_db_state_(std::move(level_db_state)),
      scopes_(std::move(leveldb_scopes)),
      class_factory_(class_factory),
      clock_(new base::DefaultClock()),
      iterator_lru_(max_open_iterators) {
  if (task_runner) {
    base::trace_event::MemoryDumpManager::GetInstance()
        ->RegisterDumpProviderWithSequencedTaskRunner(
            this, "IndexedDBBackingStore", std::move(task_runner),
            base::trace_event::MemoryDumpProvider::Options());
  }
  DCHECK(max_open_iterators);
}

TransactionalLevelDBDatabase::~TransactionalLevelDBDatabase() {
  LOCAL_HISTOGRAM_COUNTS_10000("Storage.IndexedDB.LevelDB.MaxIterators",
                               max_iterators_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

leveldb::Status TransactionalLevelDBDatabase::Put(std::string_view key,
                                                  std::string* value) {
  leveldb::WriteOptions write_options;
  write_options.sync = kSyncWrites;

  const leveldb::Status s =
      db()->Put(write_options, leveldb_env::MakeSlice(key),
                leveldb_env::MakeSlice(*value));
  EvictAllIterators();
  return s;
}

leveldb::Status TransactionalLevelDBDatabase::Remove(std::string_view key) {
  leveldb::WriteOptions write_options;
  write_options.sync = kSyncWrites;

  const leveldb::Status s =
      db()->Delete(write_options, leveldb_env::MakeSlice(key));

  EvictAllIterators();
  return s;
}

leveldb::Status TransactionalLevelDBDatabase::Get(std::string_view key,
                                                  std::string* value,
                                                  bool* found) {
  *found = false;
  leveldb::ReadOptions read_options = DefaultReadOptions();

  const leveldb::Status s =
      db()->Get(read_options, leveldb_env::MakeSlice(key), value);
  if (s.ok()) {
    *found = true;
    return s;
  }
  if (s.IsNotFound()) [[likely]] {
    return leveldb::Status::OK();
  }
  return s;
}

leveldb::Status TransactionalLevelDBDatabase::Write(
    LevelDBWriteBatch* write_batch) {
  DCHECK(write_batch);
  base::TimeTicks begin_time = base::TimeTicks::Now();
  leveldb::WriteOptions write_options;
  write_options.sync = kSyncWrites;

  const leveldb::Status s =
      db()->Write(write_options, write_batch->write_batch_.get());
  UMA_HISTOGRAM_TIMES("WebCore.IndexedDB.LevelDB.WriteTime",
                      base::TimeTicks::Now() - begin_time);
  EvictAllIterators();
  return s;
}

std::unique_ptr<TransactionalLevelDBIterator>
TransactionalLevelDBDatabase::CreateIterator(leveldb::ReadOptions options) {
  DCHECK(!options.snapshot);
  num_iterators_++;
  max_iterators_ = std::max(max_iterators_, num_iterators_);
  std::unique_ptr<LevelDBSnapshot> snapshot =
      std::make_unique<LevelDBSnapshot>(this);
  options.snapshot = snapshot->snapshot();
  // Iterator isn't added to |iterator_lru_| until it is used, as memory isn't
  // loaded for the iterator until its first Seek call.
  std::unique_ptr<leveldb::Iterator> i(db()->NewIterator(options));
  auto it = class_factory_->CreateIterator(
      std::move(i), weak_factory_for_iterators_.GetWeakPtr(),
      base::WeakPtr<TransactionalLevelDBTransaction>(), std::move(snapshot));
  db_only_loaded_iterators_.insert(it.get());
  return it;
}

std::unique_ptr<TransactionalLevelDBIterator>
TransactionalLevelDBDatabase::CreateIterator(
    base::WeakPtr<TransactionalLevelDBTransaction> txn,
    leveldb::ReadOptions options) {
  DCHECK(!options.snapshot);
  // Note - this iterator is NOT added to |db_only_*_iterators_|, as it is
  // associated with a transaction, and will be in that transaction's
  // iterator lists. The implementation assumes that the iterator lives in
  // either the database list or the transaction list, and not both.
  num_iterators_++;
  max_iterators_ = std::max(max_iterators_, num_iterators_);
  std::unique_ptr<LevelDBSnapshot> snapshot =
      std::make_unique<LevelDBSnapshot>(this);
  options.snapshot = snapshot->snapshot();
  // Iterator isn't added to |iterator_lru_| until it is used, as memory isn't
  // loaded for the iterator until its first Seek call.
  std::unique_ptr<leveldb::Iterator> i(db()->NewIterator(options));
  return class_factory_->CreateIterator(
      std::move(i), weak_factory_for_iterators_.GetWeakPtr(), std::move(txn),
      std::move(snapshot));
}

void TransactionalLevelDBDatabase::Compact(std::string_view start,
                                           std::string_view stop) {
  TRACE_EVENT0("leveldb", "LevelDBDatabase::Compact");
  const leveldb::Slice start_slice = leveldb_env::MakeSlice(start);
  const leveldb::Slice stop_slice = leveldb_env::MakeSlice(stop);
  // nullptr batch means just wait for earlier writes to be done
  db()->Write(leveldb::WriteOptions(), nullptr);
  db()->CompactRange(&start_slice, &stop_slice);
}

void TransactionalLevelDBDatabase::CompactAll() {
  db()->CompactRange(nullptr, nullptr);
}

leveldb::ReadOptions TransactionalLevelDBDatabase::DefaultReadOptions() {
  leveldb::ReadOptions read_options;
  // Always verify checksums on leveldb blocks for IndexedDB databases, which
  // detects corruptions.
  read_options.verify_checksums = true;
  read_options.snapshot = nullptr;
  return read_options;
}

bool TransactionalLevelDBDatabase::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (!level_db_state_)
    return false;
  // All leveldb databases are already dumped by leveldb_env::DBTracker. Add
  // an edge to the existing database.
  auto* db_tracker_dump =
      leveldb_env::DBTracker::GetOrCreateAllocatorDump(pmd, db());
  if (!db_tracker_dump)
    return true;

  auto* db_dump = pmd->CreateAllocatorDump(
      base::StringPrintf("site_storage/index_db/db_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(db())));
  db_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                     base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                     db_tracker_dump->GetSizeInternal());
  pmd->AddOwnershipEdge(db_dump->guid(), db_tracker_dump->guid());

  if (env() && leveldb_chrome::IsMemEnv(env())) {
    // All leveldb env's are already dumped by leveldb_env::DBTracker. Add
    // an edge to the existing env.
    auto* env_tracker_dump = DBTracker::GetOrCreateAllocatorDump(pmd, env());
    auto* env_dump = pmd->CreateAllocatorDump(
        base::StringPrintf("site_storage/index_db/memenv_0x%" PRIXPTR,
                           reinterpret_cast<uintptr_t>(env())));
    env_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        env_tracker_dump->GetSizeInternal());
    pmd->AddOwnershipEdge(env_dump->guid(), env_tracker_dump->guid());
  }

  // Dumps in BACKGROUND mode can only have whitelisted strings (and there are
  // currently none) so return early.
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
    return true;
  }

  db_dump->AddString("file_name", "", level_db_state_->name_for_tracing());

  return true;
}

void TransactionalLevelDBDatabase::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

TransactionalLevelDBDatabase::DetachIteratorOnDestruct::
    DetachIteratorOnDestruct(TransactionalLevelDBIterator* it)
    : it(it) {}
TransactionalLevelDBDatabase::DetachIteratorOnDestruct::
    DetachIteratorOnDestruct(DetachIteratorOnDestruct&& that) {
  it = that.it;
  that.it = nullptr;
}
TransactionalLevelDBDatabase::DetachIteratorOnDestruct::
    ~DetachIteratorOnDestruct() {
  if (it)
    it->EvictLevelDBIterator();
}

void TransactionalLevelDBDatabase::EvictAllIterators() {
  if (db_only_loaded_iterators_.empty())
    return;
  is_evicting_all_loaded_iterators_ = true;
  base::flat_set<raw_ptr<TransactionalLevelDBIterator, CtnExperimental>>
      to_be_evicted = std::move(db_only_loaded_iterators_);
  for (TransactionalLevelDBIterator* iter : to_be_evicted) {
    iter->EvictLevelDBIterator();
  }
  is_evicting_all_loaded_iterators_ = false;
}

void TransactionalLevelDBDatabase::OnIteratorUsed(
    TransactionalLevelDBIterator* iter) {
  // This line updates the LRU if the item exists.
  if (iterator_lru_.Get(iter) != iterator_lru_.end())
    return;
  DetachIteratorOnDestruct purger(iter);
  iterator_lru_.Put(iter, std::move(purger));
}

void TransactionalLevelDBDatabase::OnIteratorLoaded(
    TransactionalLevelDBIterator* iterator) {
  DCHECK(db_only_evicted_iterators_.find(iterator) !=
         db_only_evicted_iterators_.end());
  db_only_loaded_iterators_.insert(iterator);
  db_only_evicted_iterators_.erase(iterator);
}

void TransactionalLevelDBDatabase::OnIteratorEvicted(
    TransactionalLevelDBIterator* iterator) {
  DCHECK(db_only_loaded_iterators_.find(iterator) !=
             db_only_loaded_iterators_.end() ||
         is_evicting_all_loaded_iterators_);
  db_only_loaded_iterators_.erase(iterator);
  db_only_evicted_iterators_.insert(iterator);
}

void TransactionalLevelDBDatabase::OnIteratorDestroyed(
    TransactionalLevelDBIterator* iter) {
  DCHECK_GT(num_iterators_, 0u);
  db_only_loaded_iterators_.erase(iter);
  db_only_evicted_iterators_.erase(iter);
  --num_iterators_;
  auto lru_iterator = iterator_lru_.Peek(iter);
  if (lru_iterator == iterator_lru_.end())
    return;
  // Set the |lru_iterator|'s stored iterator to |nullptr| to avoid it
  // unnecessarily calling EvictLevelDBIterator.
  lru_iterator->second.it = nullptr;
  iterator_lru_.Erase(lru_iterator);
}

}  // namespace content::indexed_db
