// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_H_

#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class Comparator;
class Iterator;
}  // namespace leveldb

namespace content::indexed_db {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBTransaction;
class LevelDBSnapshot;

// This iterator is meant to stay 'live' to the data on disk for a given
// transaction, and be evict-able for saving memory. Specifically, it supports:
// * Staying up to date with the data on disk as long as the NotifyModified
//   method is called for every change, and
// * detaching itself (unloading it's leveldb::Iterator) to reduce memory when
//   EvictLevelDBIterator is called.
// Note: the returned std::string_view from Key or Value can become
// invalidated when EvictLevelDBIterator, OnDatabaseKeyModified, or
// OnDatabaseRangeModified are called.
class TransactionalLevelDBIterator {
 public:
  TransactionalLevelDBIterator(const TransactionalLevelDBIterator&) = delete;
  TransactionalLevelDBIterator& operator=(const TransactionalLevelDBIterator&) =
      delete;

  virtual ~TransactionalLevelDBIterator();

  virtual bool IsValid() const;
  virtual leveldb::Status SeekToLast();
  virtual leveldb::Status Seek(std::string_view target);
  virtual leveldb::Status Next();
  virtual leveldb::Status Prev();
  // The returned std::string_view can be invalidated when
  // EvictLevelDBIterator is called.
  virtual std::string_view Key() const;
  // The returned std::string_view can be invalidated when
  // EvictLevelDBIterator is called.
  virtual std::string_view Value() const;

  // Evicts the internal leveldb::Iterator, which helps save memory at the
  // performance expense of reloading and seeking later if the iterator is
  // needed again for |Seek*|, |Next|, or |Prev|.
  void EvictLevelDBIterator();
  bool IsEvicted() const { return iterator_state_ != IteratorState::kActive; }

 protected:
  friend class DefaultTransactionalLevelDBFactory;

  TransactionalLevelDBIterator(
      std::unique_ptr<leveldb::Iterator> iterator,
      base::WeakPtr<TransactionalLevelDBDatabase> db,
      base::WeakPtr<TransactionalLevelDBTransaction> txn,
      std::unique_ptr<LevelDBSnapshot> snapshot);

 private:
  enum class Direction { kNext, kPrev };
  enum class IteratorState { kActive, kEvictedAndValid, kEvictedAndInvalid };

  [[nodiscard]] leveldb::Status WrappedIteratorStatus();

  // Notifies the database of iterator usage and recreates iterator if needed.
  // If the iterator was previously evicted, this method returns the key that
  // was used, the status of reloading the iterator.
  [[nodiscard]] std::tuple<std::string, leveldb::Status> WillUseDBIterator(
      bool perform_seek);

  // If this method fails, then iterator_ will be nullptr.
  [[nodiscard]] leveldb::Status ReloadIterator();

  void NextPastScopesMetadata();
  void PrevPastScopesMetadata();

  void CheckState() const;

  // WeakPtr to allow lazy destruction order. This is assumed to be valid for
  // all other Iterator operations.
  base::WeakPtr<TransactionalLevelDBDatabase> db_;
  const raw_ref<const std::vector<uint8_t>> scopes_metadata_prefix_;
  base::WeakPtr<TransactionalLevelDBTransaction> txn_;

  // State used to facilitate memory purging.
  IteratorState iterator_state_ = IteratorState::kActive;
  // Non-null iff |iterator_state_| is kActive.
  std::unique_ptr<leveldb::Iterator> iterator_;
  // Empty if |iterator_state_| is kActive.
  std::string key_before_eviction_;
  // Empty if |iterator_state_| is kActive.
  std::string value_before_eviction_;
  // Non-null iff |iterator_state_| is kActive.
  std::unique_ptr<LevelDBSnapshot> snapshot_;

  const raw_ptr<const leveldb::Comparator> comparator_;
};

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_H_
