// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_H_

#include <memory>
#include <tuple>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class Comparator;
class Iterator;
}  // namespace leveldb

namespace content {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBTransaction;
class LevelDBSnapshot;

// This iterator is meant to stay 'live' to the data on disk for a given
// transaction, and be evict-able for saving memory. Specifically, it supports:
// * Staying up to date with the data on disk as long as the NotifyModified
//   method is called for every change, and
// * detaching itself (unloading it's leveldb::Iterator) to reduce memory when
//   EvictLevelDBIterator is called.
// Note: the returned base::StringPiece from Key or Value can become
// invalidated when EvictLevelDBIterator, OnDatabaseKeyModified, or
// OnDatabaseRangeModified are called.
class TransactionalLevelDBIterator {
 public:
  virtual ~TransactionalLevelDBIterator();

  virtual bool IsValid() const;
  virtual leveldb::Status SeekToLast();
  virtual leveldb::Status Seek(const base::StringPiece& target);
  virtual leveldb::Status Next();
  virtual leveldb::Status Prev();
  // The returned base::StringPiece can be invalidated when
  // EvictLevelDBIterator is called.
  virtual base::StringPiece Key() const;
  // The returned base::StringPiece can be invalidated when
  // EvictLevelDBIterator is called.
  virtual base::StringPiece Value() const;

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

  leveldb::Status WrappedIteratorStatus() WARN_UNUSED_RESULT;

  // Notifies the database of iterator usage and recreates iterator if needed.
  // If the iterator was previously evicted, this method returns the key that
  // was used, the status of reloading the iterator.
  std::tuple<std::string, leveldb::Status> WillUseDBIterator(bool perform_seek)
      WARN_UNUSED_RESULT;

  // If this method fails, then iterator_ will be nullptr.
  leveldb::Status ReloadIterator() WARN_UNUSED_RESULT;

  void NextPastScopesMetadata();
  void PrevPastScopesMetadata();

  void CheckState() const;

  // WeakPtr to allow lazy destruction order. This is assumed to be valid for
  // all other Iterator operations.
  base::WeakPtr<TransactionalLevelDBDatabase> db_;
  const std::vector<uint8_t>& scopes_metadata_prefix_;
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

  const leveldb::Comparator* const comparator_;

  DISALLOW_COPY_AND_ASSIGN(TransactionalLevelDBIterator);
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_TRANSACTIONAL_LEVELDB_ITERATOR_H_
