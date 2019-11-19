// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"

#include <utility>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"

namespace content {

TransactionalLevelDBIterator::TransactionalLevelDBIterator(
    std::unique_ptr<leveldb::Iterator> it,
    base::WeakPtr<TransactionalLevelDBDatabase> db,
    base::WeakPtr<TransactionalLevelDBTransaction> txn,
    std::unique_ptr<LevelDBSnapshot> snapshot)
    : db_(std::move(db)),
      scopes_metadata_prefix_(db_->scopes()->metadata_key_prefix()),
      txn_(std::move(txn)),
      iterator_(std::move(it)),
      snapshot_(std::move(snapshot)),
      comparator_(db_->leveldb_state()->comparator()) {
  DCHECK(db_);
}

TransactionalLevelDBIterator::~TransactionalLevelDBIterator() {
  CheckState();

  // db_->OnIteratorDestroyed below can cause reentry into EvictLevelDBIterator,
  // so preemptively clear this state.
  snapshot_.reset();
  iterator_.reset();
  iterator_state_ = IteratorState::kEvictedAndInvalid;

  // Sometimes destruction order can destroy the database before this iterator.
  // This is fine.
  if (db_)
    db_->OnIteratorDestroyed(this);
  if (txn_)
    txn_->OnIteratorDestroyed(this);
}

leveldb::Status TransactionalLevelDBIterator::WrappedIteratorStatus() {
  DCHECK(db_);
  DCHECK(!IsEvicted());
  CheckState();

  const leveldb::Status& s = iterator_->status();
  if (!s.ok())
    LOG(ERROR) << "LevelDB iterator error: " << s.ToString();
  return s;
}

bool TransactionalLevelDBIterator::IsValid() const {
  DCHECK(db_);
  CheckState();

  switch (iterator_state_) {
    case IteratorState::kEvictedAndValid:
      return true;
      break;
    case IteratorState::kEvictedAndInvalid:
      return false;
      break;
    case IteratorState::kActive:
      return iterator_->Valid();
  }
  NOTREACHED();
}

leveldb::Status TransactionalLevelDBIterator::SeekToLast() {
  DCHECK(db_);
  CheckState();

  leveldb::Status s;
  std::tie(std::ignore, s) = WillUseDBIterator(/*perform_seek=*/false);
  if (!s.ok())
    return s;
  DCHECK(iterator_);
  iterator_->SeekToLast();
  PrevPastScopesMetadata();
  return WrappedIteratorStatus();
}

leveldb::Status TransactionalLevelDBIterator::Seek(
    const base::StringPiece& target) {
  DCHECK(db_);
  CheckState();

  leveldb::Slice target_slice = leveldb_env::MakeSlice(target);
  leveldb::Status s;
  std::tie(std::ignore, s) = WillUseDBIterator(/*perform_seek=*/false);
  if (!s.ok())
    return s;
  DCHECK(iterator_);
  iterator_->Seek(target_slice);
  NextPastScopesMetadata();
  return WrappedIteratorStatus();
}

leveldb::Status TransactionalLevelDBIterator::Next() {
  DCHECK(db_);
  DCHECK(IsValid());
  CheckState();

  bool iterator_is_loaded = (iterator_ != nullptr);
  std::string key_before_eviction;
  leveldb::Status s;
  std::tie(key_before_eviction, s) = WillUseDBIterator(/*perform_seek=*/true);
  if (!s.ok())
    return s;
  DCHECK(iterator_);

  // Exit early if not valid.
  if (!IsValid())
    return WrappedIteratorStatus();

  // Don't call Next if the key that was Seek'ed to was deleted.
  if (iterator_is_loaded ||
      comparator_->Compare(iterator_->key(), key_before_eviction) == 0) {
    iterator_->Next();
  }
  NextPastScopesMetadata();
  return WrappedIteratorStatus();
}

leveldb::Status TransactionalLevelDBIterator::Prev() {
  DCHECK(db_);
  DCHECK(IsValid());
  CheckState();

  leveldb::Status s;
  std::tie(std::ignore, s) = WillUseDBIterator(/*perform_seek=*/true);
  DCHECK(iterator_);
  if (!s.ok())
    return s;

  // If invalid, that means the current key has been deleted AND it was at the
  // end of the database. In this case, seeking to the last item is the same as
  // 'Prev'-ing from the deleted item.
  if (!IsValid())
    iterator_->SeekToLast();
  else
    iterator_->Prev();

  PrevPastScopesMetadata();
  return WrappedIteratorStatus();
}

base::StringPiece TransactionalLevelDBIterator::Key() const {
  DCHECK(db_);
  DCHECK(IsValid());
  CheckState();

  if (IsEvicted())
    return key_before_eviction_;
  return leveldb_env::MakeStringPiece(iterator_->key());
}

base::StringPiece TransactionalLevelDBIterator::Value() const {
  DCHECK(db_);
  DCHECK(IsValid());
  CheckState();

  // Fetching the value should update the LRU. Const-cast needed, as this is
  // implementing a caching layer.
  TransactionalLevelDBIterator* non_const =
      const_cast<TransactionalLevelDBIterator*>(this);
  db_->OnIteratorUsed(non_const);
  if (IsEvicted())
    return value_before_eviction_;
  return leveldb_env::MakeStringPiece(iterator_->value());
}

void TransactionalLevelDBIterator::EvictLevelDBIterator() {
  DCHECK(db_);
  CheckState();

  if (IsEvicted())
    return;
  if (iterator_->Valid()) {
    iterator_state_ = IteratorState::kEvictedAndValid;
    key_before_eviction_ = iterator_->key().ToString();
    value_before_eviction_ = iterator_->value().ToString();
  } else {
    iterator_state_ = IteratorState::kEvictedAndInvalid;
  }
  snapshot_.reset();
  iterator_.reset();
  if (txn_)
    txn_->OnIteratorEvicted(this);
  else
    db_->OnIteratorEvicted(this);
}

std::tuple<std::string, leveldb::Status>
TransactionalLevelDBIterator::WillUseDBIterator(bool perform_seek) {
  DCHECK(db_);
  CheckState();

  leveldb::Status s;
  db_->OnIteratorUsed(this);
  if (!IsEvicted())
    return {"", s};

  s = ReloadIterator();
  if (!s.ok())
    return {"", s};

  if (iterator_state_ == IteratorState::kEvictedAndValid && perform_seek)
    iterator_->Seek(key_before_eviction_);

  iterator_state_ = IteratorState::kActive;
  value_before_eviction_.clear();
  return {std::move(key_before_eviction_), s};
}

leveldb::Status TransactionalLevelDBIterator::ReloadIterator() {
  DCHECK(db_);
  DCHECK(iterator_state_ != IteratorState::kActive);
  CheckState();

  if (txn_) {
    leveldb::Status s = txn_->ForceWriteChangesAndUndoLog();
    if (!s.ok())
      return s;
    txn_->OnIteratorLoaded(this);
  } else {
    db_->OnIteratorLoaded(this);
  }
  snapshot_ = std::make_unique<LevelDBSnapshot>(db_.get());
  leveldb::ReadOptions read_options = db_->DefaultReadOptions();
  read_options.snapshot = snapshot_->snapshot();
  iterator_ = base::WrapUnique(db_->db()->NewIterator(read_options));
  return leveldb::Status::OK();
}

void TransactionalLevelDBIterator::NextPastScopesMetadata() {
  DCHECK(db_);
  DCHECK(iterator_);
  auto prefix_slice = leveldb::Slice(
      reinterpret_cast<const char*>(scopes_metadata_prefix_.data()),
      scopes_metadata_prefix_.size());
  while (iterator_->Valid() && iterator_->key().starts_with(prefix_slice)) {
    iterator_->Next();
  }
}

void TransactionalLevelDBIterator::PrevPastScopesMetadata() {
  DCHECK(db_);
  DCHECK(iterator_);
  auto prefix_slice = leveldb::Slice(
      reinterpret_cast<const char*>(scopes_metadata_prefix_.data()),
      scopes_metadata_prefix_.size());
  while (iterator_->Valid() && iterator_->key().starts_with(prefix_slice)) {
    iterator_->Prev();
  }
}

void TransactionalLevelDBIterator::CheckState() const {
#if DCHECK_IS_ON()
  DCHECK_EQ(iterator_state_ == IteratorState::kActive, iterator_ != nullptr);
  DCHECK_EQ(iterator_state_ == IteratorState::kActive, snapshot_ != nullptr);
  DCHECK(iterator_state_ != IteratorState::kActive ||
         key_before_eviction_.empty());
  DCHECK(iterator_state_ != IteratorState::kActive ||
         value_before_eviction_.empty());
#endif
}

}  // namespace content
