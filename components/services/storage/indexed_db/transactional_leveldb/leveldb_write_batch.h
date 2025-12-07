// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_LEVELDB_WRITE_BATCH_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_LEVELDB_WRITE_BATCH_H_

#include <memory>
#include <string_view>

namespace leveldb {
class WriteBatch;
}

namespace content::indexed_db {

// Wrapper around leveldb::WriteBatch.
// This class holds a collection of updates to apply atomically to a database.
// TODO(dmurph): Remove this and just use a leveldb::WriteBatch.
class LevelDBWriteBatch {
 public:
  static std::unique_ptr<LevelDBWriteBatch> Create();
  ~LevelDBWriteBatch();

  void Put(std::string_view key, std::string_view value);
  void Remove(std::string_view key);
  void Clear();

 private:
  friend class TransactionalLevelDBDatabase;
  LevelDBWriteBatch();

  std::unique_ptr<leveldb::WriteBatch> write_batch_;
};

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_LEVELDB_WRITE_BATCH_H_
