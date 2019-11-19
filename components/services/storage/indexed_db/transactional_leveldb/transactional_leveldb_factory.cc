// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"

#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"

namespace content {

std::unique_ptr<TransactionalLevelDBDatabase>
DefaultTransactionalLevelDBFactory::CreateLevelDBDatabase(
    scoped_refptr<LevelDBState> state,
    std::unique_ptr<LevelDBScopes> scopes,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t max_open_iterators) {
  return base::WrapUnique(new TransactionalLevelDBDatabase(
      std::move(state), std::move(scopes), this, std::move(task_runner),
      max_open_iterators));
}
std::unique_ptr<LevelDBDirectTransaction>
DefaultTransactionalLevelDBFactory::CreateLevelDBDirectTransaction(
    TransactionalLevelDBDatabase* db) {
  return base::WrapUnique(new LevelDBDirectTransaction(db));
}

scoped_refptr<TransactionalLevelDBTransaction>
DefaultTransactionalLevelDBFactory::CreateLevelDBTransaction(
    TransactionalLevelDBDatabase* db,
    std::unique_ptr<LevelDBScope> scope) {
  return base::WrapRefCounted(
      new TransactionalLevelDBTransaction(db, std::move(scope)));
}

std::unique_ptr<TransactionalLevelDBIterator>
DefaultTransactionalLevelDBFactory::CreateIterator(
    std::unique_ptr<leveldb::Iterator> it,
    base::WeakPtr<TransactionalLevelDBDatabase> db,
    base::WeakPtr<TransactionalLevelDBTransaction> txn,
    std::unique_ptr<LevelDBSnapshot> snapshot) {
  return base::WrapUnique(new TransactionalLevelDBIterator(
      std::move(it), std::move(db), std::move(txn), std::move(snapshot)));
}

}  // namespace content
