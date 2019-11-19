// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_TRANSACTION_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_TRANSACTION_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope_deletion_mode.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBIterator;
class LevelDBScope;
class LevelDBWriteBatch;

// Represents a transaction on top of a TransactionalLevelDBDatabase, and is
// backed by a LevelDBScope. This class is not thread-safe.
// Isolation: Read committed
//   All changes written using this transaction are readable through the Get()
//   method and iterators returned by CreateIterator(). They are NOT invisible
//   to other readers - if a key is written to using this transaction and read
//   from in a different transaction or on the database, it might read what was
//   written here.
// Atomicity:
//   All changes in this transaction will be either fully written or fully
//   reverted. It uses the LevelDBScopes system to guarantee this. If this class
//   is destructed before Commit() is called, then it will be rolled back.
// Destruction:
//   On destruction, if the transaction is not committed, it will be rolled
//   back. In a single-sequence scopes setup, this can actually tear down the
//   whole IndexedDBOriginState! So be careful when destroying this object.
class TransactionalLevelDBTransaction
    : public base::RefCounted<TransactionalLevelDBTransaction> {
 public:
  leveldb::Status Put(const base::StringPiece& key,
                      std::string* value) WARN_UNUSED_RESULT;

  leveldb::Status Remove(const base::StringPiece& key) WARN_UNUSED_RESULT;

  leveldb::Status RemoveRange(const base::StringPiece& begin,
                              const base::StringPiece& end,
                              LevelDBScopeDeletionMode deletion_mode)
      WARN_UNUSED_RESULT;

  virtual leveldb::Status Get(const base::StringPiece& key,
                              std::string* value,
                              bool* found) WARN_UNUSED_RESULT;
  virtual leveldb::Status Commit(bool sync_on_commit) WARN_UNUSED_RESULT;

  // If the underlying scopes system is in single-sequence mode, then this
  // method will return the result of the rollback task.
  leveldb::Status Rollback() WARN_UNUSED_RESULT;

  // The returned iterator must be destroyed before the destruction of this
  // transaction.
  std::unique_ptr<TransactionalLevelDBIterator> CreateIterator();

  uint64_t GetTransactionSize() const;

  // Sets a callback that will be called after the undo log for this transaction
  // is cleaned up and any deferred deletions (from RemoveRange) are complete.
  // The callback will be called after this transaction is committed, or dropped
  // (destructed) if it is rolled back. The transaction may not be alive when
  // this callback is called.
  void set_commit_cleanup_complete_callback(base::OnceClosure callback) {
    DCHECK(commit_cleanup_complete_callback_.is_null());
    commit_cleanup_complete_callback_ = std::move(callback);
  }

  // Forces the underlying scope to write all pending changes to disk & generate
  // an undo log.
  leveldb::Status ForceWriteChangesAndUndoLog();

 protected:
  friend class DefaultTransactionalLevelDBFactory;
  friend class TransactionalLevelDBTransactionTest;

  TransactionalLevelDBTransaction(TransactionalLevelDBDatabase* db,
                                  std::unique_ptr<LevelDBScope> scope);
  virtual ~TransactionalLevelDBTransaction();

 private:
  friend class base::RefCounted<TransactionalLevelDBTransaction>;
  friend class TransactionalLevelDBIterator;

  // These methods are called from TransactionalLevelDBIterator.
  void OnIteratorLoaded(TransactionalLevelDBIterator* iterator);
  void OnIteratorEvicted(TransactionalLevelDBIterator* iterator);
  void OnIteratorDestroyed(TransactionalLevelDBIterator* iterator);

  void EvictLoadedIterators();

  TransactionalLevelDBDatabase* const db_;
  // Non-null until the transaction is committed or rolled back.
  std::unique_ptr<LevelDBScope> scope_;
  bool finished_ = false;
  base::OnceClosure commit_cleanup_complete_callback_;

  // These sets contain all iterators created directly through this
  // transaction's |CreateIterator|. We need to track iterators when they're
  // loaded, mark them evicted when the data they cover changes, and remove them
  // when they are destructed.
  //
  // Implementing this could be done with a single list of iterators. However
  // that has the downside that, when data changes, we must iterating over all
  // iterators, many of which will likely already have been evicted.
  //
  // Since we only need to iterate over the loaded iterators on data changes, we
  // can speed up the data change iteration by storing loaded iterators
  // separately. Here that's implemented by storing loaded and evicted iterators
  // in separate sets.
  //
  // Raw pointers are safe here because the destructor of LevelDBIterator
  // removes itself from its associated transaction. It is performant to have
  // |loaded_iterators_| as a flat_set, as the iterator pooling feature of
  // TransactionalLevelDBDatabase ensures a maximum number of
  // TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase loaded
  // iterators.
  base::flat_set<TransactionalLevelDBIterator*> loaded_iterators_;
  std::set<TransactionalLevelDBIterator*> evicted_iterators_;
  bool is_evicting_all_loaded_iterators_ = false;

  base::WeakPtrFactory<TransactionalLevelDBTransaction> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TransactionalLevelDBTransaction);
};

// Reads go straight to the database, ignoring any writes cached in
// write_batch_. Writes are accumulated in a leveldb::WriteBatch and written on
// |Commit()|.
// TODO(dmurph): Remove this and have users just use the database and a
// WriteBatch.
class LevelDBDirectTransaction {
 public:
  virtual ~LevelDBDirectTransaction();

  leveldb::Status Put(const base::StringPiece& key, const std::string* value);
  virtual leveldb::Status Get(const base::StringPiece& key,
                              std::string* value,
                              bool* found);
  void Remove(const base::StringPiece& key);
  leveldb::Status Commit();

  TransactionalLevelDBDatabase* db() { return db_; }

 protected:
  friend class DefaultTransactionalLevelDBFactory;

  explicit LevelDBDirectTransaction(TransactionalLevelDBDatabase* db);

  bool IsFinished() const { return write_batch_ == nullptr; }

  TransactionalLevelDBDatabase* const db_;
  std::unique_ptr<LevelDBWriteBatch> write_batch_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBDirectTransaction);
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_TRANSACTION_H_
