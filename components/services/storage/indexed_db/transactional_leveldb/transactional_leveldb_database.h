// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_DATABASE_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_state.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class DB;
class Env;
class Snapshot;
}  // namespace leveldb

namespace content {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBFactory;
class TransactionalLevelDBIterator;
class TransactionalLevelDBTransaction;
class LevelDBScopes;
class LevelDBWriteBatch;

// This class manages the acquisition and release of a leveldb snapshot.
class LevelDBSnapshot {
 public:
  explicit LevelDBSnapshot(TransactionalLevelDBDatabase* db);
  ~LevelDBSnapshot();

  const leveldb::Snapshot* snapshot() const { return snapshot_; }

 private:
  leveldb::DB* db_;
  const leveldb::Snapshot* snapshot_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBSnapshot);
};

class TransactionalLevelDBDatabase
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Necessary because every iterator hangs onto leveldb blocks which can be
  // large. See https://crbug/696055.
  static const size_t kDefaultMaxOpenIteratorsPerDatabase = 50;

  ~TransactionalLevelDBDatabase() override;

  leveldb::Status Put(const base::StringPiece& key, std::string* value);
  leveldb::Status Remove(const base::StringPiece& key);
  virtual leveldb::Status Get(const base::StringPiece& key,
                              std::string* value,
                              bool* found);
  virtual leveldb::Status Write(LevelDBWriteBatch* write_batch);

  // This iterator will stay up-to-date with changes made to this database
  // object (as in, automatically reload when values are modified), but not
  // from any transactions.
  // LevelDBIterator must not outlive the LevelDBDatabase.
  // Note: Use DefaultReadOptions() and then adjust any values afterwards.
  std::unique_ptr<TransactionalLevelDBIterator> CreateIterator(
      leveldb::ReadOptions options);

  void Compact(const base::StringPiece& start, const base::StringPiece& stop);
  void CompactAll();

  leveldb::ReadOptions DefaultReadOptions();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  LevelDBState* leveldb_state() { return level_db_state_.get(); }
  leveldb::DB* db() { return level_db_state_->db(); }
  leveldb::Env* env() { return level_db_state_->in_memory_env(); }
  LevelDBScopes* scopes() { return scopes_.get(); }
  base::Time LastModified() const { return last_modified_; }

  TransactionalLevelDBFactory* class_factory() const { return class_factory_; }

  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

 private:
  friend class DefaultTransactionalLevelDBFactory;
  friend class LevelDBSnapshot;
  friend class TransactionalLevelDBIterator;
  friend class TransactionalLevelDBTransaction;
  friend class LevelDBTestDatabase;
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, DeleteFailsIfDirectoryLocked);
  class IteratorNotifier;

  // |max_open_cursors| cannot be 0.
  // All calls to this class should be done on |task_runner|.
  TransactionalLevelDBDatabase(
      scoped_refptr<LevelDBState> level_db_state,
      std::unique_ptr<LevelDBScopes> leveldb_scopes,
      TransactionalLevelDBFactory* factory,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      size_t max_open_iterators);

  void EvictAllIterators();

  void OnIteratorUsed(TransactionalLevelDBIterator* iterator);
  void OnIteratorLoaded(TransactionalLevelDBIterator* iterator);
  void OnIteratorEvicted(TransactionalLevelDBIterator* iterator);
  void OnIteratorDestroyed(TransactionalLevelDBIterator* iterator);

  void CloseDatabase();

  // This iterator will stay up-to-date with changes from this |txn| (as
  // in, automatically reload when values are modified), but not any other
  // transactions or the database.
  // LevelDBIterator must not outlive the LevelDBDatabase.
  // Note: Use DefaultReadOptions() and then adjust any values afterwards.
  std::unique_ptr<TransactionalLevelDBIterator> CreateIterator(
      base::WeakPtr<TransactionalLevelDBTransaction> txn,
      leveldb::ReadOptions options);

  scoped_refptr<LevelDBState> level_db_state_;
  std::unique_ptr<LevelDBScopes> scopes_;
  TransactionalLevelDBFactory* class_factory_;
  base::Time last_modified_;
  std::unique_ptr<base::Clock> clock_;

  // Contains all iterators created by this database directly through
  // |CreateIterator| WITHOUT a transaction. Iterators created with a
  // transaction will not be added to this list. Raw pointers are safe here
  // because the destructor of TransactionalLevelDBIterator removes itself from
  // its associated database. |db_only_loaded_iterators_| have loaded
  // leveldb::Iterators, and |db_only_evicted_iterators_| have had their
  // leveldb::Iterator evicted. It is performant to have
  // |db_only_loaded_iterators_| as a flat_set, as the iterator pooling feature
  // of TransactionalLevelDBDatabase ensures a maximum number of
  // kDefaultMaxOpenIteratorsPerDatabase loaded iterators.
  base::flat_set<TransactionalLevelDBIterator*> db_only_loaded_iterators_;
  std::set<TransactionalLevelDBIterator*> db_only_evicted_iterators_;
  bool is_evicting_all_loaded_iterators_ = false;

  struct DetachIteratorOnDestruct {
    DetachIteratorOnDestruct() = default;
    explicit DetachIteratorOnDestruct(TransactionalLevelDBIterator* it);
    DetachIteratorOnDestruct(DetachIteratorOnDestruct&& that);
    ~DetachIteratorOnDestruct();

    TransactionalLevelDBIterator* it = nullptr;

    DISALLOW_COPY_AND_ASSIGN(DetachIteratorOnDestruct);
  };
  // Despite the type name, this object uses LRU eviction. Raw pointers are safe
  // here because the destructor of TransactionalLevelDBIterator removes itself
  // from its associated database.
  base::HashingMRUCache<TransactionalLevelDBIterator*, DetachIteratorOnDestruct>
      iterator_lru_;

  // Recorded for UMA reporting.
  uint32_t num_iterators_ = 0;
  uint32_t max_iterators_ = 0;

  base::WeakPtrFactory<TransactionalLevelDBDatabase>
      weak_factory_for_iterators_{this};
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_DATABASE_H_
