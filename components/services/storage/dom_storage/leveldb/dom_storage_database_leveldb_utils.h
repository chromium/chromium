// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_DATABASE_LEVELDB_UTILS_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_DATABASE_LEVELDB_UTILS_H_

#include <memory>

#include "base/containers/span.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "storage/common/database/db_status.h"
#include "storage/common/database/leveldb_status_helper.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace storage {

// IOError message returned whenever a call is made on a
// DomStorageDatabaseLevelDB which has been invalidated (e.g. by a failed
// |RewriteDB()| operation).
const char kInvalidDatabaseMessage[] =
    "DomStorageDatabaseLevelDB no longer valid.";

inline leveldb::Slice MakeSlice(base::span<const uint8_t> data) {
  if (data.empty()) {
    return leveldb::Slice();
  }
  return leveldb::Slice(reinterpret_cast<const char*>(data.data()),
                        data.size());
}

template <typename Func>
DbStatus ForEachWithPrefix(leveldb::DB* db,
                           DomStorageDatabase::KeyView prefix,
                           Func function) {
  std::unique_ptr<leveldb::Iterator> iter(
      db->NewIterator(leveldb::ReadOptions()));
  const leveldb::Slice prefix_slice(MakeSlice(prefix));
  iter->Seek(prefix_slice);
  for (; iter->Valid(); iter->Next()) {
    if (!iter->key().starts_with(prefix_slice)) {
      break;
    }
    function(iter->key(), iter->value());
  }
  return FromLevelDBStatus(iter->status());
}

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_DATABASE_LEVELDB_UTILS_H_
