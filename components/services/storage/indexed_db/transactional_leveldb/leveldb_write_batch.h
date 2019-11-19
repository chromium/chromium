// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_LEVELDB_WRITE_BATCH_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_LEVELDB_WRITE_BATCH_H_

#include <memory>

#include "base/strings/string_piece.h"

namespace leveldb {
class WriteBatch;
}

namespace content {

// Wrapper around leveldb::WriteBatch.
// This class holds a collection of updates to apply atomically to a database.
// TODO(dmurph): Remove this and just use a leveldb::WriteBatch.
class LevelDBWriteBatch {
 public:
  static std::unique_ptr<LevelDBWriteBatch> Create();
  ~LevelDBWriteBatch();

  void Put(const base::StringPiece& key, const base::StringPiece& value);
  void Remove(const base::StringPiece& key);
  void Clear();

 private:
  friend class TransactionalLevelDBDatabase;
  LevelDBWriteBatch();

  std::unique_ptr<leveldb::WriteBatch> write_batch_;
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_TRANSACTIONAL_LEVELDB_LEVELDB_WRITE_BATCH_H_
