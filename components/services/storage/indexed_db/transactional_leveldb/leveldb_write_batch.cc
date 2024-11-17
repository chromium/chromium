// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/transactional_leveldb/leveldb_write_batch.h"

#include "base/memory/ptr_util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace content::indexed_db {

std::unique_ptr<LevelDBWriteBatch> LevelDBWriteBatch::Create() {
  return base::WrapUnique(new LevelDBWriteBatch);
}

LevelDBWriteBatch::LevelDBWriteBatch()
    : write_batch_(new leveldb::WriteBatch) {}

LevelDBWriteBatch::~LevelDBWriteBatch() = default;

void LevelDBWriteBatch::Put(std::string_view key, std::string_view value) {
  write_batch_->Put(leveldb_env::MakeSlice(key), leveldb_env::MakeSlice(value));
}

void LevelDBWriteBatch::Remove(std::string_view key) {
  write_batch_->Delete(leveldb_env::MakeSlice(key));
}

void LevelDBWriteBatch::Clear() {
  write_batch_->Clear();
}

}  // namespace content::indexed_db
